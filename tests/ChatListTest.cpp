// The chat list (app/src/ChatList) keeps the newest row in view, and stays
// where the operator scrolled to read. This one needs a display: without
// one it is skipped (exit 77). Before the fix a scroll's own size event on
// GTK snapped the list back to the bottom, so scrolling up never stuck
// (Jeff, 2026-11-01).

#include <cstddef>
#include <cstdio>
#include <map>

#include <wx/app.h>
#include <wx/frame.h>
#include <wx/init.h>
#include <wx/sizer.h>
#include <wx/string.h>
#include <wx/utils.h>

#if defined(__WXGTK3__)
#include <gtk/gtk.h>
#endif

#include "ChatList.h"
#include "Check.h"

namespace
{

class TestApp : public wxApp
{
public:
    bool OnInit() override { return true; }
};

class Rows : public ChatList
{
public:
    explicit Rows(wxWindow* parent) : ChatList(parent) {}

    // A row's changed text.
    std::map<size_t, wxString> changed;

protected:
    wxString OnGetItem(size_t n) const override
    {
        // Every fifth a taller one, as a voice card or a picture is.
        auto c = changed.find(n);
        wxString row = c != changed.end() ? c->second : wxString::Format("<b>W1AW</b> line %zu of the chat, long enough to read", n);
        if (n % 5 == 2) row << "<br>a voice message<br>its strip<br>and its length";
        return row;
    }
};

// Lets GTK lay out, draw and send its size events.
void settle()
{
    for (int i = 0; i < 20; i++)
    {
        wxTheApp->ProcessPendingEvents();
        wxYield();
        wxMilliSleep(10);
    }
}

bool newestShows(const ChatList& list)
{
    return list.GetVisibleRowsEnd() >= list.GetItemCount();
}

} // namespace

int main(int argc, char** argv)
{
    wxApp::SetInstance(new TestApp);
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit())
    {
        std::printf("No display: skipped\n");
        return 77;
    }
    int result = 0;
    {
        auto* frame = new wxFrame(nullptr, wxID_ANY, "ChatList", wxDefaultPosition, wxSize(420, 300));
        auto* list = new Rows(frame);
        CHECK_EQ(list->GetItemCount(), (size_t)0);
        CHECK_EQ(list->GetVisibleRowsEnd(), (size_t)0);
#if defined(__WXGTK3__)
        // As the chat tabs have it (scrollbarBeside, Theme.h): a scroll bar
        // beside the rows, which GTK lays out anew as its thumb changes.
        if (GtkWidget* widget = static_cast<GtkWidget*>(list->GetHandle()); widget != nullptr && GTK_IS_SCROLLED_WINDOW(widget))
            gtk_scrolled_window_set_overlay_scrolling(GTK_SCROLLED_WINDOW(widget), FALSE);
#endif
        auto* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(list, 1, wxEXPAND);
        frame->SetSizer(sizer);
        frame->Show();
        settle();

        // A full list follows its newest row.
        list->showRows(60);
        settle();
        CHECK(list->following());
        CHECK(newestShows(*list));
        const size_t bottom = list->GetVisibleRowsBegin();
        CHECK(bottom > 0);

        // Scrolled up three rows, as the wheel does: it stays there.
        list->ScrollRows(-3);
        settle();
        CHECK(!list->following());
        CHECK_EQ(list->GetVisibleRowsBegin(), bottom - 3);

        // A new message or a redraw doesn't pull it back down.
        list->showRows(61);
        list->RefreshAll();
        settle();
        CHECK(!list->following());
        CHECK_EQ(list->GetVisibleRowsBegin(), bottom - 3);

        // Nor does a resize.
        frame->SetSize(wxSize(460, 280));
        settle();
        CHECK(!list->following());
        CHECK_EQ(list->GetVisibleRowsBegin(), bottom - 3);

        // Scrolled back to the bottom, it follows again.
        list->ScrollRows(100);
        settle();
        CHECK(list->following());
        list->showRows(62);
        settle();
        CHECK(newestShows(*list));
        CHECK_EQ(list->GetVisibleRowsEnd(), (size_t)62);

        // A change lays out only the rows that changed, not all on screen
        // again (each change used to build every row on screen some eight
        // times over).
        list->showRows(62);
        settle();
        size_t before = list->rowsBuilt();
        list->showRows(62);
        settle();
        CHECK_EQ(list->rowsBuilt() - before, (size_t)0);
        list->changed[list->GetVisibleRowsEnd() - 1] = "<b>W1AW</b> line now edited";
        before = list->rowsBuilt();
        list->showRows(62);
        settle();
        CHECK(list->rowsBuilt() - before >= 1);
        CHECK(list->rowsBuilt() - before <= 2);
        // A new row lays out the new one, and the rows on screen again when
        // the scroll bar moving resizes the list, as GTK does (some 24
        // builds here, against 175 before).
        before = list->rowsBuilt();
        list->showRows(63);
        settle();
        CHECK(newestShows(*list));
        CHECK(list->rowsBuilt() - before >= 1);
        CHECK(list->rowsBuilt() - before < 40);

        frame->Destroy();
        settle();
        result = ragtime_test::finish();
    }
    wxEntryCleanup();
    return result;
}
