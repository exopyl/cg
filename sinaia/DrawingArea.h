#pragma once

#include <wx/control.h>
#include <wx/effects.h>
#include <wx/dcclient.h>

//
//
//
class wxDrawingArea : wxControl
{
public:
	wxDrawingArea () {};
	wxDrawingArea (wxWindow *parent)
        : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           /*wxHSCROLL | wxVSCROLL |*/ wxNO_FULL_REPAINT_ON_RESIZE)
	{
		m_pParent = parent;
	}

	virtual void SetSize (int width, int height)
	{
		wxControl::SetSize (width, height);
	}

	void OnMotion (wxMouseEvent& event)
	{
		if (event.Moving())
//		if (event.Dragging())
		{
			wxClientDC dc(this);
			wxPoint pos = event.GetPosition ();
			wxSize size = GetClientSize ();

			// erase all
			dc.SetPen (*wxWHITE_PEN);
			dc.DrawRectangle (0, 0, size.x, size.y);

			// draw a vertical line
			dc.SetPen (*wxBLACK_PEN);
			dc.DrawLine (pos.x, 0, pos.x, size.y);

			// draw the rectangle in which we write tex
			dc.SetPen (*wxRED_PEN);
			dc.DrawRectangle (0, 0, 70, 30);

			wxString str;// = "eee";
			str = str.Format ((wxChar*)"%d x %d", pos.x, pos.y);

			// write the text
			dc.SetPen (*wxBLACK_PEN);
			dc.DrawText (str, 0, 6);


			dc.SetPen (wxNullPen);
		}
	}

	void OnEraseBackground (wxEraseEvent& event)
	{
		wxClientDC* clientDC = NULL;
		if (!event.GetDC())
			clientDC = new wxClientDC (this);

		wxDC* dc = clientDC ? clientDC : event.GetDC();

		wxSize size = GetClientSize ();

		dc->SetPen (*wxWHITE_PEN);
		dc->DrawRectangle (0, 0, size.x, size.y);


		if (clientDC)
			delete clientDC;
	}

	void OnPaint (wxPaintEvent& event)
	{
		wxPaintDC dc(this);

		dc.SetPen (*wxBLACK_PEN);
		dc.SetBrush (*wxRED_BRUSH);

		wxSize size = GetClientSize ();
		wxCoord w = 100, h = 50;

		int x = wxMax (0, (size.x-w/2));
		int y = wxMax (0, (size.y-h/2));

		wxRect rectToDraw (x, y, w, h);
		if (IsExposed (rectToDraw))
		{
			int r = 20;

			dc.SetPen( *wxRED_PEN );
			dc.SetBrush( *wxGREEN_BRUSH );

			dc.DrawText(_T("Some circles"), 0, y);
			dc.DrawCircle(x, y, r);
			dc.DrawCircle(x + 2*r, y, r);
			dc.DrawCircle(x + 4*r, y, r);
		}
	}

private:
	wxWindow *m_pParent;

	DECLARE_EVENT_TABLE()
};
