#include "ShortcutsDialog.h"

#include "ShortcutTable.h"

#include <wx/accel.h>
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/menu.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <memory>
#include <utility>
#include <vector>

namespace {

unsigned ToModifiers(int acceleratorFlags)
{
    unsigned mods = sinaia::modifier::None;
    if (acceleratorFlags & (wxACCEL_CTRL | wxACCEL_RAW_CTRL)) mods |= sinaia::modifier::Ctrl;
    if (acceleratorFlags & wxACCEL_ALT)                       mods |= sinaia::modifier::Alt;
    if (acceleratorFlags & wxACCEL_SHIFT)                     mods |= sinaia::modifier::Shift;
    return mods;
}

wxString FormatAccelerator(const wxAcceleratorEntry& accel)
{
    const wxString key = wxAcceleratorEntry(wxACCEL_NORMAL, accel.GetKeyCode()).ToString();
    return wxString::FromUTF8(sinaia::FormatChord(ToModifiers(accel.GetFlags()),
                                                  std::string(key.utf8_str())));
}

using Rows = std::vector<std::pair<wxString, wxString>>;

void CollectAccelerators(const wxMenu* menu, const wxString& path, Rows& rows)
{
    for (const wxMenuItem* item : menu->GetMenuItems())
    {
        if (item->IsSeparator())
            continue;
        const wxString label = item->GetItemLabelText();
        const wxString itemPath = path.empty() ? label : path + wxT(" > ") + label;
        if (const wxMenu* sub = item->GetSubMenu())
        {
            CollectAccelerators(sub, itemPath, rows);
            continue;
        }
        const std::unique_ptr<wxAcceleratorEntry> accel(item->GetAccel());
        if (accel && accel->IsOk())
            rows.emplace_back(FormatAccelerator(*accel), itemPath);
    }
}

} // namespace

ShortcutsDialog::ShortcutsDialog(wxWindow* parent, const wxMenuBar* menuBar)
    : wxDialog(parent, wxID_ANY, _("Shortcuts"), wxDefaultPosition, wxDefaultSize,
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);

    top->Add(new wxStaticText(this, wxID_ANY,
                              _("Keys of the 3D view apply when it has the focus: click it first.")),
             0, wxALL, FromDIP(8));

    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES);
    m_list->AppendColumn(_("Shortcut"));
    m_list->AppendColumn(_("Action"));
    top->Add(m_list, 1, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(8));

    AddCategory(wxString::FromUTF8(sinaia::kCanvasKeysCategory));
    for (const sinaia::CanvasKeyBinding& b : sinaia::kCanvasKeyBindings)
        AddRow(wxString::FromUTF8(sinaia::FormatChord(b.modifiers, sinaia::CanvasKeyName(b.key))),
               wxString::FromUTF8(b.action));

    const char* currentCategory = nullptr;
    for (const sinaia::GestureEntry& g : sinaia::kGestures)
    {
        if (!currentCategory || std::string_view(currentCategory) != g.category)
        {
            currentCategory = g.category;
            AddCategory(wxString::FromUTF8(g.category));
        }
        AddRow(wxString::FromUTF8(g.gesture), wxString::FromUTF8(g.action));
    }

    if (menuBar)
    {
        for (size_t i = 0; i < menuBar->GetMenuCount(); ++i)
            AddMenuAccelerators(menuBar->GetMenu(i), menuBar->GetMenuLabelText(i));
    }

    m_list->SetColumnWidth(0, wxLIST_AUTOSIZE);
    m_list->SetColumnWidth(1, wxLIST_AUTOSIZE);

    wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
    buttons->AddButton(new wxButton(this, wxID_CLOSE));
    buttons->Realize();
    top->Add(buttons, 0, wxEXPAND | wxALL, FromDIP(8));

    SetSizer(top);
    SetMinSize(FromDIP(wxSize(420, 300)));
    SetSize(FromDIP(wxSize(680, 560)));
    CentreOnParent();

    // Échap émule un clic sur Close ; Close, Échap et la case du titre
    // aboutissent tous à la destruction.
    SetEscapeId(wxID_CLOSE);
    Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); }, wxID_CLOSE);
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent&) { Destroy(); });
}

void ShortcutsDialog::AddCategory(const wxString& category)
{
    const long row = m_list->InsertItem(m_list->GetItemCount(), category);
    wxFont bold = m_list->GetFont();
    bold.MakeBold();
    m_list->SetItemFont(row, bold);
    m_list->SetItemBackgroundColour(row, wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE));
}

void ShortcutsDialog::AddRow(const wxString& shortcut, const wxString& action)
{
    const long row = m_list->InsertItem(m_list->GetItemCount(), wxT("    ") + shortcut);
    m_list->SetItem(row, 1, action);
}

void ShortcutsDialog::AddMenuAccelerators(const wxMenu* menu, const wxString& category)
{
    if (!menu)
        return;
    Rows rows;
    CollectAccelerators(menu, wxString(), rows);
    if (rows.empty())
        return;
    AddCategory(category + wxT(" menu"));
    for (const auto& [shortcut, action] : rows)
        AddRow(shortcut, action);
}
