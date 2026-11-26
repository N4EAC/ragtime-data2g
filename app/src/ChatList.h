#ifndef RAGTIME_CHAT_LIST_H
#define RAGTIME_CHAT_LIST_H

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <wx/htmllbox.h>

// A chat's messages as HTML rows (the Broadcast and Session tabs), kept
// following the newest. While the last row shows, a new message or a
// resize scrolls it back into view; scrolled up to read, the list stays
// put. A resize matters as well as a new message: a narrower window, or a
// smaller screen from a remote desktop, wraps long lines onto more rows,
// and wx keeps the top row where it was, so the newest lines went below
// the bottom and the list stopped following (Jeff, 2026-10-26).
//
// Double-clicking a callsign in a row opens a ragchew with that station
// (Jeff, 2026-11-13): wx tells which word the first click of the two hit,
// and a word that is a callsign the row names, or a station Ragtime
// knows, goes to onCallsign.
//
// A change builds anew only the rows whose HTML changed. wx lays out a row
// from its HTML each time it measures it, keeping only the last 50 it drew,
// and the count, the scroll to the newest and the paint each measure, so
// every change built each row on screen about eight times over: some
// 50 ms of the window's time on every message, state and seal change. The
// list keeps each row's HTML and height for the rows it gave lately, asks
// for their HTML again at a change, and has wx forget only those that
// differ. A new width forgets the heights.
//
// Older rows come in front as the operator scrolls near the top: the list
// asks onNearTop, and showRowsAbove keeps the rows on screen where they
// were, so the line being read doesn't jump.
//
// Whether it follows is settled the moment the operator scrolls, not at
// the next paint. On GTK a scroll's own scroll bar update sends a size
// event at once, before anything is drawn; judged at paint, the list
// still thought it was following then and jumped back to the bottom, so
// the wheel and the scroll bar couldn't move it (Jeff, 2026-11-01).
class ChatList : public wxHtmlListBox
{
public:
    explicit ChatList(wxWindow* parent);

    // Sets the count and builds anew the rows whose HTML changed, then
    // shows the newest row if the list was following it.
    void showRows(size_t count);

    // Builds anew the rows whose HTML changed (a voice card's playhead,
    // say), with the count as it is.
    void refreshChanged();

    // count rows now, the first `added` of them new in front: the rows on
    // screen stay where they were (or the newest shows, while following).
    void showRowsAbove(size_t count, size_t added);

    // Asked, after the event, when the operator scrolls to within
    // NEAR_TOP_ROWS of the first row: older rows wanted.
    static constexpr size_t NEAR_TOP_ROWS = 40;
    void onNearTop(std::function<void()> fn) { onNearTop_ = std::move(fn); }

    // How many rows wx has had built and laid out, all told: for tests.
    size_t rowsBuilt() const { return rowsBuilt_; }

    // True while the newest row is kept in view.
    bool following() const { return following_; }
    // Keeps the newest row in view from the next showRows: other rows in
    // place of these, where reading from the old place means nothing.
    void followNewest() { following_ = true; }

    // Every scroll comes through here, with the new top row already set
    // and before the scroll bar is told: a scroll that wasn't ours is the
    // operator's, and says whether the newest row still shows.
    void UpdateScrollbar() override;

    // Forget the rows' HTML and heights with wx's built rows: every row, or
    // the ones named. wx calls RefreshAll from SetItemCount too: a count
    // that only grows keeps them all (rows keep their places).
    void RefreshAll() override;
    void RefreshRow(size_t row) override;
    void RefreshRows(size_t from, size_t to) override;

    // What wx shows for a row, kept for refreshChanged: subclasses give
    // theirs in rowMarkup.
    wxString OnGetItemMarkup(size_t n) const final;

protected:
    // A row's height, kept until its HTML or the width changes.
    wxCoord OnMeasureItem(size_t n) const override;

public:

    // A callsign double-clicked in a row: one the row names (callsIn), or
    // one isStation says is a station heard. Called after the click.
    void onCallsign(std::function<void(const std::string& call)> fn,
                    std::function<bool(const std::string& call)> isStation)
    {
        onCallsign_ = std::move(fn);
        isStation_ = std::move(isStation);
    }

protected:
    // A row's HTML, as wxHtmlListBox's OnGetItemMarkup gives it (by
    // default OnGetItem's). Asked again at each change, so it should only
    // read: equal HTML means the row hasn't changed.
    virtual wxString rowMarkup(size_t n) const { return wxHtmlListBox::OnGetItemMarkup(n); }

    // The stations a row names: who sent it, who it is to, the Hands that
    // answered a roll call.
    virtual std::vector<std::string> callsIn(size_t row) const
    {
        (void)row;
        return {};
    }

private:
    // The double click's callsign, or "" when it didn't land on one.
    std::string callsignAt(const wxPoint& where) const;

    bool lastRowShows() const;
    // Scrolls so the newest row shows, as the list's own move.
    void pin();

    // Rows whose HTML and height are kept: more than wx's 50 built rows,
    // so none it holds is missing, and a few pages of heights.
    static constexpr size_t KEPT_ROWS = 300;

    struct Kept
    {
        wxString markup;        // as last given to wx
        wxCoord height = -1;    // measured at width, or -1
        int width = 0;
        uint64_t used = 0;
    };

    bool following_ = true;
    bool pinning_ = false;     // inside our own scroll
    bool growing_ = false;     // inside SetItemCount, rows only added
    wxSize laidOut_;           // the client size the rows were built for
    size_t seenTop_ = 0;       // the top row at the last scroll bar update
    wxString clickedWord_;     // the word under the last click, and where it was
    wxPoint clickedAt_;
    std::function<void(const std::string&)> onCallsign_;
    std::function<bool(const std::string&)> isStation_;
    std::function<void()> onNearTop_;
    bool askedTop_ = false;     // a near-top call waiting to run
    mutable std::map<size_t, Kept> kept_;
    mutable std::vector<size_t> untracked_;     // let go of since the last change
    mutable uint64_t uses_ = 0;
    mutable size_t rowsBuilt_ = 0;
};

#endif
