// ---------------------------------------------------------------------------
//  --drag-splitter : LE REDIMENSIONNEMENT D'UN PANNEAU, MESURE DE BOUT EN BOUT
// ---------------------------------------------------------------------------
//
// Quatre temps, chacun par la file d'evenements et par des images REELLEMENT
// rendues -- donc par l'arbitre, par RmlUi, par Enforce et par la phase 3,
// exactement comme une souris et une fenetre :
//
//   0. TEMOIN : un glisser commence DANS la vue et qui traverse le separateur.
//      La camera DOIT tourner et le panneau NE DOIT PAS changer. Sans ce temps,
//      « la camera n'a pas tourne » serait vrai aussi d'une camera inerte.
//   A. Le glisser du separateur, de <delta> pixels. Le panneau prend l'etendue
//      attendue, bornee ; la vue suit ; la camera ne tourne pas.
//   B. La fenetre est reduite jusqu'a ce que la vue n'ait plus son minimum,
//      puis rendue a sa taille : le panneau cede, puis retrouve son etendue.
//   C. Un double-clic sur le separateur rend la taille de la feuille de style,
//      meme suivi d'une secousse d'un pixel bouton tenu.
//
// `pair` : les panneaux gauche ET droit sont tires, puis la fenetre est reduite
// en trois paliers -- le manque doit etre pris au-dessus des minimums, gauche
// d'abord, avant qu'aucun panneau ne descende sous le sien.
//
// ⚠ LES ETENDUES ATTENDUES SONT RECALCULEES ICI DEPUIS LES DEUX MINIMUMS, SANS
// APPELER core::ClampPanelExtent. Un instrument qui reprendrait la fonction du
// produit serait d'accord avec elle quelle qu'elle soit.

#include "application.h"
#include "application_internal.h"

#include "view3d/camera.h"
#include "input_router.h"
#include "panel_splitters.h"
#include "rml_host.h"
#include "splitter_bounds.h"
#include "viewport_rect.h"
#include "window_glfw.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace peles
{
namespace app
{

namespace
{

struct SplitGeometry
{
	ui::ViewportRect panel;
	ui::ViewportRect view;
	ui::ViewportRect splitter;
};

int Along (const ui::ViewportRect& rect, bool horizontal)
{
	return horizontal ? rect.width : rect.height;
}

bool SameRect (const ui::ViewportRect& a, const ui::ViewportRect& b)
{
	return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

std::string RectText (const ui::ViewportRect& r)
{
	char text[64];
	std::snprintf (text, sizeof (text), "x=%d y=%d w=%d h=%d", r.x, r.y, r.width, r.height);
	return text;
}

double AngleDeg (const float a[3], const float b[3])
{
	const float dot = std::max (-1.f, std::min (1.f, a[0] * b[0] + a[1] * b[1] + a[2] * b[2]));
	return std::acos ((double)dot) * 180. / 3.14159265358979;
}

const char* CursorText (platform::CursorShape shape)
{
	switch (shape)
	{
	case platform::CursorShape::ResizeHorizontal: return "double fleche horizontale";
	case platform::CursorShape::ResizeVertical:   return "double fleche verticale";
	case platform::CursorShape::Text:             return "barre de texte";
	case platform::CursorShape::Hand:             return "main";
	case platform::CursorShape::Crosshair:        return "croix";
	case platform::CursorShape::Arrow:            break;
	}
	return "fleche (defaut)";
}

// L'ETENDUE ATTENDUE, ECRITE DEPUIS LA REGLE ET NON DEPUIS LE CODE : le panneau
// est porte au minimum de panneau, ramene sous l'etendue partagee moins le
// minimum de la vue, jamais negatif.
int ExpectedExtent (int requested, int shared)
{
	int extent = requested < core::kMinPanelExtent ? core::kMinPanelExtent : requested;
	if (extent > shared - core::kMinViewportExtent)
		extent = shared - core::kMinViewportExtent;
	return extent < 0 ? 0 : extent;
}

ui::MouseEvent Mouse (ui::MouseEvent::Kind kind, int x, int y)
{
	ui::MouseEvent event;
	event.kind    = kind;
	event.button  = ui::MouseButton::Left;
	event.windowX = x;
	event.windowY = y;
	return event;
}

} // namespace

// ---------------------------------------------------------------------------
//  LA SONDE, UN TEMPS PAR METHODE
// ---------------------------------------------------------------------------
//
// Classe imbriquee d'Application : elle a acces a la file d'evenements, a
// l'arbitre et aux images, sans que application.h ne publie un membre par
// temps de la mesure. Les manquements s'accumulent dans `m_failures` ; un temps
// qui ne peut pas conclure rend false et la mesure s'arrete la.
class Application::SplitterProbe
{
public:
	SplitterProbe (Application& host, int index)
	    : m_host (host), m_index (index), m_spec (ui::PanelSplitters::Specs ()[index]),
	      m_horizontal (m_spec.axis == ui::PanelSplitters::Axis::Horizontal),
	      m_growth (m_spec.side == core::SplitterSide::PanelBefore ? 1 : -1),
	      m_frameWidth (host.m_framebufferWidth), m_frameHeight (host.m_framebufferHeight)
	{
	}

	bool Start ();
	bool Witness ();
	void Drag (int delta);
	bool ShrinkAndRestore ();
	void Reset ();

	// Code de sortie : 0 si ni cette sonde ni `other` n'ont releve de
	// manquement.
	int Verdict (const SplitterProbe* other = nullptr) const;

	// --drag-splitter pair : les deux panneaux de l'axe horizontal, deja tires,
	// puis la fenetre reduite en trois paliers. Les manquements vont a `left`.
	static bool ShrinkPair (SplitterProbe& left, SplitterProbe& right);

private:
	// CE QUE LE GLISSER A PRODUIT, releve par DragGesture et juge par
	// ReportDrag : le geste et son verdict sont deux fonctions.
	struct DragMeasure
	{
		int                   xs = 0, ys = 0, xe = 0, ye = 0;
		std::string           hover;
		std::string           cursorName;
		platform::CursorShape cursorShape = platform::CursorShape::Arrow;
		bool                  ownerUi     = false;
		int                   drags       = 0;
		int                   wanted      = 0;
		int                   expected    = 0;
		int                   shared      = 0;
		ui::ViewportRect      expectedView;
		bool                  cameraStill = false;
	};

	DragMeasure DragGesture (int delta);
	void        ReportDrag (const DragMeasure& m);

	bool Read (SplitGeometry& g) const
	{
		Rml::Element* const root = m_host.m_rml->DocumentRoot ();
		return ui::ReadViewportRect (root, m_spec.panelId, g.panel) ==
		           ui::ViewportRectStatus::Ok &&
		       ui::ReadViewportRect (root, kViewportElementId, g.view) ==
		           ui::ViewportRectStatus::Ok &&
		       ui::ReadViewportRect (root, m_spec.splitterId, g.splitter) ==
		           ui::ViewportRectStatus::Ok;
	}

	// Un point de l'axe du separateur, `offset` pixels au-dela de son milieu.
	// L'autre coordonnee est le milieu de la vue, que le separateur borde.
	void Point (const SplitGeometry& g, int offset, int& x, int& y) const
	{
		if (m_horizontal)
		{
			x = g.splitter.x + g.splitter.width / 2 + offset;
			y = g.view.y + g.view.height / 2;
		}
		else
		{
			x = g.view.x + g.view.width / 2;
			y = g.splitter.y + g.splitter.height / 2 + offset;
		}
	}

	void Queue (ui::MouseEvent::Kind kind, int x, int y)
	{
		m_host.m_pendingEvents.push_back (Mouse (kind, x, y));
	}

	int Extent (const ui::ViewportRect& r) const { return Along (r, m_horizontal); }

	void RecordCamera ()
	{
		m_host.m_camera->GetViewAxis (m_axis);
		m_host.m_camera->GetPivot (m_pivot);
	}

	bool CameraAsRecorded () const
	{
		float axis[3] = {}, pivot[3] = {};
		m_host.m_camera->GetViewAxis (axis);
		m_host.m_camera->GetPivot (pivot);
		return std::memcmp (axis, m_axis, sizeof (axis)) == 0 &&
		       std::memcmp (pivot, m_pivot, sizeof (pivot)) == 0;
	}

	void Fail (const char* what) { m_failures.emplace_back (what); }

	Application&                    m_host;
	int                             m_index;
	const ui::PanelSplitters::Spec& m_spec;
	bool                            m_horizontal;
	// +1 : le panneau grandit quand le pointeur avance ; -1 : quand il recule.
	int m_growth;
	int m_frameWidth;
	int m_frameHeight;

	SplitGeometry m_g0; // au debut de la sonde
	SplitGeometry m_g1; // apres le glisser

	// Orientation de reference de la camera : prise par Start, reprise APRES
	// le temoin. Aucun temps suivant ne doit la changer.
	float m_axis[3]  = {};
	float m_pivot[3] = {};

	std::vector<std::string> m_failures;
};

bool Application::SplitterProbe::Start ()
{
	if (!Read (m_g0))
	{
		std::fprintf (stderr, "peles : #%s, #%s ou #%s n'a pas de geometrie. MESURE NON "
		                      "CONCLUANTE.\n",
		              m_spec.panelId, kViewportElementId, m_spec.splitterId);
		return false;
	}
	RecordCamera ();
	std::printf ("\n--- separateur de panneau : %s (#%s -> #%s) ---\n", m_spec.name,
	             m_spec.splitterId, m_spec.panelId);
	std::printf ("cadre                      : %dx%d\n", m_frameWidth, m_frameHeight);
	std::printf ("separateur                 : %s\n", RectText (m_g0.splitter).c_str ());
	std::printf ("panneau, vue au depart     : %d px, %s\n", Extent (m_g0.panel),
	             RectText (m_g0.view).c_str ());
	return true;
}

// --- 0. Le temoin : un glisser parti de la vue --------------------------------
bool Application::SplitterProbe::Witness ()
{
	float axisBefore[3] = {};
	m_host.m_camera->GetViewAxis (axisBefore);

	// La vue est du cote vers lequel le panneau grandit : le temoin part de
	// 30 px dans la vue et finit 60 px dans le panneau.
	int x0 = 0, y0 = 0, xm = 0, ym = 0, x1 = 0, y1 = 0;
	Point (m_g0, 30 * m_growth, x0, y0);
	Point (m_g0, 0, xm, ym);
	Point (m_g0, -60 * m_growth, x1, y1);
	const int dragsBefore = m_host.m_splitters->DragEvents ();

	Queue (ui::MouseEvent::Kind::Move, x0, y0);
	Queue (ui::MouseEvent::Kind::Press, x0, y0);
	Queue (ui::MouseEvent::Kind::Move, xm, ym);
	Queue (ui::MouseEvent::Kind::Move, x1, y1);
	Queue (ui::MouseEvent::Kind::Release, x1, y1);
	m_host.ProcessInput ();
	const bool ownerView = m_host.m_lastPressOwner == ui::InputRouter::Owner::Viewport;
	m_host.RenderFrame (nullptr, nullptr);

	SplitGeometry g;
	Read (g);
	m_host.m_camera->GetViewAxis (m_axis);
	m_host.m_camera->GetPivot (m_pivot);
	const double rotation = AngleDeg (axisBefore, m_axis);
	const bool   still    = SameRect (g.panel, m_g0.panel) && SameRect (g.view, m_g0.view) &&
	                   m_host.m_splitters->DragEvents () == dragsBefore;

	std::printf ("temoin, parti de la vue    : (%d,%d) -> (%d,%d), proprietaire %s, rotation "
	             "%.4f deg, panneau et vue %s\n",
	             x0, y0, x1, y1, ownerView ? "vue" : "AUTRE", rotation,
	             still ? "inchanges" : "CHANGES");
	if (!ownerView || rotation < 1.)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : le glisser temoin n'a pas fait tourner la camera "
		              "(%.4f deg, proprietaire %s). « La camera n'a pas tourne » ne prouverait "
		              "rien.\n",
		              rotation, ownerView ? "vue" : "autre");
		return false;
	}
	if (!still)
		Fail ("un glisser commence dans la vue a redimensionne le panneau");
	return true;
}


// --- A. Le glisser du separateur ----------------------------------------------
void Application::SplitterProbe::Drag (int delta)
{
	ReportDrag (DragGesture (delta));
}

Application::SplitterProbe::DragMeasure Application::SplitterProbe::DragGesture (int delta)
{
	DragMeasure m;
	int         xh = 0, yh = 0;
	Point (m_g0, 0, m.xs, m.ys);
	Point (m_g0, delta / 2, xh, yh);
	Point (m_g0, delta, m.xe, m.ye);

	// Le survol et le curseur sont releves APRES le seul deplacement, avant
	// l'enfoncement : c'est l'etat que voit l'utilisateur quand il vise.
	Queue (ui::MouseEvent::Kind::Move, m.xs, m.ys);
	m_host.ProcessInput ();
	m.hover       = m_host.m_input->HoverName ();
	m.cursorName  = m_host.m_rml->LastCursorName ();
	m.cursorShape = m_host.m_window->CurrentCursor ();

	const int dragsBefore = m_host.m_splitters->DragEvents ();
	Queue (ui::MouseEvent::Kind::Press, m.xs, m.ys);
	Queue (ui::MouseEvent::Kind::Move, xh, yh);
	Queue (ui::MouseEvent::Kind::Move, m.xe, m.ye);
	Queue (ui::MouseEvent::Kind::Release, m.xe, m.ye);
	m_host.ProcessInput ();
	m.ownerUi = m_host.m_lastPressOwner == ui::InputRouter::Owner::Ui;
	m.drags   = m_host.m_splitters->DragEvents () - dragsBefore;
	m_host.RenderFrame (nullptr, nullptr);
	Read (m_g1);

	const int p0 = Extent (m_g0.panel);
	m.shared     = p0 + Extent (m_g0.view);
	m.wanted     = p0 + m_growth * delta;
	m.expected   = ExpectedExtent (m.wanted, m.shared);

	// LA VUE ATTENDUE : elle perd ce que le panneau gagne, et son origine ne
	// bouge que si le panneau la precede.
	const int moved = Extent (m_g1.panel) - p0;
	m.expectedView  = m_g0.view;
	if (m_horizontal)
	{
		m.expectedView.width -= moved;
		if (m_spec.side == core::SplitterSide::PanelBefore)
			m.expectedView.x += moved;
	}
	else
	{
		m.expectedView.height -= moved;
	}
	m.cameraStill = CameraAsRecorded ();
	return m;
}

void Application::SplitterProbe::ReportDrag (const DragMeasure& m)
{
	const std::string           wantedHover = std::string ("#") + m_spec.splitterId;
	const platform::CursorShape wantedShape = m_horizontal
	                                              ? platform::CursorShape::ResizeHorizontal
	                                              : platform::CursorShape::ResizeVertical;
	const int p1 = Extent (m_g1.panel);

	std::printf ("survol au depart           : %s (attendu %s)\n", m.hover.c_str (),
	             wantedHover.c_str ());
	std::printf ("curseur                    : \"%s\" -> %s (attendu %s)\n", m.cursorName.c_str (),
	             CursorText (m.cursorShape), CursorText (wantedShape));
	std::printf ("glisser                    : (%d,%d) -> (%d,%d), proprietaire %s, %d "
	             "evenement(s) drag\n",
	             m.xs, m.ys, m.xe, m.ye, m.ownerUi ? "interface" : "AUTRE", m.drags);
	std::printf ("panneau                    : %d -> %d px (attendu %d, %s ; bornes [%d, %d])\n",
	             Extent (m_g0.panel), p1, m.expected, m.expected == m.wanted ? "libre" : "BORNE",
	             core::kMinPanelExtent, m.shared - core::kMinViewportExtent);
	std::printf ("vue                        : %s (attendu %s)\n", RectText (m_g1.view).c_str (),
	             RectText (m.expectedView).c_str ());
	std::printf ("camera                     : axe et pivot %s\n",
	             m.cameraStill ? "INCHANGES au bit pres" : "CHANGES");

	if (m.hover != wantedHover)
		Fail ("le pointeur pose sur le separateur ne le survole pas");
	if (m.cursorShape != wantedShape)
		Fail ("le curseur de redimensionnement n'est pas pose");
	if (!m.ownerUi)
		Fail ("l'enfoncement sur le separateur n'est pas alle a l'interface");
	if (m.drags <= 0)
		Fail ("aucun evenement `drag` n'a ete traite");
	if (p1 != m.expected)
		Fail ("l'etendue du panneau n'est pas celle attendue");
	if (!SameRect (m_g1.view, m.expectedView))
		Fail ("le rectangle de la vue n'a pas suivi le panneau");
	if (!SameRect (m_host.m_viewRect, m_g1.view))
		Fail ("le rectangle lu par la phase 3 differe de la geometrie du document");
	if (!m.cameraStill)
		Fail ("la camera a bouge pendant le glisser du separateur");
}

// --- B. La fenetre reduite, puis rendue ----------------------------------------
//
// La reduction retire a la vue tout ce qui depasse son minimum, plus 40 px : le
// panneau DOIT alors ceder ces 40 px -- ou descendre sous son propre minimum si
// l'etendue partagee ne tient plus les deux.
bool Application::SplitterProbe::ShrinkAndRestore ()
{
	const int shrink = Extent (m_g1.view) - core::kMinViewportExtent + 40;
	const int width  = m_horizontal ? m_frameWidth - shrink : m_frameWidth;
	const int height = m_horizontal ? m_frameHeight : m_frameHeight - shrink;
	m_host.m_window->SetSize (width, height);
	m_host.RenderFrame (nullptr, nullptr);

	SplitGeometry g2;
	Read (g2);
	const bool obtained =
	    m_host.m_framebufferWidth == width && m_host.m_framebufferHeight == height;
	const int shared   = Extent (m_g1.panel) + Extent (m_g1.view) - shrink;
	const int expected = ExpectedExtent (Extent (m_g1.panel), shared);
	const int p2       = Extent (g2.panel);
	const int v2       = Extent (g2.view);
	std::printf ("fenetre reduite            : %dx%d demande, %dx%d obtenu -> panneau %d "
	             "(attendu %d), vue %d (minimum %d)\n",
	             width, height, m_host.m_framebufferWidth, m_host.m_framebufferHeight, p2,
	             expected, v2, core::kMinViewportExtent);

	m_host.m_window->SetSize (m_frameWidth, m_frameHeight);
	m_host.RenderFrame (nullptr, nullptr);
	SplitGeometry g3;
	Read (g3);
	const bool restored = m_host.m_framebufferWidth == m_frameWidth &&
	                      m_host.m_framebufferHeight == m_frameHeight;
	std::printf ("fenetre rendue             : %dx%d -> panneau %d (attendu %d), vue %s\n",
	             m_host.m_framebufferWidth, m_host.m_framebufferHeight, Extent (g3.panel),
	             Extent (m_g1.panel), RectText (g3.view).c_str ());

	if (!obtained || !restored)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : le tampon n'a pas pris la taille demandee (ecran "
		              "trop petit ou facteur d'echelle different de 1).\n");
		return false;
	}
	if (p2 != expected)
		Fail ("la reduction de la fenetre n'a pas reapplique les bornes du panneau");
	if (v2 < core::kMinViewportExtent && shared >= core::kMinViewportExtent)
		Fail ("la vue est passee sous son minimum a la reduction de la fenetre");
	if (!SameRect (g3.panel, m_g1.panel) || !SameRect (g3.view, m_g1.view))
		Fail ("la fenetre rendue a sa taille n'a pas rendu au panneau son etendue");
	return true;
}


// --- C. Le double-clic, suivi d'une secousse ----------------------------------------
//
// ⚠ LA SECOUSSE D'UN PIXEL, BOUTON TENU APRES LE SECOND ENFONCEMENT, FAIT PARTIE
// DU GESTE MESURE. Elle demarre un glisser dans le MEME lot d'evenements que la
// remise a zero, donc avant que la mise en page ne l'ait prise en compte : un
// produit qui lirait l'etendue a ce moment reposerait l'ancienne.
void Application::SplitterProbe::Reset ()
{
	SplitGeometry g;
	Read (g);
	int x = 0, y = 0;
	Point (g, 0, x, y);
	const int jx = m_horizontal ? x + 1 : x;
	const int jy = m_horizontal ? y : y + 1;

	const int resetsBefore = m_host.m_splitters->Resets ();
	Queue (ui::MouseEvent::Kind::Move, x, y);
	Queue (ui::MouseEvent::Kind::Press, x, y);
	Queue (ui::MouseEvent::Kind::Release, x, y);
	Queue (ui::MouseEvent::Kind::Press, x, y);
	Queue (ui::MouseEvent::Kind::Move, jx, jy);
	Queue (ui::MouseEvent::Kind::Release, jx, jy);
	m_host.ProcessInput ();
	m_host.RenderFrame (nullptr, nullptr);

	SplitGeometry g4;
	Read (g4);
	std::printf ("double-clic + secousse 1px : (%d,%d) -> panneau %d (defaut %d), vue %s, %d "
	             "remise(s) a zero\n",
	             x, y, Extent (g4.panel), Extent (m_g0.panel), RectText (g4.view).c_str (),
	             m_host.m_splitters->Resets () - resetsBefore);

	if (!SameRect (g4.panel, m_g0.panel) || !SameRect (g4.view, m_g0.view) ||
	    m_host.m_splitters->RequestedExtent (m_index) != -1)
		Fail ("le double-clic n'a pas rendu la taille de la feuille de style");
	if (!CameraAsRecorded ())
		Fail ("la camera a bouge pendant le double-clic");
}

namespace
{

// LES DEUX PANNEAUX D'UN AXE, ECRITS DEPUIS LA REGLE ET NON DEPUIS LE CODE :
// chacun porte a son minimum, puis le manque pris au-dessus des minimums --
// le gauche d'abord --, puis sous les minimums, dans le meme ordre.
void ExpectedPair (int left, int right, int shared, int& expectedLeft, int& expectedRight)
{
	expectedLeft  = std::max (left, core::kMinPanelExtent);
	expectedRight = std::max (right, core::kMinPanelExtent);
	int  missing  = expectedLeft + expectedRight + core::kMinViewportExtent - shared;
	auto take     = [&missing] (int& extent, int floor)
	{
		const int given = std::max (0, std::min (missing, extent - floor));
		extent -= given;
		missing -= given;
	};
	take (expectedLeft, core::kMinPanelExtent);
	take (expectedRight, core::kMinPanelExtent);
	take (expectedLeft, 0);
	take (expectedRight, 0);
}

} // namespace

// --- --drag-splitter pair : deux panneaux du meme axe, fenetre reduite ---------
//
// Trois paliers, choisis sur les etendues obtenues : un manque que le panneau
// gauche absorbe seul, un manque qui le porte a son minimum et entame le droit,
// un manque qui depasse la somme des deux marges et les fait passer sous leur
// minimum.
bool Application::SplitterProbe::ShrinkPair (SplitterProbe& left, SplitterProbe& right)
{
	Application&  host = left.m_host;
	SplitGeometry gl, gr;
	left.Read (gl);
	right.Read (gr);
	const int l1 = gl.panel.width, r1 = gr.panel.width, v1 = gl.view.width;
	const int slack = v1 - core::kMinViewportExtent;
	if (l1 - core::kMinPanelExtent < 40 || r1 - core::kMinPanelExtent < 40 || slack < 0)
	{
		std::fprintf (stderr, "MESURE NON CONCLUANTE : panneaux %d et %d, vue %d -- les trois "
		                      "paliers ne sont pas distincts. Tirer davantage.\n",
		              l1, r1, v1);
		return false;
	}

	const int missing[3] = { 40, l1 - core::kMinPanelExtent + 40,
		                     l1 + r1 - 2 * core::kMinPanelExtent + 40 };
	for (const int miss : missing)
	{
		const int width = left.m_frameWidth - slack - miss;
		host.m_window->SetSize (width, left.m_frameHeight);
		host.RenderFrame (nullptr, nullptr);
		left.Read (gl);
		right.Read (gr);
		if (host.m_framebufferWidth != width || host.m_framebufferHeight != left.m_frameHeight)
		{
			std::fprintf (stderr, "MESURE NON CONCLUANTE : le tampon n'a pas pris %dx%d.\n", width,
			              left.m_frameHeight);
			return false;
		}
		const int shared = l1 + r1 + v1 - slack - miss;
		int       el = 0, er = 0;
		ExpectedPair (l1, r1, shared, el, er);
		std::printf ("fenetre %4dx%d (manque %3d) : gauche %d (attendu %d), droit %d (attendu %d), "
		             "vue %d\n",
		             width, left.m_frameHeight, miss, gl.panel.width, el, gr.panel.width, er,
		             gl.view.width);
		if (gl.panel.width != el || gr.panel.width != er)
			left.Fail ("les deux panneaux de l'axe n'ont pas cede dans l'ordre et jusqu'aux bornes "
			           "de la regle");
		if (gl.view.width < core::kMinViewportExtent)
			left.Fail ("la vue est passee sous son minimum");
	}

	host.m_window->SetSize (left.m_frameWidth, left.m_frameHeight);
	host.RenderFrame (nullptr, nullptr);
	left.Read (gl);
	right.Read (gr);
	std::printf ("fenetre rendue             : gauche %d (attendu %d), droit %d (attendu %d)\n",
	             gl.panel.width, l1, gr.panel.width, r1);
	if (gl.panel.width != l1 || gr.panel.width != r1)
		left.Fail ("la fenetre rendue n'a pas rendu aux deux panneaux leur etendue");
	return true;
}

int Application::SplitterProbe::Verdict (const SplitterProbe* other) const
{
	std::vector<std::string> failures = m_failures;
	if (other != nullptr)
		failures.insert (failures.end (), other->m_failures.begin (), other->m_failures.end ());

	std::printf ("\n");
	if (!failures.empty ())
	{
		for (const std::string& what : failures)
			std::fprintf (stderr, "SEPARATEUR ECHOUE : %s.\n", what.c_str ());
		return 1;
	}
	std::printf ("SEPARATEUR CONFORME : etendue attendue, vue qui suit, camera immobile, bornes "
	             "tenues a la reduction de la fenetre%s.\n",
	             other != nullptr ? " pour les deux panneaux de l'axe"
	                              : ", taille par defaut au double-clic");
	return 0;
}

// ---------------------------------------------------------------------------

int Application::RunDragSplitter ()
{
	const bool pair  = m_options.splitterName == "pair";
	const int  index = ui::PanelSplitters::IndexOf (pair ? "left" : m_options.splitterName.c_str ());
	if (index < 0)
		return 2;

	const int delta = m_options.splitterDelta;
	if (delta > -2 && delta < 2)
	{
		std::fprintf (stderr, "peles : un deplacement de %d px ne demarre aucun glisser. MESURE "
		                      "NON CONCLUANTE.\n",
		              delta);
		return 1;
	}

	if (!RenderFrame (nullptr, nullptr))
	{
		std::fprintf (stderr, "peles : aucune image rendue. MESURE NON CONCLUANTE.\n");
		return 1;
	}

	SplitterProbe probe (*this, index);
	if (pair)
	{
		// LES DEUX PANNEAUX GRANDISSENT de `delta`. Le droit est sonde APRES le
		// glisser du gauche : l'etendue qu'il partage avec la vue en depend.
		SplitterProbe right (*this, ui::PanelSplitters::IndexOf ("right"));
		if (!probe.Start ())
			return 1;
		probe.Drag (delta);
		if (!right.Start ())
			return 1;
		right.Drag (-delta);
		if (!SplitterProbe::ShrinkPair (probe, right))
			return 1;
		return probe.Verdict (&right);
	}

	if (!probe.Start () || !probe.Witness ())
		return 1;
	probe.Drag (delta);
	if (!probe.ShrinkAndRestore ())
		return 1;
	probe.Reset ();
	return probe.Verdict ();
}

} // namespace app
} // namespace peles
