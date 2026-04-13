#include "PropertyPanel.h"

#include <wx/sizer.h>
#include <wx/propgrid/advprops.h>

wxBEGIN_EVENT_TABLE(PropertyPanel, wxPanel)
	EVT_PG_CHANGED(wxID_ANY, PropertyPanel::OnPropertyChanged)
wxEND_EVENT_TABLE()

PropertyPanel::PropertyPanel(wxWindow *parent)
	: wxPanel(parent, wxID_ANY)
{
	m_pGrid = new wxPropertyGrid(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
	                             wxPG_SPLITTER_AUTO_CENTER | wxPG_BOLD_MODIFIED);

	wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(m_pGrid, 1, wxEXPAND);
	SetSizer(sizer);
}

void PropertyPanel::Bind(IParameterized *obj)
{
	m_pBound = obj;
	Rebuild();
}

void PropertyPanel::Rebuild()
{
	m_pGrid->Clear();
	m_params.clear();

	if (!m_pBound)
		return;

	m_params = m_pBound->GetParameters();

	// Section header with the object's display name
	m_pGrid->Append(new wxPropertyCategory(m_pBound->GetName()));

	// One grid property per Parameter. The property name is used as a lookup
	// key when the user edits a value (matched against Parameter::GetName()).
	for (const Parameter &p : m_params)
	{
		const wxString name = p.GetName();
		switch (p.GetType())
		{
		case Parameter::INT:
		{
			wxPGProperty *prop = m_pGrid->Append(new wxIntProperty(name, name, p.GetInt()));
			prop->SetAttribute(wxPG_ATTR_MIN, p.GetMinInt());
			prop->SetAttribute(wxPG_ATTR_MAX, p.GetMaxInt());
			break;
		}
		case Parameter::FLOAT:
		{
			wxPGProperty *prop = m_pGrid->Append(new wxFloatProperty(name, name, p.GetFloat()));
			prop->SetAttribute(wxPG_ATTR_MIN, p.GetMinFloat());
			prop->SetAttribute(wxPG_ATTR_MAX, p.GetMaxFloat());
			break;
		}
		case Parameter::BOOL:
		{
			wxPGProperty *prop = m_pGrid->Append(new wxBoolProperty(name, name, p.GetBool()));
			prop->SetAttribute(wxPG_BOOL_USE_CHECKBOX, true);
			break;
		}
		case Parameter::ENUM:
		{
			wxArrayString labels;
			wxArrayInt values;
			for (size_t i = 0; i < p.GetChoices().size(); i++)
			{
				labels.Add(p.GetChoices()[i]);
				values.Add((int)i);
			}
			m_pGrid->Append(new wxEnumProperty(name, name, labels, values, p.GetInt()));
			break;
		}
		}
	}
}

void PropertyPanel::OnPropertyChanged(wxPropertyGridEvent &event)
{
	if (!m_pBound)
		return;

	wxPGProperty *prop = event.GetProperty();
	if (!prop)
		return;

	const wxString changedName = prop->GetName();

	// Look up the Parameter matching the property name and push the new value
	// back into the underlying storage.
	for (Parameter &p : m_params)
	{
		if (changedName != p.GetName())
			continue;

		switch (p.GetType())
		{
		case Parameter::INT:
		case Parameter::ENUM:
			p.SetInt(prop->GetValue().GetLong());
			break;
		case Parameter::FLOAT:
			p.SetFloat((float)prop->GetValue().GetDouble());
			break;
		case Parameter::BOOL:
			p.SetBool(prop->GetValue().GetBool());
			break;
		}
		break;
	}

	m_pBound->Regenerate();
	if (m_onChanged)
		m_onChanged();
}
