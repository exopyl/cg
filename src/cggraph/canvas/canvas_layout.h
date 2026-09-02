#pragma once
//
//  Disposition du canvas -- DERIVEE DE L'AFFICHAGE, jamais ecrite en dur.
//
// Les trois panneaux de l'editeur etaient poses en pixels fixes (280 + 760 +
// 320 a partir de x = 10), pour un total de 1390 x 880. Sur un affichage plus
// petit ils debordaient, sur un plus grand ils laissaient du vide -- mesure sur
// l'hote web, ou 1390 x 880 de panneaux flottaient dans un cadre de
// 1600 x 1000. Ce fichier remplace ces constantes par une fonction de la taille
// d'affichage, et c'est la seule chose qu'il fait.
//
// ⚠ LE GRAPHE OCCUPE L'AFFICHAGE ENTIER, A PARTIR DE (0, 0), et ce n'est pas
// une preference de mise en page : c'est une CONTRAINTE MESUREE de
// imgui-node-editor 0.9.3. L'editeur de noeuds dessine son contenu aux bonnes
// coordonnees d'ecran mais lui laisse un rectangle de decoupe exprime dans son
// repere LOCAL. Le contenu visible est donc l'intersection de la zone de dessin
// avec le rectangle [0, 0] - [largeur, hauteur] pris en coordonnees d'ecran :
// il manque exactement l'origine du contenu de la fenetre hote, a droite et en
// bas. Mesure a la colonne de pixels, hote natif et hote web, capture de
// 1400 x 800 et de 1600 x 1000 :
//
//     fenetre du graphe posee a (300, 10), zone de dessin de 744 px de large
//     a partir de x = 308  ->  grille rendue de x = 309 a x = 743, soit 435 px,
//     et fond de fenetre au-dela. 744 - 308 = 436.
//
//     meme fenetre posee a (0, 0), sans barre de titre ni marge interieure,
//     zone de dessin de 1384 px a partir de x = 0  ->  grille rendue jusqu'a
//     x = 1383. Perte nulle.
//
// La perte vaut donc l'origine du contenu, et elle ne s'annule QUE si cette
// origine est (0, 0). Une disposition en trois colonnes juxtaposees, avec le
// graphe au milieu, couterait sa marge gauche entiere -- 308 px sur 744 dans la
// mesure ci-dessus. Le graphe est donc le FOND de l'affichage, et les deux
// panneaux flottent au-dessus. C'est aussi la facon dont l'amont s'emploie
// lui-meme.
//
// ⚠ CE FICHIER NE CONNAIT NI ImGui NI LE NODE EDITOR, pour la meme raison que
// canvas_style.h : `TU` ne lie pas cggraph_canvas, qui n'est bati que sous
// ENABLE_CGGRAPH_BOILERPLATE, mais il peut INCLURE un en-tete dont tout est `inline`
// et sans dependance. Ce qui suit est couvert en CI ; le dessin qui s'en sert
// ne l'est pas.
//
namespace cggraph_canvas
{

// Coin superieur gauche et dimensions, en pixels de l'affichage.
struct LayoutRect
{
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

struct CanvasLayout
{
	// L'EDITEUR : toujours a partir de (0, 0) -- voir l'en-tete, l'origine est ce
	// qui se perd au decoupage -- mais plus forcement sur toute la largeur : il
	// s'arrete au separateur.
	LayoutRect graph;
	LayoutRect palette;
	LayoutRect inspector;

	// LA VUE 3D : ce qui reste a droite du separateur. Le canvas ne la dessine
	// pas -- c'est l'hote qui y pose son viewport --, mais elle est calculee ici
	// pour que les deux cotes viennent d'un seul endroit et ne puissent pas
	// diverger d'un pixel.
	LayoutRect view;
};

// Marge entre un panneau flottant et le bord de l'affichage.
inline constexpr float kLayoutMargin = 10.0f;

// Largeurs nominales des deux panneaux, tenues tant que l'affichage les porte.
inline constexpr float kPaletteWidth = 280.0f;
inline constexpr float kInspectorWidth = 320.0f;

// Ce qui doit rester VISIBLE du graphe entre les deux panneaux. En deca, les
// panneaux se retrecissent plutot que de se rejoindre : un editeur nodal dont
// on ne voit plus le graphe n'est plus un editeur nodal.
inline constexpr float kMinGraphWidth = 260.0f;

inline float ClampFraction (float value)
{
	if (!(value > 0.0f)) return 0.0f;   // attrape aussi NaN
	return value < 1.0f ? value : 1.0f;
}

inline float ClampToZero (float value)
{
	return value > 0.0f ? value : 0.0f;
}

// La disposition pour un affichage de `displayWidth` x `displayHeight`.
//
// Trois regles, dans cet ordre :
//   1. le graphe couvre l'affichage entier depuis (0, 0) -- voir l'en-tete ;
//   2. les deux panneaux gardent leur largeur nominale tant que
//      2 * marge + nominal + kMinGraphWidth tient dans la largeur ;
//   3. sinon ils se retrecissent ENSEMBLE, dans le meme rapport, jusqu'a
//      zero -- aucune dimension rendue n'est negative, quelle que soit
//      l'entree, y compris nulle.
inline CanvasLayout ComputeLayout (float displayWidth, float displayHeight,
                                   float splitFraction = 1.0f)
{
	const float width = ClampToZero (displayWidth);
	const float height = ClampToZero (displayHeight);

	// PART DE L'EDITEUR dans la largeur. Le defaut vaut 1 : l'editeur occupe
	// tout, la vue 3D est vide, et c'est exactement la disposition d'avant le
	// separateur -- les appelants qui ne le passent pas ne changent pas de
	// comportement.
	const float editorWidth = width * ClampFraction (splitFraction);

	CanvasLayout layout;
	layout.graph.x = 0.0f;
	layout.graph.y = 0.0f;
	layout.graph.width = editorWidth;
	layout.graph.height = height;

	layout.view.x = editorWidth;
	layout.view.y = 0.0f;
	layout.view.width = ClampToZero (width - editorWidth);
	layout.view.height = height;

	// La marge suit l'affichage quand celui-ci devient minuscule : une marge
	// fixe de 10 px sur un cadre de 16 px ne laisserait pas de panneau du tout.
	// LES PANNEAUX SUIVENT L'EDITEUR, pas l'affichage : ils flottent au-dessus de
	// lui, donc les pousser au bord de l'ecran les ferait passer par-dessus la
	// vue 3D -- exactement ce que le separateur existe pour empecher.
	float margin = kLayoutMargin;
	if (margin > editorWidth * 0.125f)
		margin = editorWidth * 0.125f;
	if (margin > height * 0.125f)
		margin = height * 0.125f;

	const float nominal = kPaletteWidth + kInspectorWidth;
	const float room = ClampToZero (editorWidth - 2.0f * margin - kMinGraphWidth);
	const float scale = room < nominal ? room / nominal : 1.0f;

	const float paletteWidth = kPaletteWidth * scale;
	const float inspectorWidth = kInspectorWidth * scale;
	const float panelHeight = ClampToZero (height - 2.0f * margin);

	layout.palette.x = margin;
	layout.palette.y = margin;
	layout.palette.width = paletteWidth;
	layout.palette.height = panelHeight;

	layout.inspector.x = editorWidth - margin - inspectorWidth;
	layout.inspector.y = margin;
	layout.inspector.width = inspectorWidth;
	layout.inspector.height = panelHeight;

	return layout;
}

} // namespace cggraph_canvas
