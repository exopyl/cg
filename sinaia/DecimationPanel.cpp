#include "DecimationPanel.h"

#include <wx/sizer.h>
#include <wx/slider.h>
#include <wx/checkbox.h>
#include <wx/textctrl.h>
#include <wx/stattext.h>
#include <wx/button.h>

enum
{
	ID_DECIM_APPLY = wxID_HIGHEST + 100
};

wxBEGIN_EVENT_TABLE(DecimationPanel, wxPanel)
	EVT_BUTTON(ID_DECIM_APPLY, DecimationPanel::OnApply)
wxEND_EVENT_TABLE()

DecimationPanel::DecimationPanel(wxWindow *parent)
	: wxPanel(parent, wxID_ANY)
{
	SetBackgroundColour(*wxWHITE);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);

	sizer->Add(new wxStaticText(this, wxID_ANY, _("Target faces (% of original)")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	m_pRatio = new wxSlider(this, wxID_ANY, 50, 1, 100, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
	sizer->Add(m_pRatio, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_pFeatures = new wxCheckBox(this, wxID_ANY, _("Preserve features (creases, material seams)"));
	m_pFeatures->SetValue(true);
	sizer->Add(m_pFeatures, 0, wxALL, 6);

	sizer->Add(new wxStaticText(this, wxID_ANY, _("Feature angle (deg)")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	m_pAngle = new wxSlider(this, wxID_ANY, 45, 0, 180, wxDefaultPosition, wxDefaultSize, wxSL_HORIZONTAL | wxSL_LABELS);
	sizer->Add(m_pAngle, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_pAttrs = new wxCheckBox(this, wxID_ANY, _("Preserve colours / normals"));
	m_pAttrs->SetValue(true);
	sizer->Add(m_pAttrs, 0, wxALL, 6);

	m_pMetric = new wxCheckBox(this, wxID_ANY, _("Attribute-aware metric (nD quadric)"));
	m_pMetric->SetValue(false);
	sizer->Add(m_pMetric, 0, wxALL, 6);

	sizer->Add(new wxStaticText(this, wxID_ANY, _("Attribute weight")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	m_pWeight = new wxTextCtrl(this, wxID_ANY, wxT("1.0"));
	sizer->Add(m_pWeight, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_pUV = new wxCheckBox(this, wxID_ANY, _("Preserve UV (split seams)"));
	m_pUV->SetValue(false);
	sizer->Add(m_pUV, 0, wxALL, 6);

	sizer->Add(new wxStaticText(this, wxID_ANY, _("Max error (fraction of bbox diagonal, 0 = off)")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	m_pMaxError = new wxTextCtrl(this, wxID_ANY, wxT("0"));
	sizer->Add(m_pMaxError, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_pExact = new wxCheckBox(this, wxID_ANY, _("Reliable surface error bound"));
	m_pExact->SetValue(false);
	sizer->Add(m_pExact, 0, wxALL, 6);

	sizer->Add(new wxButton(this, ID_DECIM_APPLY, _("Apply decimation")), 0, wxALL, 6);

	SetSizer(sizer);
}

static double parse_or(wxTextCtrl *t, double def)
{
	double d = def;
	if (t && t->GetValue().ToDouble(&d))
		return d;
	return def;
}

float DecimationPanel::GetTargetRatio(void) const      { return m_pRatio ? m_pRatio->GetValue() / 100.f : 0.5f; }
bool  DecimationPanel::GetPreserveFeatures(void) const { return m_pFeatures && m_pFeatures->GetValue(); }
float DecimationPanel::GetFeatureAngleDeg(void) const  { return m_pAngle ? (float)m_pAngle->GetValue() : 45.f; }
bool  DecimationPanel::GetPreserveAttributes(void) const { return m_pAttrs && m_pAttrs->GetValue(); }
bool  DecimationPanel::GetAttributeMetric(void) const  { return m_pMetric && m_pMetric->GetValue(); }
float DecimationPanel::GetAttributeWeight(void) const  { return (float)parse_or(m_pWeight, 1.0); }
bool  DecimationPanel::GetPreserveUV(void) const       { return m_pUV && m_pUV->GetValue(); }
float DecimationPanel::GetMaxError(void) const         { return (float)parse_or(m_pMaxError, 0.0); }
bool  DecimationPanel::GetExactError(void) const       { return m_pExact && m_pExact->GetValue(); }

void DecimationPanel::OnApply(wxCommandEvent &WXUNUSED(event))
{
	if (m_onApply)
		m_onApply();
}
