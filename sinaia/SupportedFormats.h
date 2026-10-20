#pragma once

// Chaînes wx dérivées du catalogue des formats (FormatCatalog.h), unique source
// de vérité partagée par le wildcard du wxFileDialog (File > Open, File > Add)
// et par le filtre et l'icône du panneau latéral "Files".

#include <wx/string.h>

#include "FormatCatalog.h"

namespace sinaia {

// Wildcard des dialogues d'ouverture : OpenSCAD n'y figure que si
// `openScadAvailable` (répertoire réglé valide).
inline wxString BuildOpenWildcard(bool openScadAvailable)
{
    return wxString::FromUTF8(BuildOpenWildcardText(openScadAvailable));
}

// Emplacement d'icône (>= 0) d'une extension du catalogue entier, insensible
// à la casse, ou -1 si l'extension n'est pas importable (à masquer du panneau).
inline int IconForExtension(const wxString& ext)
{
    const FormatSpec* f = FindFormat(ext.utf8_string());
    return f ? static_cast<int>(f->icon) : -1;
}

// Libellé lisible de la famille d'une extension (ex. "STEP"), ou chaîne vide
// si l'extension n'est pas importable.
inline wxString LabelForExtension(const wxString& ext)
{
    const FormatSpec* f = FindFormat(ext.utf8_string());
    return f ? wxString::FromUTF8(f->label) : wxString();
}

} // namespace sinaia
