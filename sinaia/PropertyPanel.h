#pragma once
#include <wx/panel.h>
#include <wx/propgrid/propgrid.h>

#include "../src/cgmesh/parameterized.h"

//
// PropertyPanel: a dockable wxAUI panel that displays and edits the
// parameters of the currently bound IParameterized object.
//
// Usage:
//   PropertyPanel *panel = new PropertyPanel(parent);
//   panel->Bind(parameterizedObject);  // populates the grid
//   // any change triggers object->Regenerate() and fires an optional callback
//
class PropertyPanel : public wxPanel
{
public:
	PropertyPanel(wxWindow *parent);

	// Bind to a parameterized object. Pass nullptr to clear.
	// The panel does NOT take ownership of `obj`.
	void Bind(IParameterized *obj);

	// Called after each successful Regenerate(), if set.
	// Allows the host frame to refresh the 3D view.
	void SetOnChanged(std::function<void()> cb) { m_onChanged = std::move(cb); }

private:
	void OnPropertyChanged(wxPropertyGridEvent &event);
	void Rebuild();

	wxPropertyGrid *m_pGrid = nullptr;
	IParameterized *m_pBound = nullptr;
	std::vector<Parameter> m_params;  // cached copy for the bound object
	std::function<void()> m_onChanged;

	wxDECLARE_EVENT_TABLE();
};
