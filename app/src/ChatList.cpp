#include "ChatList.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include <wx/html/htmlcell.h>
#include <wx/html/htmlwin.h>

#include "ragtime/protocol/Fields.h"

ChatList::ChatList(wxWindow* parent)
    : wxHtmlListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_THEME)
{
    // Cocoa can focus an empty list before its first size event. Give wx
    // a valid visible-row range before focus tries to refresh selection.
    SetItemCount(0);
    SetMargins(FromDIP(4), FromDIP(1));
    // The window has its new size, but the top row hasn't moved: scroll so
    // the newest row ends at the bottom again. wx's own handler runs after
    // this one, and a scroll bar coming or going sends another size.
    //
    // GTK sends a size event when only the scroll bar was updated, with the
    // size unchanged, and wx throws away every row it built at any size
    // event: one with the same client size goes no further.
    Bind(wxEVT_SIZE, [this](wxSizeEvent& e) {
        if (following_) pin();
        const wxSize client = GetClientSize();
        if (client == laidOut_) return;
        laidOut_ = client;
        e.Skip();
    });
    // wx says which cell a click hit, but not on a double click: the
    // word under the first click of the two is kept for it.
    Bind(wxEVT_HTML_CELL_CLICKED, [this](wxHtmlCellEvent& e) {
        const wxHtmlCell* cell = e.GetCell();
        clickedWord_ = cell != nullptr ? cell->ConvertToText(nullptr) : wxString();
        clickedAt_ = e.GetMouseEvent().GetPosition();
        e.Skip();
    });
    Bind(wxEVT_LEFT_DCLICK, [this](wxMouseEvent& e) {
        e.Skip();
        if (!onCallsign_) return;
        std::string call = callsignAt(e.GetPosition());
        clickedWord_.clear();
        if (call.empty()) return;
        auto fn = onCallsign_;
        CallAfter([fn, call]() { fn(call); });
    });
}

std::string ChatList::callsignAt(const wxPoint& where) const
{
    // The first click of the two, where the second landed.
    const int slack = FromDIP(4);
    if (clickedWord_.empty() || std::abs(where.x - clickedAt_.x) > slack || std::abs(where.y - clickedAt_.y) > slack)
        return std::string();
    int row = VirtualHitTest(where.y);
    if (row == wxNOT_FOUND) return std::string();
    std::string call = ragtime::protocol::callsignInWord(std::string(clickedWord_.utf8_str()));
    if (call.empty()) return std::string();
    std::vector<std::string> named = callsIn((size_t)row);
    if (std::find(named.begin(), named.end(), call) != named.end()) return call;
    return isStation_ && isStation_(call) ? call : std::string();
}

wxString ChatList::OnGetItemMarkup(size_t n) const
{
    wxString markup = rowMarkup(n);
    rowsBuilt_++;
    Kept& k = kept_[n];
    if (k.markup != markup) k.height = -1;
    k.markup = markup;
    k.used = ++uses_;
    if (kept_.size() > KEPT_ROWS)
    {
        // The row used longest ago goes.
        auto oldest = kept_.begin();
        for (auto it = kept_.begin(); it != kept_.end(); ++it)
            if (it->second.used < oldest->second.used) oldest = it;
        if (oldest->first != n)
        {
            // wx may still hold it built: forgotten at the next change.
            untracked_.push_back(oldest->first);
            kept_.erase(oldest);
        }
    }
    return markup;
}

wxCoord ChatList::OnMeasureItem(size_t n) const
{
    const int width = GetClientSize().x;
    auto it = kept_.find(n);
    if (it != kept_.end() && it->second.height >= 0 && it->second.width == width)
    {
        it->second.used = ++uses_;
        return it->second.height;
    }
    // wx lays the row out, asking for its HTML (OnGetItemMarkup keeps it).
    const wxCoord height = wxHtmlListBox::OnMeasureItem(n);
    it = kept_.find(n);
    if (it != kept_.end())
    {
        it->second.height = height;
        it->second.width = width;
    }
    return height;
}

void ChatList::refreshChanged()
{
    const size_t count = GetItemCount();
    std::vector<size_t> changed;
    for (auto it = kept_.begin(); it != kept_.end();)
    {
        if (it->first >= count)
        {
            it = kept_.erase(it);
            continue;
        }
        if (rowMarkup(it->first) != it->second.markup) changed.push_back(it->first);
        ++it;
    }
    // Rows let go of may have changed unseen.
    for (size_t row : untracked_)
        if (row < count && kept_.count(row) == 0) changed.push_back(row);
    untracked_.clear();
    // wx forgets each and builds it when next it measures or draws.
    for (size_t row : changed) RefreshRow(row);
}

void ChatList::showRows(size_t count)
{
    bool follow = following_ || lastRowShows();
    pinning_ = true;
    if (count < GetItemCount())
    {
        // Rows gone: start afresh.
        SetItemCount(count);
        RefreshAll();
    }
    else
    {
        // The changed rows first, so the new count measures none stale.
        refreshChanged();
        if (count != GetItemCount())
        {
            // wxHtmlListBox's own SetItemCount forgets every row built.
            growing_ = true;
            wxVListBox::SetItemCount(count);
            growing_ = false;
        }
    }
    pinning_ = false;
    following_ = follow;
    seenTop_ = GetVisibleRowsBegin();
    if (follow) pin();
}

void ChatList::RefreshAll()
{
    if (growing_)
    {
        // The rows built stay; the window is redrawn.
        wxVListBox::RefreshAll();
        return;
    }
    kept_.clear();
    untracked_.clear();
    wxHtmlListBox::RefreshAll();
}

void ChatList::RefreshRow(size_t row)
{
    kept_.erase(row);
    wxHtmlListBox::RefreshRow(row);
}

void ChatList::RefreshRows(size_t from, size_t to)
{
    kept_.erase(kept_.lower_bound(from), kept_.upper_bound(to));
    wxHtmlListBox::RefreshRows(from, to);
}

void ChatList::UpdateScrollbar()
{
    // Decided before wx tells the scroll bar, which on GTK can send a size
    // event straight away.
    const size_t top = GetVisibleRowsBegin();
    if (!pinning_ && top != seenTop_) following_ = lastRowShows();
    seenTop_ = top;
    wxHtmlListBox::UpdateScrollbar();
}

void ChatList::pin()
{
    if (GetItemCount() == 0) return;
    pinning_ = true;
    ScrollToRow(GetItemCount() - 1);
    pinning_ = false;
    seenTop_ = GetVisibleRowsBegin();
}

bool ChatList::lastRowShows() const
{
    // Worked out from the rows' heights rather than wx's count of rows on
    // screen, which is only brought up to date after this is asked.
    const size_t count = GetItemCount();
    return count == 0 || GetVisibleRowsBegin() >= FindFirstVisibleFromLast(count - 1);
}
