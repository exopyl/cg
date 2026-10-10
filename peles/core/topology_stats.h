#pragma once

class Model;

namespace peles
{
namespace core
{

// ---------------------------------------------------------------------------
//  LES DEUX COMPTEURS TOPOLOGIQUES, DU COTE DOCUMENT
// ---------------------------------------------------------------------------
//
// ⚠ DANS L'HOTE HISTORIQUE, CES DEUX NOMBRES VIENNENT DE LA VUE
// (sinaia/SinaiaFrame.cpp:2219-2220 interroge pGLCanvas->GetNBorders() et
// GetNNonManifoldEdges(), dont le cache est MyGLCanvas::UpdateTopologicIssues).
// Ici ils viennent de core/, et ce n'est pas un rangement : peles_core ne lie
// ni cgre, ni glfw, ni RmlUi. Les prendre a la vue ferait dependre core de
// view3d et le pare-feu d'inclusion tomberait.
//
// ⚠ C'EST AUSSI CE QUI REND --dump-info EXECUTABLE SANS CONTEXTE GL, donc sur
// une machine sans interface graphique.
//
// LA DIVISION PAR DEUX N'EST PAS UN ARRONDI : Mesh::GetTopologicIssues rend les
// aretes sous forme de PAIRES D'INDICES DE SOMMETS a plat, donc deux entrees
// par arete.
struct TopologyStats
{
	unsigned int borders          = 0;
	unsigned int nonManifoldEdges = 0;
};

// ⚠ PORTEE : LE MODELE DESIGNE, ET NON LA SCENE ENTIERE. L'hote historique
// affiche les compteurs du modele SELECTIONNE mais les calcule sur tous les
// modeles ouverts ; les deux coincident tant qu'un seul fichier est charge, et
// divergent des le second. Ce qui est affiche ici decrit le modele nomme par la
// ligne « File » juste au-dessus.
//
// ⚠ `model` EST PRIS PAR REFERENCE NON CONSTANTE parce que le cache derive de
// cgmesh est indexe par `Mesh*` non constant. Le calcul lui-meme ne modifie pas
// la geometrie.
TopologyStats ComputeTopologyStats (Model& model);

} // namespace core
} // namespace peles
