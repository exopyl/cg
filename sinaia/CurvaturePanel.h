#pragma once
#include <wx/panel.h>
#include <functional>

#include "../src/cgmesh/DiffParamEvaluator.h"  // TensorMethodId, CurvatureType

class wxComboBox;
class wxCheckBox;

//
// CurvaturePanel: a dockable wxAUI panel that lets the user pick which
// curvature to visualise on the active 3D model.
//
// It exposes two choices:
//   - the estimation Method  (Taubin / Desbrun / Hamann), and
//   - the Curvature scalar   (Gaussian / Mean / Min / Max).
//
// The panel owns no model state; it merely reports user choices through two
// callbacks. The host frame (MyFrame) stores the per-canvas selection,
// (re)computes the tensor on a Method change and re-colours the mesh on a
// curvature-type change. Call SetSelection() to make the radios reflect the
// active canvas's stored choice before showing the pane.
//
class CurvaturePanel : public wxPanel
{
public:
	CurvaturePanel(wxWindow *parent);

	// Make the combos reflect a stored selection without firing callbacks.
	void SetSelection(TensorMethodId method, CurvatureType type);

	// Reflect the "apply visualization" state without firing the callback.
	void SetEnabled(bool enabled);

	void SetOnMethodChanged(std::function<void(TensorMethodId)> cb) { m_onMethod = std::move(cb); }
	void SetOnTypeChanged(std::function<void(CurvatureType)> cb)    { m_onType   = std::move(cb); }
	void SetOnEnableChanged(std::function<void(bool)> cb)           { m_onEnable = std::move(cb); }

private:
	void OnMethod(wxCommandEvent &event);
	void OnType(wxCommandEvent &event);
	void OnApply(wxCommandEvent &event);

	wxComboBox *m_pMethod = nullptr;
	wxComboBox *m_pType   = nullptr;
	wxCheckBox *m_pApply  = nullptr;

	std::function<void(TensorMethodId)> m_onMethod;
	std::function<void(CurvatureType)>  m_onType;
	std::function<void(bool)>           m_onEnable;

	wxDECLARE_EVENT_TABLE();
};
