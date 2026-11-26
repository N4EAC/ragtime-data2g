#ifndef RAGTIME_SEARCH_PAGE_H
#define RAGTIME_SEARCH_PAGE_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <wx/panel.h>
#include <wx/timer.h>

#include "ragtime/search/HistorySearch.h"

class wxButton;
class wxCheckBox;
class wxChoice;
class wxComboBox;
class wxSearchCtrl;
class wxStaticText;
class SearchResults;
class SearchNearby;

// The Search tab (Ctrl+F, Jeff 2026-11-25): words looked for in every
// Broadcast line and every ragchew kept on disk, the rolled history too.
// Hits come as the search reads the logs, on a thread of its own, newest
// files first, and list grouped by place: Broadcast first, then each
// ragchew, the latest first, each group folding at a click on its head.
// The picked hit shows in place below, a few lines either side; Enter, a
// double click or the button there jumps to it in its own tab
// (onJump). Esc or Close shuts the tab (onClose).
class SearchPage : public wxPanel
{
public:
    explicit SearchPage(wxWindow* parent);
    ~SearchPage() override;

    // The files to search, asked again at each search: the logs grow and
    // roll.
    void sources(std::function<std::vector<ragtime::search::Source>()> fn) { sources_ = std::move(fn); }
    // The stations the From box offers.
    void setStations(const std::vector<std::string>& calls);
    // A hit to show in its own tab.
    void onJump(std::function<void(const ragtime::search::Hit& hit)> fn) { onJump_ = std::move(fn); }
    void onClose(std::function<void()> fn) { onClose_ = std::move(fn); }

    // The cursor in the box, all of it picked, with words to look for
    // first when given; the search runs again (the logs may have grown).
    void begin(const wxString& words = wxString());
    // A line under the status: why a jump couldn't go, say.
    void setNote(const wxString& note);

    // What was found, in the list's order, and the terms looked for.
    const std::vector<ragtime::search::Hit>& hits() const { return hits_; }
    const std::vector<std::string>& terms() const { return query_.terms; }
    wxString typed() const;
    // The picked hit, or hits().size() when none is; and picking one.
    size_t picked() const { return picked_; }
    void pick(size_t hit);

    // A hit's text with the terms marked, as HTML (the lists use it).
    static wxString markedHtml(const std::string& text, const std::vector<std::string>& terms, bool picked);

private:
    friend class SearchResults;
    friend class SearchNearby;

    // One row of the list: a group's head, or a hit.
    struct Row
    {
        bool head = false;
        size_t hit = 0;         // the hit, or a head's first hit
        size_t count = 0;       // a head's hits
    };

    void restart();         // after a change: wait a moment for more typing
    void start();           // stop any search going, and start this one
    void stop();
    void poll();            // take what the thread found
    void rebuildRows();
    void showStatus();
    void showNearby();
    void jumpToPicked();
    void toggle(size_t row);
    ragtime::search::Query readQuery() const;
    static std::string placeKey(const ragtime::search::Hit& hit);

    wxSearchCtrl* box_ = nullptr;
    wxCheckBox* inBroadcast_ = nullptr;
    wxCheckBox* inRagchews_ = nullptr;
    wxComboBox* from_ = nullptr;
    wxChoice* when_ = nullptr;
    wxButton* close_ = nullptr;
    wxStaticText* status_ = nullptr;
    wxStaticText* note_ = nullptr;
    SearchResults* list_ = nullptr;
    wxStaticText* where_ = nullptr;
    wxButton* jump_ = nullptr;
    SearchNearby* nearby_ = nullptr;
    wxTimer typing_;
    wxTimer polling_;

    ragtime::search::Query query_;
    std::vector<ragtime::search::Hit> hits_;
    std::vector<Row> rows_;
    std::vector<std::string> folded_;       // the places folded, by placeKey
    size_t picked_ = 0;
    bool capped_ = false;

    // The thread's: what it found since the last poll, how far it got.
    std::thread worker_;
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::vector<ragtime::search::Hit> found_;
    size_t filesDone_ = 0;
    size_t filesTotal_ = 0;
    uint64_t bytesRead_ = 0;
    bool finished_ = true;

    std::function<std::vector<ragtime::search::Source>()> sources_;
    std::function<void(const ragtime::search::Hit&)> onJump_;
    std::function<void()> onClose_;

public:
    // More hits than this, and the search stops with the newest files'.
    static constexpr size_t MAX_HITS = 2000;
};

#endif
