#include <gtest/gtest.h>

#include "../src/cggraph/canvas/canvas_layout.h"

// ===========================================================================
//  Disposition du canvas -- LA SECONDE PARTIE DU DESSIN QUI SE TESTE
// ===========================================================================
//
// ⚠ CE QUI N'EST PAS COUVERT, ET LE RESTE. Le rendu du canvas n'est verifie par
// aucun test : `TU` ne lie pas cggraph_canvas, qui n'est bati que sous
// ENABLE_CGGRAPH_BOILERPLATE, et un filet qui ne tourne pas en CI ne protege rien.
// Ce qui suit couvre la seule chose que la correction de dimensionnement ait
// rendue PURE : le calcul de la disposition a partir d'une taille d'affichage.
// Que les fenetres ImGui soient effectivement posees dessus n'est etabli que
// par la mesure a la colonne de pixels sur capture, hote par hote.
//
// canvas_layout.h est inclus et non lie, comme canvas_style.h : tout y est
// `inline` et rien n'y tire ImGui.

using namespace cggraph_canvas;

namespace {

// La regle qui a motive tout le chantier : l'editeur de noeuds ne dessine la
// totalite de sa zone que si l'origine du contenu de sa fenetre hote est
// exactement (0, 0). Mesure a la colonne de pixels, hote natif et hote web.
void ExpectGraphCoversDisplay (float width, float height)
{
	const CanvasLayout layout = ComputeLayout (width, height);
	EXPECT_FLOAT_EQ (0.0f, layout.graph.x) << "affichage " << width << "x" << height;
	EXPECT_FLOAT_EQ (0.0f, layout.graph.y) << "affichage " << width << "x" << height;
	EXPECT_FLOAT_EQ (width, layout.graph.width) << "affichage " << width << "x" << height;
	EXPECT_FLOAT_EQ (height, layout.graph.height) << "affichage " << width << "x" << height;
}

} // namespace

// Le fond couvre l'affichage, pour TOUTE taille plausible -- c'est la propriete
// dont depend la correction, et elle ne souffre pas d'exception.
TEST (CggraphCanvasLayout, GraphCoversWholeDisplay)
{
	const float sizes[][2] = { { 1400.0f, 900.0f },  { 1600.0f, 1000.0f }, { 1920.0f, 1080.0f },
		                       { 1024.0f, 768.0f },  { 800.0f, 600.0f },   { 640.0f, 480.0f },
		                       { 320.0f, 240.0f },   { 120.0f, 90.0f },    { 1.0f, 1.0f } };
	for (const float *size : sizes)
		ExpectGraphCoversDisplay (size[0], size[1]);
}

// Sur un affichage confortable, les largeurs nominales sont tenues telles
// quelles : le chantier ne devait pas changer l'aspect des panneaux, seulement
// cesser de les poser en dur.
TEST (CggraphCanvasLayout, NominalWidthsOnRoomyDisplay)
{
	const CanvasLayout layout = ComputeLayout (1600.0f, 1000.0f);

	EXPECT_FLOAT_EQ (kPaletteWidth, layout.palette.width);
	EXPECT_FLOAT_EQ (kInspectorWidth, layout.inspector.width);
	EXPECT_FLOAT_EQ (kLayoutMargin, layout.palette.x);
	EXPECT_FLOAT_EQ (kLayoutMargin, layout.palette.y);
	EXPECT_FLOAT_EQ (kLayoutMargin, layout.inspector.y);
}

// L'inspecteur est colle au bord DROIT, et la hauteur des deux panneaux suit
// celle de l'affichage : c'est ce que « remplir le cadre » veut dire.
TEST (CggraphCanvasLayout, PanelsFollowDisplayEdges)
{
	const float widths[] = { 900.0f, 1280.0f, 1600.0f, 2560.0f };
	const float height = 1000.0f;

	for (float width : widths)
	{
		const CanvasLayout layout = ComputeLayout (width, height);
		EXPECT_FLOAT_EQ (width - kLayoutMargin,
		                 layout.inspector.x + layout.inspector.width)
		    << "largeur " << width;
		EXPECT_FLOAT_EQ (height - 2.0f * kLayoutMargin, layout.palette.height)
		    << "largeur " << width;
		EXPECT_FLOAT_EQ (height - 2.0f * kLayoutMargin, layout.inspector.height)
		    << "largeur " << width;
	}
}

// Les deux panneaux ne se rejoignent jamais : il reste toujours de quoi voir le
// graphe entre eux. Sans cette borne, un cadre etroit donnerait un editeur dont
// la seule chose invisible serait ce qu'il edite.
TEST (CggraphCanvasLayout, PanelsNeverOverlap)
{
	for (int width = 0; width <= 2600; width += 13)
	{
		const CanvasLayout layout = ComputeLayout (static_cast<float> (width), 800.0f);
		const float gap = layout.inspector.x - (layout.palette.x + layout.palette.width);
		EXPECT_GE (gap, -1e-3f) << "largeur " << width;
	}
}

// Cadre etroit : les deux panneaux se retrecissent ENSEMBLE et gardent leur
// rapport 280:320. Un seul des deux qui cederait donnerait une disposition
// bancale a mi-chemin.
TEST (CggraphCanvasLayout, PanelsShrinkTogether)
{
	const CanvasLayout layout = ComputeLayout (500.0f, 700.0f);

	ASSERT_GT (layout.palette.width, 0.0f);
	EXPECT_LT (layout.palette.width, kPaletteWidth);
	EXPECT_LT (layout.inspector.width, kInspectorWidth);
	EXPECT_NEAR (kPaletteWidth / kInspectorWidth,
	             layout.palette.width / layout.inspector.width, 1e-4f);
}

// La largeur d'un panneau ne DECROIT jamais quand l'affichage s'elargit. Une
// disposition non monotone se traduirait a l'ecran par un panneau qui sursaute
// pendant qu'on tire le coin de la fenetre.
TEST (CggraphCanvasLayout, PanelWidthIsMonotonic)
{
	float previousPalette = -1.0f;
	float previousInspector = -1.0f;

	for (int width = 0; width <= 2000; width += 7)
	{
		const CanvasLayout layout = ComputeLayout (static_cast<float> (width), 800.0f);
		EXPECT_GE (layout.palette.width, previousPalette - 1e-4f) << "largeur " << width;
		EXPECT_GE (layout.inspector.width, previousInspector - 1e-4f) << "largeur " << width;
		previousPalette = layout.palette.width;
		previousInspector = layout.inspector.width;
	}
}

// Aucune dimension negative, quelle que soit l'entree -- y compris nulle ou
// absurde. Une largeur negative passee a ImGui::SetNextWindowSize ne serait pas
// refusee : elle serait dessinee.
TEST (CggraphCanvasLayout, NoNegativeDimension)
{
	const float sizes[][2] = { { 0.0f, 0.0f },   { 1.0f, 1.0f },     { 16.0f, 16.0f },
		                       { 0.0f, 900.0f }, { 1400.0f, 0.0f },  { -100.0f, -100.0f },
		                       { 40.0f, 4000.0f } };

	for (const float *size : sizes)
	{
		const CanvasLayout layout = ComputeLayout (size[0], size[1]);
		const LayoutRect *rects[] = { &layout.graph, &layout.palette, &layout.inspector };
		for (const LayoutRect *rect : rects)
		{
			EXPECT_GE (rect->x, 0.0f) << size[0] << "x" << size[1];
			EXPECT_GE (rect->y, 0.0f) << size[0] << "x" << size[1];
			EXPECT_GE (rect->width, 0.0f) << size[0] << "x" << size[1];
			EXPECT_GE (rect->height, 0.0f) << size[0] << "x" << size[1];
		}
	}
}

// Les panneaux restent DANS l'affichage. C'est le defaut d'origine, pris a
// l'envers : 1390 x 880 de panneaux poses en dur debordaient de tout cadre plus
// petit, et flottaient dans tout cadre plus grand.
TEST (CggraphCanvasLayout, PanelsStayInsideDisplay)
{
	const float sizes[][2] = { { 1600.0f, 1000.0f }, { 1400.0f, 900.0f }, { 1024.0f, 768.0f },
		                       { 800.0f, 600.0f },   { 480.0f, 320.0f },  { 200.0f, 200.0f },
		                       { 32.0f, 32.0f } };

	for (const float *size : sizes)
	{
		const CanvasLayout layout = ComputeLayout (size[0], size[1]);
		const LayoutRect *panels[] = { &layout.palette, &layout.inspector };
		for (const LayoutRect *panel : panels)
		{
			EXPECT_LE (panel->x + panel->width, size[0] + 1e-3f) << size[0] << "x" << size[1];
			EXPECT_LE (panel->y + panel->height, size[1] + 1e-3f) << size[0] << "x" << size[1];
		}
	}
}

// ---------------------------------------------------------------------------
//  SEPARATEUR -- l'editeur a gauche, la vue 3D a droite
// ---------------------------------------------------------------------------

TEST (TEST_cggraph_canvas_layout, the_default_split_leaves_the_editor_alone)
{
	// Le defaut vaut 1 : c'est la disposition d'AVANT le separateur, et tout
	// appelant qui ne le passe pas doit obtenir exactement la meme chose. C'est
	// ce qui permet aux dix-neuf cas ci-dessus de ne pas bouger d'un pixel.
	const CanvasLayout implicit = ComputeLayout (1600.0f, 1000.0f);
	const CanvasLayout explicitOne = ComputeLayout (1600.0f, 1000.0f, 1.0f);

	EXPECT_FLOAT_EQ (implicit.graph.width, 1600.0f);
	EXPECT_FLOAT_EQ (implicit.graph.width, explicitOne.graph.width);
	EXPECT_FLOAT_EQ (implicit.inspector.x, explicitOne.inspector.x);
	// Et la vue est alors VIDE, pas negative.
	EXPECT_FLOAT_EQ (implicit.view.width, 0.0f);
}

TEST (TEST_cggraph_canvas_layout, the_two_panes_tile_the_display_exactly)
{
	// Les deux cotes viennent du meme calcul, donc ils se touchent sans
	// recouvrement ni trou -- un pixel de l'un ne peut pas appartenir a l'autre.
	const float fractions[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
	for (float f : fractions)
	{
		const CanvasLayout layout = ComputeLayout (1600.0f, 1000.0f, f);
		EXPECT_FLOAT_EQ (layout.graph.x, 0.0f) << f;
		EXPECT_FLOAT_EQ (layout.view.x, layout.graph.width) << f;
		EXPECT_FLOAT_EQ (layout.graph.width + layout.view.width, 1600.0f) << f;
		EXPECT_FLOAT_EQ (layout.graph.height, 1000.0f) << f;
		EXPECT_FLOAT_EQ (layout.view.height, 1000.0f) << f;
	}
}

TEST (TEST_cggraph_canvas_layout, the_panels_stay_inside_the_editor_pane)
{
	// LE POINT QUI MOTIVE LE SEPARATEUR. Les panneaux flottent AU-DESSUS de
	// l'editeur ; s'ils suivaient la largeur d'affichage, l'inspecteur se
	// poserait par-dessus la vue 3D -- soit exactement la superposition que la
	// separation existe pour supprimer.
	for (float f : { 0.4f, 0.5f, 0.6f, 0.8f })
	{
		const CanvasLayout layout = ComputeLayout (1600.0f, 1000.0f, f);
		EXPECT_GE (layout.palette.x, 0.0f) << f;
		EXPECT_LE (layout.inspector.x + layout.inspector.width, layout.graph.width + 0.001f) << f;
		EXPECT_LE (layout.palette.x + layout.palette.width, layout.graph.width + 0.001f) << f;
	}
}

TEST (TEST_cggraph_canvas_layout, a_narrow_editor_pane_shrinks_the_panels_rather_than_overflowing)
{
	// La regle de retrecissement s'applique a la part de l'EDITEUR, pas a
	// l'affichage : reduire le volet gauche doit retrecir les panneaux comme le
	// ferait une fenetre etroite.
	const CanvasLayout wide = ComputeLayout (1600.0f, 800.0f, 1.0f);
	const CanvasLayout narrow = ComputeLayout (1600.0f, 800.0f, 0.3f);
	EXPECT_LT (narrow.palette.width, wide.palette.width);
	EXPECT_GE (narrow.palette.width, 0.0f);
	EXPECT_GE (narrow.inspector.width, 0.0f);
	EXPECT_LE (narrow.inspector.x + narrow.inspector.width, narrow.graph.width + 0.001f);
}

TEST (TEST_cggraph_canvas_layout, an_out_of_range_split_is_brought_back_into_it)
{
	// Bornage, comme partout ou une valeur vient de l'exterieur -- ici d'un
	// glissement a la souris dans la page. Hors de [0, 1] la fraction n'a pas de
	// sens, et une largeur negative ferait un viewport GL invalide.
	const CanvasLayout below = ComputeLayout (1200.0f, 600.0f, -3.0f);
	EXPECT_FLOAT_EQ (below.graph.width, 0.0f);
	EXPECT_FLOAT_EQ (below.view.width, 1200.0f);

	const CanvasLayout above = ComputeLayout (1200.0f, 600.0f, 4.0f);
	EXPECT_FLOAT_EQ (above.graph.width, 1200.0f);
	EXPECT_FLOAT_EQ (above.view.width, 0.0f);
}
