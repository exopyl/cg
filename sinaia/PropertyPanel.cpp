#include "PropertyPanel.h"

#include <algorithm>
#include <chrono>

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

void PropertyPanel::RefreshBounds()
{
	if (!m_pBound)
		return;

	// Copie NON const : une valeur retenue au-dela de la nouvelle borne est
	// reecrite dans le membre, faute de quoi la grille afficherait 6 recursions
	// sous une borne a 1 -- Regenerate() la plafonne de son cote, l'affichage
	// mentirait.
	std::vector<Parameter> cur = m_pBound->GetParameters();
	for (Parameter &p : cur)
	{
		wxPGProperty *prop = m_pGrid->GetPropertyByName(p.GetName());
		if (!prop)
			continue;

		if (p.GetType() == Parameter::INT)
		{
			prop->SetAttribute(wxPG_ATTR_MIN, p.GetMinInt());
			prop->SetAttribute(wxPG_ATTR_MAX, p.GetMaxInt());
			const int v = std::min(std::max(p.GetInt(), p.GetMinInt()), p.GetMaxInt());
			if (v != p.GetInt())
			{
				p.SetInt(v);
				prop->SetValue(v);
			}
		}
		else if (p.GetType() == Parameter::FLOAT)
		{
			prop->SetAttribute(wxPG_ATTR_MIN, p.GetMinFloat());
			prop->SetAttribute(wxPG_ATTR_MAX, p.GetMaxFloat());
			const float v = std::min(std::max(p.GetFloat(), p.GetMinFloat()), p.GetMaxFloat());
			if (v != p.GetFloat())
			{
				p.SetFloat(v);
				prop->SetValue((double)v);
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

	// AVANT Regenerate() : la relecture peut plafonner une valeur devenue hors
	// bornes, et c'est cette valeur-la qu'il faut regenerer.
	RefreshBounds();

	auto t0 = std::chrono::high_resolution_clock::now();
	m_pBound->Regenerate();
	auto t1 = std::chrono::high_resolution_clock::now();
	m_lastRegenMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

	if (m_onChanged)
		m_onChanged();
}
