#pragma once
#include <wx/panel.h>
#include <functional>

class wxSlider;
class wxCheckBox;
class wxTextCtrl;

//
// DecimationPanel: a dockable wxAUI panel exposing the parameters of the QEM
// mesh decimation (Mesh_half_edge::simplify) and an "Apply" button.
//
// The panel owns no model state: it only gathers the user's parameter choices
// (via getters) and fires a single Apply callback. The host frame reads the
// getters, builds a SimplifyOptions and runs the decimation on the active
// model. Modelled on CurvaturePanel.
//
class DecimationPanel : public wxPanel
{
public:
	DecimationPanel(wxWindow *parent);

	float GetTargetRatio(void) const;      // fraction of faces to keep, 0..1
	bool  GetPreserveFeatures(void) const;
	float GetFeatureAngleDeg(void) const;
	bool  GetPreserveAttributes(void) const;
	bool  GetAttributeMetric(void) const;
	float GetAttributeWeight(void) const;
	bool  GetPreserveUV(void) const;
	float GetMaxError(void) const;         // fraction of bbox diagonal, 0 = off
	bool  GetExactError(void) const;

	void SetOnApply(std::function<void()> cb) { m_onApply = std::move(cb); }

private:
	void OnApply(wxCommandEvent &event);

	wxSlider   *m_pRatio    = nullptr;
	wxCheckBox *m_pFeatures = nullptr;
	wxSlider   *m_pAngle    = nullptr;
	wxCheckBox *m_pAttrs    = nullptr;
	wxCheckBox *m_pMetric   = nullptr;
	wxTextCtrl *m_pWeight   = nullptr;
	wxCheckBox *m_pUV       = nullptr;
	wxTextCtrl *m_pMaxError = nullptr;
	wxCheckBox *m_pExact    = nullptr;

	std::function<void()> m_onApply;

	wxDECLARE_EVENT_TABLE();
};
