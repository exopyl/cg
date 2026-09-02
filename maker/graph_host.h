#pragma once
// ===========================================================================
//  L'etat de l'instance web du graphe -- UN document, DEUX facades
// ===========================================================================
//
// Deux fichiers exposent ce meme document au JS du worker :
//
//   graph_api.cpp     la facade PROCEDURALE (graphAddNode, graphConnect,
//                     graphEvaluate...), celle que les sondes de maker/probes/
//                     pilotent ;
//   graph_editor.cpp  l'hote ImGui, qui dessine le canvas commun aux deux hotes
//                     (D4) et rend les gestes de la souris.
//
// ⚠ ELLES PARTAGENT LE DOCUMENT, ET C'EST DELIBERE. Deux Host separes auraient
// donne deux graphes dans la meme instance : la page aurait edite l'un et
// affiche l'autre. C'est la meme erreur que D17 ecarte entre instances, en
// pire -- ici rien ne l'aurait signalee.
//
// LE PILOTE EST « EN LIGNE », et il n'y a pas d'alternative sur cette cible :
// cggraph::AsyncEvaluator demarre un std::thread a sa construction, et sous
// Emscripten sans -pthread ce constructeur LEVE (std::system_error, errno 138).
// C'est le fil du worker qui calcule, et EditorModel::Pump () est le seul
// endroit ou cela se produit.
//
#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "../src/cggraph/ui/editor_model.h"
#include "../src/cggraph/ui/eval_driver.h"

namespace maker_graph
{

// Echappement JSON, ici parce que les DEUX facades ecrivent du JSON a la main et
// qu'une copie de chaque cote finirait par diverger. Les chemins de fichier lui
// doivent leur passage : sous Windows ils portent des antislashs, qui sans cela
// casseraient le document rendu au JS.
inline std::string JsonEscape (const std::string &s)
{
    std::string o;
    o.reserve (s.size () + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:   o += c;      break;
        }
    }
    return o;
}

struct Host
{
	// Le pilote est garde a part du modele : l'hote web regle le budget de
	// cache de l'evaluateur et lit ses statistiques (criteres 7.5 et 7.6), ce
	// que le modele ne relaie pas -- sur un pilote a fil, ces deux gestes
	// seraient des courses, et le modele n'a pas a savoir lequel il tient.
	cggraph_ui::InlineEvalDriver *driver = nullptr;

	// Declare APRES le pilote : la fabrique le renseigne pendant la
	// construction du modele.
	cggraph_ui::EditorModel model;

	unsigned int progressTicks = 0;

	Host ();
};

Host &host ();

// Repart d'un graphe vide. Le pilote est reconstruit avec lui : son cache est
// indexe sur les signatures du precedent, et le collecteur de progression est
// perdu -- l'appelant reinstalle son pompage apres un reset.
void ResetHost ();

// Budget de cache du HOTE WEB -- voir la derivation dans graph_api.cpp.
extern const std::size_t kWebMemoryBudget;

} // namespace maker_graph
