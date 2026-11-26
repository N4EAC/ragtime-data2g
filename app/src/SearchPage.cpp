#include "SearchPage.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/combobox.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/htmllbox.h>
#include <wx/numformatter.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/srchctrl.h>
#include <wx/stattext.h>

#include "Html.h"
#include "Theme.h"
#include "ragtime/files/SessionFiles.h"

using ragtime::search::Hit;
using ragtime::search::Place;

namespace
{

wxString utf8(const std::string& s)
{
    return wxString::FromUTF8(s);
}

wxString dayAndTime(uint64_t ms)
{
    wxDateTime at((time_t)(ms / 1000));
    return at.Format("%d %b %H:%M");
}

wxString timeOnly(uint64_t ms)
{
    wxDateTime at((time_t)(ms / 1000));
    return at.Format("%H:%M");
}

wxString count(size_t n)
{
    return wxNumberFormatter::ToString((long)n);
}

// "27 min", "1 h 05 min".
wxString lasted(uint64_t fromMs, uint64_t toMs)
{
    uint64_t minutes = toMs > fromMs ? (toMs - fromMs + 30000) / 60000 : 0;
    if (minutes < 60) return wxString::Format("%d min", (int)minutes);
    return wxString::Format("%d h %02d min", (int)(minutes / 60), (int)(minutes % 60));
}

// The ragchew's place: "Ragchew with W1AW · 18 Nov 20:14–20:41".
wxString ragchewTitle(const Hit& h)
{
    wxString title = "Ragchew with " + utf8(h.peer);
    if (h.beganMs != 0)
    {
        title << wxString::FromUTF8(" \xc2\xb7 ") << dayAndTime(h.beganMs);
        if (h.endedMs > h.beganMs) title << wxString::FromUTF8("\xe2\x80\x93") << timeOnly(h.endedMs);
    }
    return title;
}

wxString fontColour(const wxColour& c, bool picked)
{
    return picked ? wxString() : " color=\"" + htmlColour(c) + "\"";
}

// A file line's sign before its name.
wxString fileSign(bool file, bool picture, bool voice)
{
    if (!file) return wxString();
    if (voice) return wxString::FromUTF8("\xe2\x96\xb6 ");          // the voice card's play
    if (picture) return wxString::FromUTF8("\xf0\x9f\x96\xbc ");    // a framed picture
    return wxString::FromUTF8("\xf0\x9f\x93\x84 ");                 // a page
}

} // namespace

//-------------------------------------------------------------------------
// The list of hits, grouped under their places' heads.

class SearchResults : public wxHtmlListBox
{
public:
    SearchResults(wxWindow* parent, SearchPage* owner) : wxHtmlListBox(parent, wxID_ANY), owner_(owner)
    {
        followThemeBackground(this, wxSYS_COLOUR_LISTBOX);
        scrollbarBeside(this);
        Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
            int row = GetSelection();
            if (row < 0 || (size_t)row >= owner_->rows_.size()) return;
            const SearchPage::Row& r = owner_->rows_[(size_t)row];
            if (r.head)
            {
                owner_->toggle((size_t)row);
                return;
            }
            owner_->picked_ = r.hit;
            owner_->showNearby();
            RefreshAll();
        });
        Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) {
            int row = GetSelection();
            if (row >= 0 && (size_t)row < owner_->rows_.size() && !owner_->rows_[(size_t)row].head)
                owner_->jumpToPicked();
        });
    }

    wxString OnGetItem(size_t n) const override
    {
        if (n >= owner_->rows_.size()) return wxString();
        const SearchPage::Row& r = owner_->rows_[n];
        const Hit& h = owner_->hits_[r.hit];
        bool picked = IsSelected(n);
        wxString html;
        if (r.head)
        {
            bool folded = std::find(owner_->folded_.begin(), owner_->folded_.end(), SearchPage::placeKey(h))
                          != owner_->folded_.end();
            wxString title = h.place == Place::Broadcast ? wxString("Broadcast") : ragchewTitle(h);
            wxString after;
            if (h.place == Place::Ragchew && h.endedMs > h.beganMs && h.beganMs != 0)
                after << wxString::FromUTF8(" \xc2\xb7 ") << lasted(h.beganMs, h.endedMs);
            html << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"3\""
                 << (picked ? wxString() : " bgcolor=\"" + htmlColour(blendColour(
                                               wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX),
                                               wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT), 0.07)) + "\"")
                 << "><tr><td width=\"16\">" << (folded ? wxString::FromUTF8("\xe2\x96\xb8") : wxString::FromUTF8("\xe2\x96\xbe"))
                 << "</td><td><b>" << htmlEscape(std::string(title.utf8_str())) << "</b><font"
                 << fontColour(secondaryText(), picked) << ">" << after << "</font></td>"
                 << "<td align=\"right\" nowrap><font" << fontColour(secondaryText(), picked) << ">" << count(r.count)
                 << "</font></td><td width=\"10\"></td></tr></table>";
            return listText(html, picked);
        }
        html << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\"><tr>"
             << "<td width=\"18\"></td>"
             << "<td valign=\"top\" nowrap width=\"110\"><font" << fontColour(secondaryText(), picked) << ">"
             << dayAndTime(h.atMs) << "</font></td>"
             << "<td valign=\"top\" nowrap width=\"90\"><b>" << htmlEscape(h.from) << "</b></td>"
             << "<td valign=\"top\">";
        if (!h.to.empty())
            html << "<font" << fontColour(accentText(), picked) << ">"
                 << SearchPage::markedHtml("@" + h.to, owner_->terms(), picked) << "</font> ";
        html << fileSign(h.file, h.picture, h.voice) << SearchPage::markedHtml(h.text, owner_->terms(), picked)
             << "</td></tr></table>";
        return listText(html, picked);
    }

private:
    SearchPage* owner_;
};

//-------------------------------------------------------------------------
// The picked hit in place: the lines either side of it.

class SearchNearby : public wxHtmlListBox
{
public:
    explicit SearchNearby(wxWindow* parent, SearchPage* owner) : wxHtmlListBox(parent, wxID_ANY), owner_(owner)
    {
        followThemeBackground(this, wxSYS_COLOUR_LISTBOX);
        scrollbarBeside(this);
        Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { owner_->jumpToPicked(); });
    }

    wxString OnGetItem(size_t n) const override
    {
        if (owner_->picked_ >= owner_->hits_.size()) return wxString();
        const Hit& h = owner_->hits_[owner_->picked_];
        if (n >= h.nearby.size()) return wxString();
        const ragtime::search::Nearby& line = h.nearby[n];
        bool hit = n == h.at;
        bool picked = IsSelected(n);
        wxString bg;
        if (hit && !picked)
            bg = " bgcolor=\"" + htmlColour(blendColour(wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX), accentText(), 0.16)) + "\"";
        wxString html;
        html << "<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\"" << bg << "><tr>"
             << "<td valign=\"top\" nowrap width=\"110\"><font" << fontColour(secondaryText(), picked) << ">"
             << dayAndTime(line.atMs) << "</font></td>"
             << "<td valign=\"top\"><b>" << htmlEscape(line.from) << "</b>&nbsp; ";
        const std::vector<std::string> none;
        const std::vector<std::string>& terms = hit ? owner_->terms() : none;
        if (!line.to.empty())
            html << "<font" << fontColour(accentText(), picked) << ">" << SearchPage::markedHtml("@" + line.to, terms, picked)
                 << "</font> ";
        html << fileSign(line.file, false, false) << SearchPage::markedHtml(line.text, terms, picked) << "</td></tr></table>";
        return listText(html, picked);
    }

private:
    SearchPage* owner_;
};

//-------------------------------------------------------------------------

SearchPage::SearchPage(wxWindow* parent) : wxPanel(parent, wxID_ANY), typing_(this), polling_(this)
{
    box_ = new wxSearchCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    box_->ShowCancelButton(true);
    box_->SetDescriptiveText("Words to find in Broadcast and the ragchews");
    inBroadcast_ = new wxCheckBox(this, wxID_ANY, "Broadcast");
    inBroadcast_->SetValue(true);
    inRagchews_ = new wxCheckBox(this, wxID_ANY, "Ragchews");
    inRagchews_->SetValue(true);
    from_ = new wxComboBox(this, wxID_ANY, wxEmptyString, wxDefaultPosition, FromDIP(wxSize(160, -1)));
    from_->SetHint("From anyone");
    from_->SetToolTip("Only lines from this station");
    when_ = new wxChoice(this, wxID_ANY);
    when_->Append("Any time");
    when_->Append("Today");
    when_->Append("Past 7 days");
    when_->Append("Past 30 days");
    when_->SetSelection(0);
    close_ = new wxButton(this, wxID_ANY, "Close", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    close_->SetToolTip("Close the Search tab (Esc)");

    auto* top = new wxBoxSizer(wxHORIZONTAL);
    top->Add(box_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    top->Add(inBroadcast_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    top->Add(inRagchews_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    top->Add(from_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(6));
    top->Add(when_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    top->Add(close_, 0, wxALIGN_CENTER_VERTICAL);

    status_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    followThemeText(status_, []() { return secondaryText(); });
    note_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    note_->Hide();

    auto* split = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxSP_LIVE_UPDATE | wxSP_3DSASH | wxSP_NO_XP_THEME);
    split->SetMinimumPaneSize(FromDIP(60));
    split->SetSashGravity(0.6);
    list_ = new SearchResults(split, this);
    auto* below = new wxPanel(split);
    where_ = new wxStaticText(below, wxID_ANY, wxEmptyString);
    where_->SetFont(where_->GetFont().Bold());
    jump_ = new wxButton(below, wxID_ANY, "Show in Broadcast");
    nearby_ = new SearchNearby(below, this);
    auto* head = new wxBoxSizer(wxHORIZONTAL);
    head->Add(where_, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(8));
    head->Add(jump_, 0, wxALIGN_CENTER_VERTICAL);
    auto* belowSizer = new wxBoxSizer(wxVERTICAL);
    belowSizer->Add(head, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(4));
    belowSizer->Add(nearby_, 1, wxEXPAND);
    below->SetSizer(belowSizer);
    split->SplitHorizontally(list_, below, -FromDIP(210));

    auto* sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(top, 0, wxEXPAND | wxALL, FromDIP(6));
    sizer->Add(status_, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));
    sizer->Add(note_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(8));
    sizer->Add(split, 1, wxEXPAND | wxALL, FromDIP(6));
    SetSizer(sizer);

    box_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { restart(); });
    box_->Bind(wxEVT_SEARCH_CANCEL, [this](wxCommandEvent&) {
        box_->ChangeValue(wxString());
        start();
    });
    box_->Bind(wxEVT_SEARCH, [this](wxCommandEvent&) { jumpToPicked(); });
    box_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { jumpToPicked(); });
    inBroadcast_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { start(); });
    inRagchews_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { start(); });
    from_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { restart(); });
    from_->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { start(); });
    when_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { start(); });
    close_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onClose_) onClose_();
    });
    jump_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { jumpToPicked(); });
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { start(); }, typing_.GetId());
    Bind(wxEVT_TIMER, [this](wxTimerEvent&) { poll(); }, polling_.GetId());

    // Esc closes the tab; up and down in the box move the pick.
    Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& e) {
        int key = e.GetKeyCode();
        if (key == WXK_ESCAPE)
        {
            if (onClose_) onClose_();
            return;
        }
        if ((key == WXK_DOWN || key == WXK_UP) && !hits_.empty() && FindFocus() != list_ && FindFocus() != nearby_
            && FindFocus() != when_ && !from_->HasFocus())
        {
            // Step through the hits in the list's order, over folded groups.
            size_t row = rows_.size();
            for (size_t i = 0; i < rows_.size(); i++)
                if (!rows_[i].head && rows_[i].hit == picked_) row = i;
            int step = key == WXK_DOWN ? 1 : -1;
            for (long i = (long)row + step; i >= 0 && (size_t)i < rows_.size(); i += step)
            {
                if (!rows_[(size_t)i].head)
                {
                    pick(rows_[(size_t)i].hit);
                    break;
                }
            }
            return;
        }
        if ((key == WXK_RETURN || key == WXK_NUMPAD_ENTER) && FindFocus() == list_)
        {
            jumpToPicked();
            return;
        }
        e.Skip();
    });

    picked_ = 0;
    showStatus();
    showNearby();
}

SearchPage::~SearchPage()
{
    stop();
}

void SearchPage::setStations(const std::vector<std::string>& calls)
{
    wxString typed = from_->GetValue();
    wxArrayString items;
    for (const std::string& c : calls) items.Add(utf8(c));
    if (items == from_->GetStrings()) return;
    from_->Set(items);
    from_->ChangeValue(typed);
}

wxString SearchPage::typed() const
{
    return box_->GetValue();
}

void SearchPage::begin(const wxString& words)
{
    if (!words.empty()) box_->ChangeValue(words);
    box_->SetFocus();
    box_->SelectAll();
    setNote(wxString());
    start();
}

void SearchPage::setNote(const wxString& note)
{
    note_->SetLabel(note);
    note_->Show(!note.empty());
    Layout();
}

void SearchPage::pick(size_t hit)
{
    if (hit >= hits_.size()) return;
    picked_ = hit;
    for (size_t i = 0; i < rows_.size(); i++)
    {
        if (!rows_[i].head && rows_[i].hit == hit)
        {
            list_->SetSelection((int)i);
            if (i < list_->GetVisibleRowsBegin() || i + 1 >= list_->GetVisibleRowsEnd())
                list_->ScrollToRow(i > 2 ? i - 2 : 0);
        }
    }
    list_->RefreshAll();
    showNearby();
}

wxString SearchPage::markedHtml(const std::string& text, const std::vector<std::string>& terms, bool picked)
{
    std::vector<std::pair<size_t, size_t>> spans = ragtime::search::matchSpans(terms, text);
    wxColour back = darkSurface() ? wxColour(0x8a, 0x6d, 0x10) : wxColour(0xfb, 0xe2, 0x7a);
    wxColour fore = darkSurface() ? wxColour(0xff, 0xff, 0xff) : wxColour(0x1f, 0x1f, 0x1f);
    wxString html;
    size_t at = 0;
    for (const auto& s : spans)
    {
        html << htmlEscape(text.substr(at, s.first - at));
        wxString word = htmlEscape(text.substr(s.first, s.second - s.first));
        if (picked)
            html << "<b><u>" << word << "</u></b>";
        else
            html << "<span style=\"background-color: " << htmlColour(back) << "\"><b><font color=\""
                 << htmlColour(fore) << "\">" << word << "</font></b></span>";
        at = s.second;
    }
    html << htmlEscape(text.substr(at));
    return html;
}

ragtime::search::Query SearchPage::readQuery() const
{
    ragtime::search::Query q;
    q.terms = ragtime::search::searchTerms(std::string(box_->GetValue().utf8_str()));
    q.from = std::string(from_->GetValue().Strip(wxString::both).utf8_str());
    q.broadcast = inBroadcast_->GetValue();
    q.ragchews = inRagchews_->GetValue();
    int when = when_->GetSelection();
    if (when > 0)
    {
        wxDateTime since = wxDateTime::Today();
        if (when == 2) since -= wxDateSpan::Days(6);
        if (when == 3) since -= wxDateSpan::Days(29);
        q.sinceMs = (uint64_t)since.GetValue().GetValue();
    }
    return q;
}

std::string SearchPage::placeKey(const Hit& hit)
{
    if (hit.place == Place::Broadcast) return "b";
    return "r" + ragtime::search::foldCase(hit.peer) + "@" + std::to_string(hit.beganMs);
}

void SearchPage::restart()
{
    typing_.StartOnce(250);
}

void SearchPage::stop()
{
    stopping_ = true;
    if (worker_.joinable()) worker_.join();
    stopping_ = false;
    polling_.Stop();
}

void SearchPage::start()
{
    typing_.Stop();
    stop();
    query_ = readQuery();
    hits_.clear();
    rows_.clear();
    picked_ = 0;
    capped_ = false;
    found_.clear();
    filesDone_ = 0;
    bytesRead_ = 0;
    std::vector<ragtime::search::Source> sources;
    if (!query_.terms.empty() && sources_) sources = sources_();
    filesTotal_ = sources.size();
    finished_ = sources.empty();
    list_->SetItemCount(0);
    list_->RefreshAll();
    showNearby();
    showStatus();
    if (sources.empty()) return;

    ragtime::search::Query query = query_;
    worker_ = std::thread([this, sources, query]() {
        size_t total = 0;
        for (const ragtime::search::Source& source : sources)
        {
            if (stopping_) break;
            std::vector<Hit> hits = ragtime::search::searchFile(source, query);
            std::error_code ec;
            uintmax_t bytes = std::filesystem::file_size(ragtime::files::pathFromUtf8(source.path), ec);
            total += hits.size();
            std::lock_guard<std::mutex> lock(mutex_);
            for (Hit& h : hits) found_.push_back(std::move(h));
            filesDone_++;
            if (!ec) bytesRead_ += bytes;
            if (total >= MAX_HITS) break;
        }
        std::lock_guard<std::mutex> lock(mutex_);
        finished_ = true;
    });
    polling_.Start(60);
}

void SearchPage::poll()
{
    std::vector<Hit> found;
    bool finished = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        found.swap(found_);
        finished = finished_;
    }
    if (finished && worker_.joinable())
    {
        worker_.join();
        polling_.Stop();
    }
    if (!found.empty())
    {
        // Keep the pick across the new order.
        bool hadPick = picked_ < hits_.size();
        Hit keep = hadPick ? hits_[picked_] : Hit();
        for (Hit& h : found)
        {
            if (hits_.size() >= MAX_HITS)
            {
                capped_ = true;
                break;
            }
            hits_.push_back(std::move(h));
        }
        ragtime::search::sortHits(hits_);
        picked_ = 0;
        if (hadPick)
        {
            for (size_t i = 0; i < hits_.size(); i++)
            {
                const Hit& h = hits_[i];
                if (h.atMs == keep.atMs && h.seq == keep.seq && h.from == keep.from && h.place == keep.place)
                {
                    picked_ = i;
                    break;
                }
            }
        }
        rebuildRows();
        pick(picked_);
    }
    showStatus();
}

void SearchPage::rebuildRows()
{
    rows_.clear();
    size_t head = 0;
    for (size_t i = 0; i < hits_.size(); i++)
    {
        if (i == 0 || !ragtime::search::samePlace(hits_[i], hits_[i - 1]))
        {
            Row row;
            row.head = true;
            row.hit = i;
            head = rows_.size();
            rows_.push_back(row);
        }
        rows_[head].count++;
        bool folded = std::find(folded_.begin(), folded_.end(), placeKey(hits_[i])) != folded_.end();
        if (!folded)
        {
            Row row;
            row.hit = i;
            rows_.push_back(row);
        }
    }
    list_->SetItemCount(rows_.size());
    list_->RefreshAll();
}

void SearchPage::toggle(size_t row)
{
    if (row >= rows_.size()) return;
    std::string key = placeKey(hits_[rows_[row].hit]);
    auto it = std::find(folded_.begin(), folded_.end(), key);
    if (it == folded_.end())
        folded_.push_back(key);
    else
        folded_.erase(it);
    rebuildRows();
    for (size_t i = 0; i < rows_.size(); i++)
        if (rows_[i].head && placeKey(hits_[rows_[i].hit]) == key) list_->SetSelection((int)i);
}

void SearchPage::showStatus()
{
    wxString line;
    if (query_.terms.empty())
        line = "Type words to find them in every Broadcast line and every ragchew kept on disk.";
    else if (!finished_ || worker_.joinable())
        line = wxString::Format("Searching... %s of %s logs read", count(filesDone_), count(filesTotal_));
    else if (hits_.empty())
        line = filesTotal_ == 0 ? wxString("Nothing kept to search yet.")
                                : wxString("No line has all of those words. Try fewer words, or widen From and When.");
    else
    {
        size_t onBroadcast = 0;
        std::vector<std::string> ragchews;
        for (const Hit& h : hits_)
        {
            if (h.place == Place::Broadcast)
                onBroadcast++;
            else if (std::find(ragchews.begin(), ragchews.end(), placeKey(h)) == ragchews.end())
                ragchews.push_back(placeKey(h));
        }
        size_t inRagchews = hits_.size() - onBroadcast;
        line = wxString::Format("%s line%s: %s on Broadcast, %s in %s ragchew%s", count(hits_.size()),
                                hits_.size() == 1 ? "" : "s", count(onBroadcast), count(inRagchews),
                                count(ragchews.size()), ragchews.size() == 1 ? "" : "s");
        line << wxString::FromUTF8(" \xc2\xb7 ") << wxString::Format("searched %s logs, ", count(filesTotal_))
             << wxFileName::GetHumanReadableSize(wxULongLong(bytesRead_), "0 B", 1, wxSIZE_CONV_SI);
        if (capped_) line << wxString::FromUTF8(" \xc2\xb7 ") << "the newest " << count(MAX_HITS) << " shown";
    }
    if (status_->GetLabel() != line) status_->SetLabel(line);
}

void SearchPage::showNearby()
{
    if (picked_ >= hits_.size())
    {
        where_->SetLabel(query_.terms.empty() ? wxString() : wxString("Pick a line to see it in place."));
        jump_->Hide();
        nearby_->SetItemCount(0);
        nearby_->RefreshAll();
        where_->GetParent()->Layout();
        return;
    }
    const Hit& h = hits_[picked_];
    if (h.place == Place::Broadcast)
    {
        where_->SetLabel("Broadcast");
        jump_->SetLabel("Show in Broadcast");
    }
    else
    {
        where_->SetLabel(ragchewTitle(h) + wxString::FromUTF8(" \xc2\xb7 read only"));
        jump_->SetLabel("Open in the Session tab");
    }
    jump_->Show();
    nearby_->SetItemCount(h.nearby.size());
    nearby_->RefreshAll();
    if (h.at < h.nearby.size()) nearby_->ScrollToRow(h.at > 2 ? h.at - 2 : 0);
    where_->GetParent()->Layout();
}

void SearchPage::jumpToPicked()
{
    if (picked_ < hits_.size() && onJump_) onJump_(hits_[picked_]);
}
