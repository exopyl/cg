#pragma once

#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/checkbox.h>
#include <wx/radiobox.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/sizer.h>
#include <wx/aui/auibook.h>

#include <functional>

#include "ImportSettings.h"
#include "PanelSettings.h"

//
// SettingsDialog: tabbed application settings.
//
// The "Import" tab exposes the operations applied to a 3D model when it is
// loaded (see ImportSettings); these are read back with GetImportSettings()
// only after a wxID_OK result.
//
// The "2D Panel" tab exposes the notebook appearance options that used to live
// in the "Options > 3D Panel" menu (see PanelSettings). These are applied
// *live*: every time a control changes, the onPanelChanged callback is invoked
// with the current values so the effect is visible immediately. Pressing "OK"
// keeps the live state; pressing "Cancel" (or closing the dialog) restores the
// settings that were in effect when the dialog opened.
//
class SettingsDialog : public wxDialog
{
public:
    SettingsDialog(wxWindow* parent,
                   const ImportSettings& settings,
                   const PanelSettings& panel,
                   std::function<void(const PanelSettings&)> onPanelChanged = {})
        : wxDialog(parent, wxID_ANY, _("Settings"),
                   wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
        , m_origStyle(panel.notebookStyle)
        , m_initialPanel(panel)
        , m_onPanelChanged(std::move(onPanelChanged))
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

        // --- 2D Panel tab ---
        notebook->AddPage(BuildPanelPage(notebook, panel), _("2D Panel"));

        // --- 3D Panel tab ---
        notebook->AddPage(BuildViewPage(notebook, panel), _("3D Panel"));

        // --- notebook + OK/Cancel ---
        wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(notebook, 1, wxEXPAND | wxALL, 5);
        topSizer->Add(CreateButtonSizer(wxOK | wxCANCEL), 0, wxEXPAND | wxALL, 5);
        SetSizerAndFit(topSizer);

        SetMinSize(wxSize(320, 220));
        Centre();

        // Cancel / close the window restores the settings in effect on open.
        Bind(wxEVT_BUTTON, &SettingsDialog::OnCancel, this, wxID_CANCEL);
        Bind(wxEVT_CLOSE_WINDOW, &SettingsDialog::OnCloseWindow, this);
    }

    ImportSettings GetImportSettings() const
    {
        ImportSettings s;
        s.normalize     = m_normalize->GetValue();
        s.triangulate   = m_triangulate->GetValue();
        s.mergeVertices = m_mergeVertices->GetValue();
        return s;
    }

    PanelSettings GetPanelSettings() const
    {
        PanelSettings p;

        // Preserve any flags not exposed by the dialog; rebuild only the bits
        // controlled by the radio boxes / checkboxes below.
        long style = m_origStyle;

        // Close button mode (radio: None / Right / All / Active).
        style &= ~(wxAUI_NB_CLOSE_BUTTON |
                   wxAUI_NB_CLOSE_ON_ALL_TABS |
                   wxAUI_NB_CLOSE_ON_ACTIVE_TAB);
        switch (m_closeMode->GetSelection())
        {
            case 1: style |= wxAUI_NB_CLOSE_BUTTON;         break;
            case 2: style |= wxAUI_NB_CLOSE_ON_ALL_TABS;    break;
            case 3: style |= wxAUI_NB_CLOSE_ON_ACTIVE_TAB;  break;
            default: break; // 0 = no close button
        }

        // Tab alignment (radio: Top / Bottom).
        style &= ~(wxAUI_NB_TOP | wxAUI_NB_BOTTOM);
        style |= (m_alignment->GetSelection() == 1) ? wxAUI_NB_BOTTOM
                                                     : wxAUI_NB_TOP;

        // Independent flag checkboxes.
        SetFlag(style, wxAUI_NB_TAB_MOVE,          m_allowTabMove->GetValue());
        SetFlag(style, wxAUI_NB_TAB_EXTERNAL_MOVE, m_allowExternalMove->GetValue());
        SetFlag(style, wxAUI_NB_TAB_SPLIT,         m_allowSplit->GetValue());
        SetFlag(style, wxAUI_NB_SCROLL_BUTTONS,    m_scrollButtons->GetValue());
        SetFlag(style, wxAUI_NB_WINDOWLIST_BUTTON, m_windowList->GetValue());
        SetFlag(style, wxAUI_NB_TAB_FIXED_WIDTH,   m_fixedWidth->GetValue());

        p.notebookStyle = style;
        p.notebookTheme = m_theme->GetSelection();

        // 3D Panel tab: line width / point size (integer pixels).
        p.lineWidth = static_cast<float>(m_lineWidth->GetValue());
        p.pointSize = static_cast<float>(m_pointSize->GetValue());
        return p;
    }

private:
    static void SetFlag(long& style, long flag, bool on)
    {
        if (on) style |= flag; else style &= ~flag;
    }

    static int ClampInt(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static wxString PxLabel(int v) { return wxString::Format(wxT("%d px"), v); }

    // 3D Panel tab: two sliders driving the 3D view line width and point size.
    // Live-applied like the 2D Panel controls.
    wxPanel* BuildViewPage(wxWindow* parent, const PanelSettings& panel)
    {
        wxPanel* page = new wxPanel(parent, wxID_ANY);

        m_lineWidth = new wxSlider(page, wxID_ANY,
                                   ClampInt(static_cast<int>(panel.lineWidth + 0.5f), 1, 10),
                                   1, 10, wxDefaultPosition, wxDefaultSize,
                                   wxSL_HORIZONTAL | wxSL_AUTOTICKS);
        m_lineWidthLabel = new wxStaticText(page, wxID_ANY, PxLabel(m_lineWidth->GetValue()));

        m_pointSize = new wxSlider(page, wxID_ANY,
                                   ClampInt(static_cast<int>(panel.pointSize + 0.5f), 1, 20),
                                   1, 20, wxDefaultPosition, wxDefaultSize,
                                   wxSL_HORIZONTAL | wxSL_AUTOTICKS);
        m_pointSizeLabel = new wxStaticText(page, wxID_ANY, PxLabel(m_pointSize->GetValue()));

        // label | slider (grows) | value
        wxFlexGridSizer* grid = new wxFlexGridSizer(2, 3, 8, 10);
        grid->AddGrowableCol(1, 1);
        grid->Add(new wxStaticText(page, wxID_ANY, _("Line width")), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(m_lineWidth, 1, wxEXPAND);
        grid->Add(m_lineWidthLabel, 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(new wxStaticText(page, wxID_ANY, _("Point size")), 0, wxALIGN_CENTER_VERTICAL);
        grid->Add(m_pointSize, 1, wxEXPAND);
        grid->Add(m_pointSizeLabel, 0, wxALIGN_CENTER_VERTICAL);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(grid, 0, wxEXPAND | wxALL, 10);
        page->SetSizer(sizer);

        m_lineWidth->Bind(wxEVT_SLIDER, &SettingsDialog::OnViewChanged, this);
        m_pointSize->Bind(wxEVT_SLIDER, &SettingsDialog::OnViewChanged, this);

        return page;
    }

    wxPanel* BuildPanelPage(wxWindow* parent, const PanelSettings& panel)
    {
        wxPanel* page = new wxPanel(parent, wxID_ANY);

        // Theme (tab art provider).
        const wxString themeChoices[] = { _("Glossy Theme (Default)"),
                                          _("Simple Theme") };
        m_theme = new wxRadioBox(page, wxID_ANY, _("Theme"),
                                 wxDefaultPosition, wxDefaultSize,
                                 WXSIZEOF(themeChoices), themeChoices,
                                 1, wxRA_SPECIFY_COLS);
        m_theme->SetSelection(panel.notebookTheme == 1 ? 1 : 0);

        // Close button mode.
        const wxString closeChoices[] = { _("No Close Button"),
                                          _("Close Button at Right"),
                                          _("Close Button on All Tabs"),
                                          _("Close Button on Active Tab") };
        m_closeMode = new wxRadioBox(page, wxID_ANY, _("Close Button"),
                                     wxDefaultPosition, wxDefaultSize,
                                     WXSIZEOF(closeChoices), closeChoices,
                                     1, wxRA_SPECIFY_COLS);
        m_closeMode->SetSelection(CloseModeFromStyle(panel.notebookStyle));

        // Tab alignment.
        const wxString alignChoices[] = { _("Tab Top Alignment"),
                                          _("Tab Bottom Alignment") };
        m_alignment = new wxRadioBox(page, wxID_ANY, _("Alignment"),
                                     wxDefaultPosition, wxDefaultSize,
                                     WXSIZEOF(alignChoices), alignChoices,
                                     1, wxRA_SPECIFY_COLS);
        m_alignment->SetSelection((panel.notebookStyle & wxAUI_NB_BOTTOM) ? 1 : 0);

        // Independent flags.
        m_allowTabMove      = new wxCheckBox(page, wxID_ANY, _("Allow Tab Move"));
        m_allowExternalMove = new wxCheckBox(page, wxID_ANY, _("Allow External Tab Move"));
        m_allowSplit        = new wxCheckBox(page, wxID_ANY, _("Allow Notebook Split"));
        m_scrollButtons     = new wxCheckBox(page, wxID_ANY, _("Scroll Buttons Visible"));
        m_windowList        = new wxCheckBox(page, wxID_ANY, _("Window List Button Visible"));
        m_fixedWidth        = new wxCheckBox(page, wxID_ANY, _("Fixed-width Tabs"));

        m_allowTabMove->SetValue((panel.notebookStyle & wxAUI_NB_TAB_MOVE) != 0);
        m_allowExternalMove->SetValue((panel.notebookStyle & wxAUI_NB_TAB_EXTERNAL_MOVE) != 0);
        m_allowSplit->SetValue((panel.notebookStyle & wxAUI_NB_TAB_SPLIT) != 0);
        m_scrollButtons->SetValue((panel.notebookStyle & wxAUI_NB_SCROLL_BUTTONS) != 0);
        m_windowList->SetValue((panel.notebookStyle & wxAUI_NB_WINDOWLIST_BUTTON) != 0);
        m_fixedWidth->SetValue((panel.notebookStyle & wxAUI_NB_TAB_FIXED_WIDTH) != 0);

        // Layout: radio boxes on a row, flags stacked below.
        wxBoxSizer* radios = new wxBoxSizer(wxHORIZONTAL);
        radios->Add(m_theme,     0, wxALL, 6);
        radios->Add(m_closeMode, 0, wxALL, 6);
        radios->Add(m_alignment, 0, wxALL, 6);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(radios, 0);
        sizer->Add(m_allowTabMove,      0, wxLEFT | wxRIGHT | wxTOP, 8);
        sizer->Add(m_allowExternalMove, 0, wxLEFT | wxRIGHT | wxTOP, 4);
        sizer->Add(m_allowSplit,        0, wxLEFT | wxRIGHT | wxTOP, 4);
        sizer->Add(m_scrollButtons,     0, wxLEFT | wxRIGHT | wxTOP, 4);
        sizer->Add(m_windowList,        0, wxLEFT | wxRIGHT | wxTOP, 4);
        sizer->Add(m_fixedWidth,        0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 8);

        page->SetSizer(sizer);

        // Live apply: any change to a control is pushed to the frame at once.
        m_theme->Bind(wxEVT_RADIOBOX,     &SettingsDialog::OnPanelChanged, this);
        m_closeMode->Bind(wxEVT_RADIOBOX, &SettingsDialog::OnPanelChanged, this);
        m_alignment->Bind(wxEVT_RADIOBOX, &SettingsDialog::OnPanelChanged, this);
        for (wxCheckBox* cb : { m_allowTabMove, m_allowExternalMove, m_allowSplit,
                                m_scrollButtons, m_windowList, m_fixedWidth })
            cb->Bind(wxEVT_CHECKBOX, &SettingsDialog::OnPanelChanged, this);

        return page;
    }

    // Push the current 2D Panel values to the frame for immediate preview.
    void OnPanelChanged(wxCommandEvent& event)
    {
        if (m_onPanelChanged)
            m_onPanelChanged(GetPanelSettings());
        event.Skip();
    }

    // Push the current 3D Panel slider values to the frame for immediate
    // preview, and refresh the px readouts next to the sliders.
    void OnViewChanged(wxCommandEvent& event)
    {
        m_lineWidthLabel->SetLabel(PxLabel(m_lineWidth->GetValue()));
        m_pointSizeLabel->SetLabel(PxLabel(m_pointSize->GetValue()));
        if (m_onPanelChanged)
            m_onPanelChanged(GetPanelSettings());
        event.Skip();
    }

    // Cancel: roll the live preview back to the settings on open, then close.
    void OnCancel(wxCommandEvent& WXUNUSED(event))
    {
        RestoreInitialPanel();
        EndModal(wxID_CANCEL);
    }

    void OnCloseWindow(wxCloseEvent& WXUNUSED(event))
    {
        RestoreInitialPanel();
        EndModal(wxID_CANCEL);
    }

    void RestoreInitialPanel()
    {
        if (m_onPanelChanged)
            m_onPanelChanged(m_initialPanel);
    }

    static int CloseModeFromStyle(long style)
    {
        if (style & wxAUI_NB_CLOSE_ON_ALL_TABS)   return 2;
        if (style & wxAUI_NB_CLOSE_ON_ACTIVE_TAB) return 3;
        if (style & wxAUI_NB_CLOSE_BUTTON)        return 1;
        return 0;
    }

private:
    // Import tab.
    wxCheckBox* m_normalize;
    wxCheckBox* m_triangulate;
    wxCheckBox* m_mergeVertices;

    // 2D Panel tab.
    long                m_origStyle;
    PanelSettings       m_initialPanel;                 // state to restore on Cancel
    std::function<void(const PanelSettings&)> m_onPanelChanged;  // live-apply hook
    wxRadioBox*         m_theme;
    wxRadioBox*         m_closeMode;
    wxRadioBox*         m_alignment;
    wxCheckBox*         m_allowTabMove;
    wxCheckBox*         m_allowExternalMove;
    wxCheckBox*         m_allowSplit;
    wxCheckBox*         m_scrollButtons;
    wxCheckBox*         m_windowList;
    wxCheckBox*         m_fixedWidth;

    // 3D Panel tab.
    wxSlider*           m_lineWidth;
    wxSlider*           m_pointSize;
    wxStaticText*       m_lineWidthLabel;
    wxStaticText*       m_pointSizeLabel;
};
