// Clear remains persistent when upstream history paging and Search reveal old rows.
#include <filesystem>
#include <string>
#include <wx/app.h>
#include <wx/button.h>
#include <wx/frame.h>
#include <wx/image.h>
#include <wx/sizer.h>
#include <wx/utils.h>
#include "BroadcastPage.h"
#include "ChatList.h"
#include "Check.h"
#include "FindStrip.h"
#include "ragtime/broadcast/BroadcastLog.h"
#include "ragtime/link/HostLink.h"

class HistoryPageApp : public wxApp { public: bool OnInit() override { return true; } };
wxIMPLEMENT_APP_NO_MAIN(HistoryPageApp);

template<class T> T* find(wxWindow* parent)
{
    for (auto* child : parent->GetChildren())
    {
        if (auto* match = dynamic_cast<T*>(child)) return match;
        if (auto* match = find<T>(child)) return match;
    }
    return nullptr;
}

int main(int argc, char** argv)
{
    using namespace ragtime;
    using broadcast::Message;
    if (!wxEntryStart(argc, argv) || !wxTheApp->CallOnInit()) return 77;
    wxInitAllImageHandlers();
    const auto dir = std::filesystem::temp_directory_path() /
                     ("ragtime-page-history-" + std::to_string(wxGetProcessId()));
    std::filesystem::create_directories(dir);
    broadcast::BroadcastLog log((dir / "broadcast.log").string());
    const uint64_t now = 1792627200000;
    log.began(now);
    for (size_t i = 0; i < broadcast::HISTORY_PAGE * 2 + 30; ++i)
    {
        Message message;
        message.id = i + 1;
        message.from = "W1AW";
        message.seq = static_cast<uint32_t>(i + 1);
        message.time = static_cast<uint32_t>(now / 1000 + i);
        message.atMs = now + i * 1000;
        message.text = "archived antenna " + std::to_string(i);
        message.state = Message::State::Heard;
        log.added(message);
    }
    log.ended(now + 2000000);
    link::HostLink link({"AG7EW", true, false});
    broadcast::BroadcastChat chat(link);
    broadcast::BroadcastChat::Settings settings;
    settings.callsign = "AG7EW";
    chat.setSettings(settings, 0);
    chat.setLog(&log, now + 3000000);
    auto* frame = new wxFrame(nullptr, wxID_ANY, "Broadcast history", wxDefaultPosition, wxSize(760, 520));
    auto* page = new BroadcastPage(frame, chat);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(page, 1, wxEXPAND);
    frame->SetSizer(layout);
    page->onClear([&] { log.cleared(); });
    page->refresh();
    frame->Show();
    auto* list = find<ChatList>(page);
    wxButton* clear = nullptr;
    for (auto* child : page->GetChildren())
        if (auto* button = dynamic_cast<wxButton*>(child); button && button->GetLabel() == "Clear") clear = button;
    CHECK(list && clear);
    if (list && clear)
    {
        CHECK(list->GetItemCount() > 0);
        wxCommandEvent click(wxEVT_BUTTON, clear->GetId());
        clear->ProcessWindowEvent(click);
        CHECK_EQ(list->GetItemCount(), (size_t)0);
        CHECK(chat.loadOlder() > 0);
        page->refresh();
        CHECK_EQ(list->GetItemCount(), (size_t)0);
        // Search is an explicit, temporary review of history; closing it restores Clear.
        page->showLine(0);
        page->findStrip()->showHit("antenna", false, true);
        CHECK(list->GetItemCount() > 0);
        CHECK_EQ(list->GetSelection(), 0);
        page->findStrip()->close();
        CHECK_EQ(list->GetItemCount(), (size_t)0);
        const auto queued = chat.send("queued after clear", "", now + 3001000);
        CHECK(!queued.empty());
        page->refresh();
        CHECK_EQ(list->GetItemCount(), queued.size());
        const auto kept = list->GetItemCount();
        clear->ProcessWindowEvent(click);
        CHECK_EQ(list->GetItemCount(), kept);
        while (chat.loadOlder() != 0) {}
        page->refresh();
        CHECK_EQ(list->GetItemCount(), kept);
    }
    chat.closeLog(now + 4000000);
    const auto reopened = log.load(SIZE_MAX);
    for (const auto& row : reopened)
        if (row.from == "W1AW") CHECK(row.cleared);
    frame->Destroy();
    wxTheApp->ProcessPendingEvents();
    wxEntryCleanup();
    std::filesystem::remove_all(dir);
    return ragtime_test::finish();
}
