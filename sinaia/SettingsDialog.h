#pragma once

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>

#include "ImportSettings.h"

//
// SettingsDialog: tabbed application settings.
//
// The first tab, "Import", exposes the operations applied to a 3D model
// when it is loaded (see ImportSettings). The dialog is purely a value
// editor: it is seeded with the current settings and the caller reads the
// edited values back with GetImportSettings() after a wxID_OK result.
//
class SettingsDialog : public wxDialog
{
public:
    SettingsDialog(wxWindow* parent, const ImportSettings& settings)
        : wxDialog(parent, wxID_ANY, _("Settings"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
    {
        wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

        // --- Import tab ---
        wxPanel* importPage = new wxPanel(notebook, wxID_ANY);

        m_normalize     = new wxCheckBox(importPage, wxID_ANY, _("Normalisation"));
        m_triangulate   = new wxCheckBox(importPage, wxID_ANY, _("Triangulate"));
        m_mergeVertices = new wxCheckBox(importPage, wxID_ANY, _("Merge vertices"));

        m_normalize->SetValue(settings.normalize);
        m_triangulate->SetValue(settings.triangulate);
        m_mergeVertices->SetValue(settings.mergeVertices);

        wxBoxSizer* pageSizer = new wxBoxSizer(wxVERTICAL);
        pageSizer->Add(m_normalize,     0, wxALL, 8);
        pageSizer->Add(m_triangulate,   0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        pageSizer->Add(m_mergeVertices, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        importPage->SetSizer(pageSizer);

        notebook->AddPage(importPage, _("Import"), true);

        // --- notebook + OK/Cancel ---
        wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
        topSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 5);
        SetSizerAndFit(topSizer);

        SetMinSize(wxSize(320, 220));
        Centre();
    }

    ImportSettings GetImportSettings() const
    {
        ImportSettings s;
        s.normalize     = m_normalize->GetValue();
        s.triangulate   = m_triangulate->GetValue();
        s.mergeVertices = m_mergeVertices->GetValue();
        return s;
    }

private:
    wxCheckBox* m_normalize;
    wxCheckBox* m_triangulate;
    wxCheckBox* m_mergeVertices;
};
