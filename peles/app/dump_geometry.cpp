// ---------------------------------------------------------------------------
//  LA GEOMETRIE : --dump-layout et --dump-pick
// ---------------------------------------------------------------------------
//
// Les deux mesurent une GEOMETRIE et rien d'autre -- le rectangle resolu d'un
// element, le rayon monde d'un pixel. Ni clavier, ni souris, ni etat GL.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

#include "dump_modes.h"
#include "view3d/camera.h"
#include "viewport_rect.h"
#include "rml_host.h"

#include <RmlUi/Core/Box.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Types.h>

#include <cmath>
#include <cstdio>

namespace peles
{
namespace app
{

namespace
{

// Rayon du pixel FENETRE `(windowX, windowY)`, par la conversion partagee.
bool RayFromWindowPixel (const ui::ViewportRect& rect, view3d::Camera& camera, float windowX,
                         float windowY, float origin[3], float direction[3])
{
	float viewX = 0.f;
	float viewY = 0.f;
	ui::WindowToView (rect, windowX, windowY, viewX, viewY);
	return view3d::ScreenToRay (camera, viewX, viewY, origin, direction);
}

float CrossNorm (const float a[3], const float b[3])
{
	const float cross[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
		                     a[0] * b[1] - a[1] * b[0] };
	return std::sqrt (cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2]);
}

// ⚠ SEUIL DU CRITERE, ET IL N'EST PAS UNE ESTIMATION. Les deux vecteurs sont
// colineaires PAR CONSTRUCTION quand la conversion est juste -- le NDC du pixel
// central vaut (0,0), donc la direction en espace oeil vaut exactement
// (0, 0, -1). Ce qui reste est l'arrondi de la simple precision sur la rotation,
// de l'ordre de 1e-7. Le seuil est deux ordres de grandeur au-dessus.
const float kCollinearTolerance = 1e-5f;

} // namespace

// ---------------------------------------------------------------------------
//  --dump-layout et --dump-timing
// ---------------------------------------------------------------------------

int DumpLayout (platform::RmlHost& rml, const char* elementId, int width, int height)
{
	// LA TAILLE EST IMPOSEE AU CONTEXTE, PAS PRISE SUR LA FENETRE : le rectangle
	// mesure est celui que la feuille de style prescrit pour un cadre de cette
	// taille, independamment de la fenetre que le systeme a bien voulu donner.
	rml.SetDimensions (width, height);
	rml.Update ();

	ui::ViewportRect             rect;
	const ui::ViewportRectStatus status = ui::ReadViewportRect (rml.DocumentRoot (), elementId,
	                                                            rect);

	std::printf ("\n--- rectangle de la vue 3D ---\n");
	std::printf ("cadre                    : %dx%d\n", width, height);

	if (status == ui::ViewportRectStatus::ElementMissing)
	{
		std::fprintf (stderr, "peles : aucun element d'identifiant \"%s\" dans le document RML.\n",
		              elementId);
		return 3;
	}
	if (status == ui::ViewportRectStatus::DegenerateSize)
	{
		std::fprintf (stderr,
		              "peles : l'element \"%s\" n'occupe aucune surface pour ce cadre : il n'y "
		              "a pas de rectangle a rendre.\n",
		              elementId);
		return 1;
	}

	const ui::GlViewport view = ui::ToGlViewport (rect, height);
	std::printf ("RmlUi (origine en haut)  : x=%d y=%d w=%d h=%d\n", rect.x, rect.y, rect.width,
	             rect.height);
	std::printf ("GL    (origine en bas)   : x=%d y=%d w=%d h=%d\n", view.x, view.y, view.width,
	             view.height);
	return 0;
}

// ---------------------------------------------------------------------------
//  --dump-pick
// ---------------------------------------------------------------------------


int DumpPick (const ui::ViewportRect& rect, view3d::Camera& camera, int frameWidth,
              int frameHeight, int pixelX, int pixelY)
{
	// ⚠ AUCUN SetViewSize ICI. Les dimensions lues sont celles que la phase 5 de
	// l'image rendue a posees ; les poser ici reviendrait a mesurer
	// l'instrument.
	std::printf ("\n--- rayon de picking ---\n");
	std::printf ("cadre                    : %dx%d\n", frameWidth, frameHeight);
	std::printf ("rectangle de la vue      : x=%d y=%d w=%d h=%d\n", rect.x, rect.y, rect.width,
	             rect.height);
	std::printf ("set_dimensions (PRODUIT) : (%d, %d)\n", camera.ViewWidth (),
	             camera.ViewHeight ());

	// PREMIER VERDICT : CE QUE LE PRODUIT A POSE EST CE QUE LE RECTANGLE DIT.
	//
	// ⚠ IL EST RENDU AVANT LE SECOND, ET IL N'EST PAS REDONDANT AVEC LUI. Une
	// hauteur fausse laisse le rayon CENTRAL colineaire a l'axe de vue -- le NDC
	// du centre vaut (0,0) quelles que soient les dimensions -- et ne se voit
	// que sur les pixels hors du centre. Le second verdict seul ne peut donc pas
	// echouer sur ce mode de panne.
	if (camera.ViewWidth () != rect.width || camera.ViewHeight () != rect.height)
	{
		std::fprintf (stderr,
		              "DIMENSIONS DE LA CAMERA FAUSSES : le produit a pose (%d, %d) la ou le "
		              "rectangle de la vue vaut (%d, %d). Le rapport d'aspect et le "
		              "denominateur du picking en dependent tous les deux.\n",
		              camera.ViewWidth (), camera.ViewHeight (), rect.width, rect.height);
		return 1;
	}

	float axis[3] = { 0.f, 0.f, 0.f };
	camera.GetViewAxis (axis);
	float pivot[3] = { 0.f, 0.f, 0.f };
	camera.GetPivot (pivot);
	std::printf ("axe de vue               : (%.6f, %.6f, %.6f)\n", axis[0], axis[1], axis[2]);
	std::printf ("pivot                    : (%.6f, %.6f, %.6f)\n", pivot[0], pivot[1], pivot[2]);

	float origin[3]    = { 0.f, 0.f, 0.f };
	float direction[3] = { 0.f, 0.f, 0.f };
	if (!RayFromWindowPixel (rect, camera, (float)pixelX, (float)pixelY, origin, direction))
	{
		std::fprintf (stderr, "peles : la vue n'a pas de dimensions exploitables.\n");
		return 1;
	}

	float demandedViewX = 0.f;
	float demandedViewY = 0.f;
	ui::WindowToView (rect, (float)pixelX, (float)pixelY, demandedViewX, demandedViewY);
	std::printf ("pixel demande (FENETRE)  : (%d, %d)\n", pixelX, pixelY);
	std::printf ("             (VUE)       : (%.1f, %.1f)\n", demandedViewX, demandedViewY);
	std::printf ("origine du rayon         : (%.6f, %.6f, %.6f)\n", origin[0], origin[1],
	             origin[2]);
	std::printf ("direction du rayon       : (%.6f, %.6f, %.6f)\n", direction[0], direction[1],
	             direction[2]);

	// LE PIXEL CENTRAL DU RECTANGLE, EXPRIME EN COORDONNEES DE FENETRE.
	const float centreWindowX = (float)rect.x + 0.5f * (float)rect.width;
	const float centreWindowY = (float)rect.y + 0.5f * (float)rect.height;

	float centreOrigin[3]    = { 0.f, 0.f, 0.f };
	float centreDirection[3] = { 0.f, 0.f, 0.f };
	if (!RayFromWindowPixel (rect, camera, centreWindowX, centreWindowY, centreOrigin,
	                         centreDirection))
	{
		std::fprintf (stderr, "peles : la vue n'a pas de dimensions exploitables.\n");
		return 1;
	}

	const float deviation = CrossNorm (centreDirection, axis);

	std::printf ("\ncentre du rectangle (FENETRE) : (%.1f, %.1f)\n", centreWindowX, centreWindowY);
	std::printf ("direction du rayon central    : (%.6f, %.6f, %.6f)\n", centreDirection[0],
	             centreDirection[1], centreDirection[2]);
	std::printf ("|rayon central x axe de vue|  : %.9f (tolerance %.0e)\n", deviation,
	             (double)kCollinearTolerance);

	if (deviation < kCollinearTolerance)
	{
		std::printf ("RAYON CENTRAL COLINEAIRE A L'AXE DE VUE : la conversion fenetre -> vue est "
		             "juste.\n");
		return 0;
	}

	// ⚠ LES DEUX TERMES SONT NOMMES, PAS SEULEMENT CELUI EN X. Dans le document
	// du produit `rect.y` vaut 0 : un message qui n'accuserait que la largeur
	// des zones de gauche designerait le mauvais terme des qu'une mise en page
	// descend la vue.
	std::fprintf (stderr,
	              "RAYON CENTRAL DIVERGENT : la conversion fenetre -> vue est fausse. L'origine "
	              "du rectangle vaut (%d, %d) : un decalage de l'un ou l'autre de ces deux "
	              "termes produit exactement cet ecart.\n",
	              rect.x, rect.y);
	return 1;
}

} // namespace app
} // namespace peles
