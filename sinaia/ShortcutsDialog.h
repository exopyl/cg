#pragma once

#include <wx/dialog.h>

class wxListCtrl;
class wxMenu;
class wxMenuBar;

// Help > Shortcuts : liste non modale des raccourcis de sinaia.
//
// Trois sources, aucune recopiée :
//   - les accélérateurs lus dans la barre de menus passée au constructeur ;
//   - sinaia::kCanvasKeyBindings, la table que MyGLCanvas::OnKeyDown dispatche ;
//   - sinaia::kGestures, pour la souris et le glisser-déposer.
//
// Le dialogue se détruit à sa fermeture (Close, Échap, case du titre) : le
// propriétaire le suit par un wxWeakRef pour n'en garder qu'une instance.
class ShortcutsDialog : public wxDialog
{
public:
    ShortcutsDialog(wxWindow* parent, const wxMenuBar* menuBar);

private:
    void AddCategory(const wxString& category);
    void AddRow(const wxString& shortcut, const wxString& action);
    void AddMenuAccelerators(const wxMenu* menu, const wxString& category);

    wxListCtrl* m_list = nullptr;
};
