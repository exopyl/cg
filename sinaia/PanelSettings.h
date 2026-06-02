#pragma once

//
// Appearance options for the application's notebook panels, edited via the
// "2D Panel" tab of the Settings dialog. These used to live in the
// "Options > 3D Panel" menu and drive the AUI notebook style flags and the
// tab art theme.
//
// notebookStyle is a bitwise OR of wxAUI_NB_* flags (including the tab
// alignment bits wxAUI_NB_TOP / wxAUI_NB_BOTTOM). notebookTheme selects the
// tab art provider: 0 = glossy (default), 1 = simple.
//
// Note: the FPS overlay and the 3D view background colour remain in the
// "Options > 3D Panel" menu (they are properties of the 3D view, not of the
// notebook), so they are deliberately not part of this struct.
//
struct PanelSettings
{
    long notebookStyle = 0;       // wxAUI_NB_* flags applied to all notebooks
    int  notebookTheme = 0;       // 0 = glossy (default), 1 = simple
};
