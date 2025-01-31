#include "DrawingArea.h"

BEGIN_EVENT_TABLE(wxDrawingArea, wxWindow)
    EVT_MOTION(wxDrawingArea::OnMotion)
    EVT_ERASE_BACKGROUND(wxDrawingArea::OnEraseBackground)
    EVT_PAINT(wxDrawingArea::OnPaint)
END_EVENT_TABLE()