#ifndef RAGTIME_FIND_STRIP_H
#define RAGTIME_FIND_STRIP_H

#include <functional>

#include <wx/panel.h>

class wxButton;
class wxStaticText;

// The strip over the Broadcast and Session tabs' lines after a jump from
// the Search tab: what was looked for, which hit of how many in this place
// shows, earlier and later hits, Back to search, and a close button (Esc
// too, from the page). Hidden until shown.
class FindStrip : public wxPanel
{
public:
    explicit FindStrip(wxWindow* parent);

    // "“dipole”: 2 of 5 on Broadcast", with the arrows enabled as there
    // are earlier and later hits.
    void showHit(const wxString& label, bool earlier, bool later);
    void close();

    // -1 for the earlier hit, +1 for the later one.
    void onStep(std::function<void(int step)> fn) { onStep_ = std::move(fn); }
    void onBack(std::function<void()> fn) { onBack_ = std::move(fn); }
    void onClose(std::function<void()> fn) { onClose_ = std::move(fn); }

private:
    wxStaticText* label_ = nullptr;
    wxButton* earlier_ = nullptr;
    wxButton* later_ = nullptr;
    std::function<void(int)> onStep_;
    std::function<void()> onBack_, onClose_;
};

#endif
