#include "FindStrip.h"

#include <wx/button.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "Html.h"
#include "Theme.h"

FindStrip::FindStrip(wxWindow* parent) : wxPanel(parent, wxID_ANY)
{
    followThemeBackground(this, wxSYS_COLOUR_BTNFACE);
    label_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    earlier_ = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xb2"), wxDefaultPosition, wxDefaultSize,
                            wxBU_EXACTFIT);
    earlier_->SetToolTip("The earlier line found here");
    later_ = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xbc"), wxDefaultPosition, wxDefaultSize,
                          wxBU_EXACTFIT);
    later_->SetToolTip("The later line found here");
    auto* back = new wxButton(this, wxID_ANY, "Back to search");
    auto* close = new wxButton(this, wxID_ANY, wxString::FromUTF8("\xe2\x9c\x95"), wxDefaultPosition, wxDefaultSize,
                               wxBU_EXACTFIT);
    close->SetToolTip("Close this strip (Esc)");

    auto* sizer = new wxBoxSizer(wxHORIZONTAL);
    sizer->Add(label_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, FromDIP(6));
    sizer->Add(earlier_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(2));
    sizer->Add(later_, 0, wxALIGN_CENTER_VERTICAL);
    sizer->AddStretchSpacer();
    sizer->Add(back, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(4));
    sizer->Add(close, 0, wxALIGN_CENTER_VERTICAL);
    SetSizer(sizer);

    earlier_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onStep_) onStep_(-1);
    });
    later_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onStep_) onStep_(1);
    });
    back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (onBack_) onBack_();
    });
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { this->close(); });
    Hide();
}

void FindStrip::showHit(const wxString& label, bool earlier, bool later)
{
    label_->SetLabel(label);
    earlier_->Enable(earlier);
    later_->Enable(later);
    if (!IsShown())
    {
        Show();
        GetParent()->Layout();
    }
    Layout();
}

void FindStrip::close()
{
    if (!IsShown()) return;
    Hide();
    if (onClose_) onClose_();
    GetParent()->Layout();
}
