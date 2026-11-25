#ifndef RAGTIME_SESSION_PAGE_H
#define RAGTIME_SESSION_PAGE_H

#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include <wx/font.h>
#include <wx/panel.h>

#include "FrontWindow.h"
#include "ragtime/files/FileTraffic.h"
#include "ragtime/session/SessionChat.h"

class wxBoxSizer;
class wxBitmapToggleButton;
class wxButton;
class wxListBox;
class wxStaticText;
class wxTextCtrl;
class SessionMessages;

// The Session tab: the status strip
// across the top with the one button the moment needs (Cancel while
// calling, Leave in a session, Leave now while leaving, Call again once it
// has ended); the session's messages, each of ours Sent, then Delivered
// when the far end's modem has acknowledged it; and the line to type in.
// With no session up the tab is greyed: the last session's messages stay
// readable, and nothing can be typed.
//
// A station picked in Recent work, with no session up, shows here read
// only (Jeff, 2026-11-25): its earlier ragchews from the session log,
// greyed as an ended session is, with the words in the typing line where a
// lost session puts its own, and a button to call it. A session starting
// ends the review.
//
// Files (package G) show among the messages as cards: an offer to us with
// Save, Save as..., Decline and its countdown, and "4,096 of 7,000 kept
// from before" when a part is kept; a file moving either way with its
// progress and Cancel; how each ended. Files dropped on the tab, or picked
// with Send file..., queue for the partner; the queue shows above the line
// to type in, to reorder or remove before each starts.
class SessionPage : public wxPanel
{
public:
    // The notebook gives the page the focus as it shows it: only in a
    // window already in front, or a tab brought up by something heard
    // lifts the main window over the Ear (FrontWindow.h).
    void SetFocus() override
    {
        if (inFrontWindow(this)) wxPanel::SetFocus();
    }

    SessionPage(wxWindow* parent, const ragtime::session::SessionChat& session,
                const ragtime::files::FileTraffic& files);

    // Send pressed: true when the text went, and the line is cleared.
    void onSend(std::function<bool(const std::string& typed)> fn) { onSend_ = std::move(fn); }
    void onCancel(std::function<void()> fn) { onCancel_ = std::move(fn); }
    void onLeave(std::function<void()> fn) { onLeave_ = std::move(fn); }
    void onLeaveNow(std::function<void()> fn) { onLeaveNow_ = std::move(fn); }
    // Call again pressed, or Call in a review: the station to call.
    void onCallAgain(std::function<void(const std::string& call)> fn) { onCallAgain_ = std::move(fn); }
    // A callsign double-clicked in a line: the partner's, or a station
    // isStation knows named in the text.
    void onCallsign(std::function<void(const std::string& call)> fn,
                    std::function<bool(const std::string& call)> isStation);
    // Re-send on a message's right-click menu, with the message's id.
    void onResend(std::function<void(uint64_t id)> fn) { onResend_ = std::move(fn); }
    // A reaction picked on a message's menu or chip, with its palette code.
    void onReact(std::function<void(uint64_t id, uint8_t code)> fn) { onReact_ = std::move(fn); }
    // The station's history: loadOlder puts the page before the first row
    // in front and says how many came; hold keeps old rows while the list
    // isn't following the newest.
    void onHistory(std::function<size_t()> loadOlder, std::function<void(bool hold)> hold)
    {
        onLoadOlder_ = std::move(loadOlder);
        onHoldHistory_ = std::move(hold);
    }
    // Send file... pressed.
    void onSendFile(std::function<void()> fn) { onSendFile_ = std::move(fn); }
    // Inline pictures: the picture
    // button, left of the floppy; a picture card's picture as it stands, an
    // <img> linked to href ("" while nothing shows yet), with the stop
    // button linked to stopHref when that isn't empty; and a word that a
    // picture changed, so its card is drawn again.
    void onSendPicture(std::function<void()> fn) { onSendPicture_ = std::move(fn); }
    // The microphone button, left of the picture's: opens and closes the
    // microphone deck, and stays pushed in while it is open.
    void onVoiceDeck(std::function<void()> fn) { onVoiceDeck_ = std::move(fn); }
    void setVoiceDeckLit(bool lit);
    void pictures(std::function<wxString(uint64_t id, const wxWindow* list, const wxString& href,
                                         const wxString& stopHref)> fn)
    {
        pictureHtml_ = std::move(fn);
    }
    void picturesChanged() { shownFiles_ = ~0ull; }
    // A picture under an earlier session's file line, from the file kept
    // (its path in the session log), and that picture clicked.
    void pastPictures(std::function<wxString(const std::string& path, const wxWindow* list, const wxString& href)> html,
                      std::function<void(const std::string& path)> view)
    {
        pastPictureHtml_ = std::move(html);
        onViewPast_ = std::move(view);
    }
    // Voice cards: a card's button and
    // strip by its key ("s<id>" a transfer, "p<path>" an earlier
    // session's, from its file; "" while nothing shows), and a card's link
    // ("voice:play:0", "voice:stop:0", "voice:seek:<frame>") clicked.
    void voices(std::function<wxString(const std::string& key, const wxWindow* list, bool picked)> html,
                std::function<void(const std::string& key, const std::string& href)> act)
    {
        voiceHtml_ = std::move(html);
        onVoice_ = std::move(act);
    }
    // Redraws the cards in view (a playhead moving), without a rebuild.
    void refreshVoices();
    // Files dropped on the tab.
    void onDropFiles(std::function<void(const std::vector<std::string>& paths)> fn) { onDropFiles_ = std::move(fn); }
    // A card's link or the queue's buttons: "save", "saveas", "decline",
    // "cancel", "open", "folder", "up", "down", and a picture's "view" and
    // "enough" (its stop sign), with the transfer's id.
    void onFileAction(std::function<void(const std::string& action, uint64_t id)> fn)
    {
        onFileAction_ = std::move(fn);
    }

    // Redraws the strip every second, and the messages when they change.
    void refresh(uint64_t nowMs, long utcOffsetS);
    // Pause sending shows a line saying so; with a reason (receive only,
    // outside the US data segments) the line gives it instead.
    void setPaused(bool paused, const wxString& why = wxString());
    void focusCompose();

    // The earlier ragchews with call, read only, in place of the session's
    // messages until endReview, or until a session starts.
    void review(const std::string& call, std::vector<ragtime::session::Message> messages);
    void endReview();
    // The station under review, or "" when none is.
    const std::string& reviewing() const { return reviewCall_; }
    // The words in the typing line during a review.
    static std::string reviewWords(const std::string& call);

private:
    friend class SessionMessages;

    // One line of the list: a message, or a file's card.
    struct Item
    {
        bool file = false;
        size_t message = 0;         // the message's index
        uint64_t transfer = 0;      // the file's id
        uint64_t atMs = 0;
    };

    // What the list shows: the review's, or the session's.
    const std::deque<ragtime::session::Message>& shownMessages() const
    {
        return reviewCall_.empty() ? session_.messages() : review_;
    }
    const std::string& shownPeer() const { return reviewCall_.empty() ? session_.peer() : reviewCall_; }

    void send();
    void showMenu(int row);
    void rebuild(uint64_t nowMs);
    void refreshQueue();
    wxString cardHtml(const ragtime::files::FileTransfer& t, bool picked, const wxWindow* list) const;

    const ragtime::session::SessionChat& session_;
    const ragtime::files::FileTraffic& files_;
    std::vector<Item> items_;
    std::map<uint64_t, ragtime::files::FileTransfer> transfers_;   // the cards', as of the last rebuild
    std::vector<uint64_t> queued_;                                  // the queue's, top first
    uint64_t nowMs_ = 0;
    long utcOffsetS_ = 0;
    uint64_t shownFiles_ = ~0ull;
    bool liveFiles_ = false;        // a card counts down or moves: redrawn every second
    wxPanel* strip_ = nullptr;
    wxStaticText* stripText_ = nullptr;
    wxButton* cancel_ = nullptr;
    wxButton* leave_ = nullptr;
    wxButton* leaveNow_ = nullptr;
    wxButton* callAgain_ = nullptr;
    SessionMessages* list_ = nullptr;
    wxStaticText* paused_ = nullptr;
    wxTextCtrl* compose_ = nullptr;
    wxFont composeFont_;
    bool showingWords_ = false;     // the line holds words, not typing: "Session lost", or a review's
    std::string shownWords_;
    std::string reviewCall_;        // the station under review, or ""
    std::deque<ragtime::session::Message> review_;
    std::string shownReview_;
    wxButton* send_ = nullptr;
    wxButton* sendFile_ = nullptr;
    wxButton* sendPicture_ = nullptr;
    wxBitmapToggleButton* mic_ = nullptr;
    wxPanel* queue_ = nullptr;
    wxStaticText* queueTitle_ = nullptr;
    wxListBox* queueList_ = nullptr;
    uint64_t shownVersion_ = ~0ull;
    std::string shownStrip_;
    bool shownAmber_ = false;
    std::string shownHint_;
    int shownPhase_ = -1;

    std::function<bool(const std::string&)> onSend_;
    std::function<void()> onCancel_, onLeave_, onLeaveNow_, onSendFile_, onSendPicture_, onVoiceDeck_;
    std::function<void(const std::string&)> onCallAgain_;
    std::function<wxString(uint64_t, const wxWindow*, const wxString&, const wxString&)> pictureHtml_;
    std::function<wxString(const std::string&, const wxWindow*, const wxString&)> pastPictureHtml_;
    std::function<wxString(const std::string&, const wxWindow*, bool)> voiceHtml_;
    std::function<void(const std::string&, const std::string&)> onVoice_;
    std::function<void(const std::string&)> onViewPast_;
    std::function<void(const std::vector<std::string>&)> onDropFiles_;
    std::function<void(const std::string&, uint64_t)> onFileAction_;
    std::function<void(uint64_t)> onResend_;
    std::function<void(uint64_t, uint8_t)> onReact_;
    std::function<size_t()> onLoadOlder_;
    std::function<void(bool)> onHoldHistory_;
};

#endif
