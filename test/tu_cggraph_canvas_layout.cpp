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
