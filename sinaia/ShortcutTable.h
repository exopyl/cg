#pragma once

// Raccourcis de sinaia qui ne passent pas par la barre de menus : touches du
// canvas 3D et gestes souris. Sans wxWidgets, pour être testé dans TU.
//
// Deux tables, deux statuts :
//   - kCanvasKeyBindings est LUE par MyGLCanvas::OnKeyDown : une touche absente
//     de la table n'a aucun effet, une touche présente est affichée par
//     Help > Shortcuts. Le libellé ne peut pas diverger du dispatch.
//   - kGestures est DESCRIPTIVE : MyGLCanvas::OnMouse et les panneaux ne la
//     lisent pas (debt_sinaia.md §10). Toute modification d'un geste dans le
//     code doit être reportée ici.
//
// Les accélérateurs de menus ne figurent dans aucune des deux : le dialogue les
// lit à l'exécution dans la wxMenuBar.

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace sinaia {

// Modificateurs, combinables par OU binaire. Indépendants des valeurs wx :
// le canvas convertit l'état de l'événement (CanvasModifiers dans
// wxOpenGLCanvas.cpp).
namespace modifier {
constexpr unsigned None  = 0u;
constexpr unsigned Ctrl  = 1u << 0;
constexpr unsigned Alt   = 1u << 1;
constexpr unsigned Shift = 1u << 2;
} // namespace modifier

// Touches que le canvas sait recevoir. La correspondance avec les codes wx est
// dans ToCanvasKey (wxOpenGLCanvas.cpp) : une nouvelle valeur doit y être
// ajoutée, sans quoi sa liaison ne se déclenche jamais.
enum class CanvasKey { F, Up, Down };

enum class CanvasCommand { FrameSelectedModel, RaiseClippingPlane, LowerClippingPlane };

struct CanvasKeyBinding
{
    CanvasKey     key;
    unsigned      modifiers;   // combinaison EXACTE exigée
    CanvasCommand command;
    const char*   action;      // libellé affiché par Help > Shortcuts
};

inline constexpr const char* kCanvasKeysCategory = "3D view (keyboard)";

inline constexpr CanvasKeyBinding kCanvasKeyBindings[] = {
    { CanvasKey::F,    modifier::None, CanvasCommand::FrameSelectedModel,
      "Frame the selected model and rotate around its center (no effect without a selection)" },
    { CanvasKey::Up,   modifier::None, CanvasCommand::RaiseClippingPlane,
      "Raise the clipping plane (when the Clipping toolbar button is on)" },
    { CanvasKey::Down, modifier::None, CanvasCommand::LowerClippingPlane,
      "Lower the clipping plane (when the Clipping toolbar button is on)" },
};

struct GestureEntry
{
    const char* category;
    const char* gesture;
    const char* action;
};

inline constexpr GestureEntry kGestures[] = {
    { "3D view (mouse)", "Left drag",
      "Rotate the view around the rotation center (mode: Options > 3D Panel > Trackballs)" },
    { "3D view (mouse)", "Middle drag",           "Pan the view" },
    { "3D view (mouse)", "Right drag",            "Dolly: drag down to move closer, up to move away" },
    { "3D view (mouse)", "Wheel",                 "Dolly: roll forward to move closer, backward to move away" },
    { "3D view (mouse)", "Left click",            "Select the model under the cursor (empty space keeps the selection)" },
    { "3D view (mouse)", "Alt + Left click",      "Set the rotation center on the surface under the cursor" },
    { "3D view (mouse)", "Hover",                 "Highlight the model under the cursor and its row in Models" },

    { "Drag and drop",   "Drop files",            "Open each file in a new tab (on the 3D view or the main window)" },
    { "Drag and drop",   "Shift + Drop files",    "Add the files to the current view" },

    { "Files panel",     "Double click / Enter",  "Open the selected files, one tab each" },
    { "Files panel",     "Drag to the 3D view",   "Same as dropping the selected files" },
    { "Files panel",     "Right click",           "Context menu: Open, Add to current view, Properties" },

    { "Models panel",    "Left click on a name",  "Select the model" },
};

inline const char* CanvasKeyName(CanvasKey key)
{
    switch (key)
    {
    case CanvasKey::F:    return "F";
    case CanvasKey::Up:   return "Up";
    case CanvasKey::Down: return "Down";
    }
    return "?";
}

// « Ctrl + Alt + Shift + Key » : même séparateur que les gestes souris, pour
// que les touches, les gestes et les accélérateurs de menus se lisent pareil.
inline std::string FormatChord(unsigned modifiers, std::string_view key)
{
    std::string out;
    const auto add = [&out](std::string_view part) {
        if (!out.empty())
            out += " + ";
        out += part;
    };
    if (modifiers & modifier::Ctrl)  add("Ctrl");
    if (modifiers & modifier::Alt)   add("Alt");
    if (modifiers & modifier::Shift) add("Shift");
    add(key);
    return out;
}

inline std::optional<CanvasCommand> FindCanvasCommand(CanvasKey key, unsigned modifiers)
{
    for (const CanvasKeyBinding& b : kCanvasKeyBindings)
        if (b.key == key && b.modifiers == modifiers)
            return b.command;
    return std::nullopt;
}

} // namespace sinaia
