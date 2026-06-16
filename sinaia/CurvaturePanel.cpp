#include "CurvaturePanel.h"

#include <wx/sizer.h>
#include <wx/combobox.h>
#include <wx/checkbox.h>
#include <wx/stattext.h>

// control ids
enum
{
	ID_CURV_METHOD = wxID_HIGHEST + 1,
	ID_CURV_TYPE,
	ID_CURV_APPLY
};

// Combo index <-> enum mappings. The combo items are in this order; the
// arrays translate the selected index back to the cgmesh enums.
static const TensorMethodId kMethods[] = { TENSOR_TAUBIN, TENSOR_DESBRUN, TENSOR_HAMANN };
static const CurvatureType  kTypes[]   = { CurvatureType::Gaussian, CurvatureType::Mean,
                                           CurvatureType::Min, CurvatureType::Max };

static int MethodToIndex(TensorMethodId m)
{
	for (int i = 0; i < (int)(sizeof(kMethods) / sizeof(kMethods[0])); i++)
		if (kMethods[i] == m) return i;
	return 0;
}

static int TypeToIndex(CurvatureType t)
{
	for (int i = 0; i < (int)(sizeof(kTypes) / sizeof(kTypes[0])); i++)
		if (kTypes[i] == t) return i;
	return 1;  // default: Mean
}

wxBEGIN_EVENT_TABLE(CurvaturePanel, wxPanel)
	EVT_COMBOBOX(ID_CURV_METHOD, CurvaturePanel::OnMethod)
	EVT_COMBOBOX(ID_CURV_TYPE,   CurvaturePanel::OnType)
	EVT_CHECKBOX(ID_CURV_APPLY,  CurvaturePanel::OnApply)
wxEND_EVENT_TABLE()

CurvaturePanel::CurvaturePanel(wxWindow *parent)
	: wxPanel(parent, wxID_ANY)
{
	// Match the white background of the other docked widgets (the panel
	// otherwise inherits the grey system face colour).
	SetBackgroundColour(*wxWHITE);

	wxArrayString methods;
	methods.Add(_("Taubin"));
	methods.Add(_("Desbrun"));
	methods.Add(_("Hamann"));

	wxArrayString types;
	types.Add(_("Gaussian"));
	types.Add(_("Mean"));
	types.Add(_("Min"));
	types.Add(_("Max"));

	// Read-only combo boxes: a drop-down list, no free text entry.
	m_pMethod = new wxComboBox(this, ID_CURV_METHOD, methods[0], wxDefaultPosition,
	                           wxDefaultSize, methods, wxCB_READONLY);
	m_pType = new wxComboBox(this, ID_CURV_TYPE, types[TypeToIndex(CurvatureType::Mean)],
	                         wxDefaultPosition, wxDefaultSize, types, wxCB_READONLY);

	m_pType->SetSelection(TypeToIndex(CurvatureType::Mean));

	// Toggles whether the curvature colour map is shown on the mesh. When
	// off, the mesh keeps its normal appearance but the chosen method/type
	// are remembered for when it is turned back on.
	m_pApply = new wxCheckBox(this, ID_CURV_APPLY, _("Apply visualization"));
	m_pApply->SetValue(false);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(new wxStaticText(this, wxID_ANY, _("Method")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	sizer->Add(m_pMethod, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
	sizer->Add(new wxStaticText(this, wxID_ANY, _("Curvature")), 0, wxLEFT | wxRIGHT | wxTOP, 6);
	sizer->Add(m_pType, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
	sizer->Add(m_pApply, 0, wxALL, 6);
	SetSizer(sizer);
}

void CurvaturePanel::SetSelection(TensorMethodId method, CurvatureType type)
{
	// SetSelection() on a wxComboBox does not emit an EVT_COMBOBOX, so this is
	// safe to call without re-triggering the host callbacks.
	if (m_pMethod) m_pMethod->SetSelection(MethodToIndex(method));
	if (m_pType)   m_pType->SetSelection(TypeToIndex(type));
}

void CurvaturePanel::SetEnabled(bool enabled)
{
	// SetValue() does not emit an EVT_CHECKBOX, so this won't re-fire m_onEnable.
	if (m_pApply) m_pApply->SetValue(enabled);
}

void CurvaturePanel::OnMethod(wxCommandEvent &event)
{
	const int i = event.GetSelection();
	if (m_onMethod && i >= 0 && i < (int)(sizeof(kMethods) / sizeof(kMethods[0])))
		m_onMethod(kMethods[i]);
}

void CurvaturePanel::OnType(wxCommandEvent &event)
{
	const int i = event.GetSelection();
	if (m_onType && i >= 0 && i < (int)(sizeof(kTypes) / sizeof(kTypes[0])))
		m_onType(kTypes[i]);
}

void CurvaturePanel::OnApply(wxCommandEvent &event)
{
	if (m_onEnable)
		m_onEnable(event.IsChecked());
}
