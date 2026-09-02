// ===========================================================================
//  maker - pont Embind pour la couche nodale (cggraph / cggraph_nodes)
// ===========================================================================
//
// SECONDE facade du meme artefact, a cote de wasm_api.cpp. Un seul module
// (D27) ; deux INSTANCES au chargement (D17) : les six pages de formes gardent
// la leur sur le thread UI, le graphe charge la sienne dans un Web Worker.
//
// CE FICHIER EST LA FACADE PROCEDURALE. Le document qu'il edite est celui de
// maker_graph::host () -- le meme que l'hote ImGui de graph_editor.cpp. Deux
// documents dans une meme instance auraient donne une page qui edite l'un et
// affiche l'autre ; voir graph_host.h.
//
// L'EVALUATION EST SYNCHRONE ICI, et ce n'est pas une simplification.
// cggraph::AsyncEvaluator demarre un std::thread a sa construction ; sous
// Emscripten sans -pthread, ce constructeur LEVE -- std::system_error
// "thread constructor failed: Not supported", errno 138. Il compile, il lie, il
// ne s'instancie pas. Le document est donc porte par un EditorModel construit
// sur le pilote EN LIGNE (cggraph_ui::InlineEvalDriver), qui calcule dans
// Pump () sur le fil du worker.
//
// ⚠ ET C'EST LA LE CHANGEMENT : cggraph_ui::EditorModel etait hors d'atteinte
// de cette cible parce qu'il DETENAIT un AsyncEvaluator par valeur. Il detient
// desormais un EvalDriver, dont le pilote a fil n'est qu'une des deux
// realisations. L'API publique du modele n'a pas bouge pour l'hote natif.
//
// POMPAGE DE FRAME (conception §7.3) : l'hote installe un collecteur de
// progression qui rappelle une fonction JS. cggraph ne connait donc pas le
// rendu -- il appelle ctx.Progress, et c'est CE fichier, cote hote, qui decide
// que cela veut dire "dessine une frame". Retirer le rappel JS ne change rien
// au moteur.
//
// FRONTIERE : aucune geometrie ne quitte ce module vers le thread UI (D27). Les
// vues typees rendues par graphMeshView() sont lues par le JS DU WORKER, qui
// les televerse dans WebGL2 sur l'OffscreenCanvas qu'on lui a transfere.
//
// ===========================================================================

#include <cstdio>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "../src/cggraph/core/serialize.h"
#include "../src/cggraph/nodes/catalog.h"
#include "../src/cggraph/nodes/catalog_factory.h"
#include "../src/cggraph/nodes/node_support.h"
#include "../src/cggraph/nodes/value_types.h"
#include "graph_host.h"
#include "mesh_payload.h"
#include "mesh.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/bind.h>
#include <emscripten/emscripten.h>
#include <emscripten/heap.h>
#include <emscripten/val.h>
#endif

namespace maker_graph
{

// Budget de cache du HOTE WEB -- 128 Mio, et c'est un chiffre RELEVE.
//
// La valeur par defaut du moteur (Evaluator::kDefaultMemoryBudget, 256 Mio) est
// une valeur unique qui ne vise aucun hote. Celle-ci vise celui-ci, et voici sa
// derivation, mesuree par maker/probes/step7.js sur un graphe de 8 noeuds
// (Chrome, tas des deux instances additionnes) :
//
//   500 000 triangles : 415 Mio au total, dont 92 Mio de cache -- soit un
//                       PLANCHER de ~324 Mio que le budget ne touche pas ;
//   2 000 000          : 844 Mio au total avec un budget de 32 Mio, 1 357 Mio
//                        avec 1 Gio. Hors enveloppe quel que soit le budget.
//
// L'enveloppe web est 256 a 512 Mio. Le plus gros cas qui y tienne est donc le
// premier, et le budget doit satisfaire deux choses a la fois : contenir sa
// chaine entiere -- 92 Mio, mesures, 8 entrees et zero eviction --, et ne pas
// pouvoir a lui seul faire sortir le module de l'enveloppe : 324 + 128 = 452,
// sous 512. Un budget de 192 Mio tiendrait la premiere condition et pas la
// seconde (324 + 192 = 516).
//
// Ce que la mesure dit d'autre, et qu'il faut porter : le budget n'est PAS ce
// qui garde la cible dans son enveloppe. Le plancher -- le pic transitoire d'un
// seul noeud -- l'est, et aucun budget ne l'atteint.
const std::size_t kWebMemoryBudget = 128u * 1024u * 1024u;

Host::Host ()
	: model ([this] (cggraph::Graph &graph) {
		  std::unique_ptr<cggraph_ui::InlineEvalDriver> made (
		      new cggraph_ui::InlineEvalDriver (graph));
		  driver = made.get ();
		  return std::unique_ptr<cggraph_ui::EvalDriver> (std::move (made));
	  })
{
	driver->GetEvaluator ().SetMemoryBudget (kWebMemoryBudget);
}

namespace
{
Host *g_host = nullptr;
}

Host &host ()
{
	if (g_host == nullptr)
		g_host = new Host ();
	return *g_host;
}

void ResetHost ()
{
	delete g_host;
	g_host = new Host ();
}

} // namespace maker_graph

namespace {

using maker_graph::host;
using maker_graph::JsonEscape;

const char *NameOf (cggraph::ConnectStatus s)
{
    switch (s) {
        case cggraph::ConnectStatus::Ok:                    return "ok";
        case cggraph::ConnectStatus::UnknownNode:           return "unknown-node";
        case cggraph::ConnectStatus::UnknownPort:           return "unknown-port";
        case cggraph::ConnectStatus::TypeMismatch:          return "type-mismatch";
        case cggraph::ConnectStatus::InputAlreadyConnected: return "input-already-connected";
        case cggraph::ConnectStatus::Cycle:                 return "cycle";
    }
    return "?";
}

const char *NameOf (cggraph::EvalStatus s)
{
    switch (s) {
        case cggraph::EvalStatus::Ok:                  return "ok";
        case cggraph::EvalStatus::UnknownNode:         return "unknown-node";
        case cggraph::EvalStatus::IncompatibleVersion: return "incompatible-version";
        case cggraph::EvalStatus::MissingInput:        return "missing-input";
        case cggraph::EvalStatus::DrivenParameter:     return "driven-parameter";
        case cggraph::EvalStatus::ComputeFailed:       return "compute-failed";
        case cggraph::EvalStatus::Aborted:             return "aborted";
        case cggraph::EvalStatus::Busy:                return "busy";
    }
    return "?";
}

double NowMs ()
{
#ifdef __EMSCRIPTEN__
    return emscripten_get_now ();
#else
    return 0.0;
#endif
}

} // namespace

// ---------------------------------------------------------------------------
// Cycle de vie
// ---------------------------------------------------------------------------

void graphReset ()
{
    maker_graph::ResetHost ();
}

// ---------------------------------------------------------------------------
// Catalogue, topologie, parametres
// ---------------------------------------------------------------------------

// Catalogue des types de noeuds, derive de cggraph_nodes::Catalog(). La palette
// de la page se peuple d'ici : une seconde liste cote JS divergerait.
std::string graphCatalog ()
{
    std::string j = "[";
    bool first = true;
    for (const cggraph_nodes::CatalogEntry &e : cggraph_nodes::Catalog ()) {
        if (!first) j += ',';
        first = false;
        j += "{\"type\":\""; j += JsonEscape (e.typeName);
        j += "\",\"label\":\""; j += JsonEscape (e.label);
        j += "\",\"category\":\""; j += JsonEscape (e.category);
        j += "\"}";
    }
    j += ']';
    return j;
}

// 0 si le type est inconnu -- kInvalidNodeId, et c'est le meme refus que celui
// de la fabrique du catalogue.
unsigned int graphAddNode (const std::string &typeName, float x, float y)
{
    return host ().model.AddNode (typeName, x, y);
}

bool graphRemoveNode (unsigned int id)
{
    return host ().model.RemoveNode (id) == cggraph::RemoveStatus::Ok;
}

std::string graphConnect (unsigned int from, unsigned int fromPort, unsigned int to,
                          unsigned int toPort)
{
    return NameOf (host ().model.Connect (from, (cggraph::PortIdx)fromPort, to,
                                          (cggraph::PortIdx)toPort));
}

bool graphDisconnect (unsigned int to, unsigned int toPort)
{
    return host ().model.Disconnect (to, (cggraph::PortIdx)toPort);
}

// Description d'un noeud : ports et parametres PUBLICS. L'inspecteur de la page
// s'en deduit ; les parametres Internal (source.identity) n'y figurent pas,
// exactement comme dans l'inspecteur natif.
std::string graphNodeInfo (unsigned int id)
{
    const cggraph::Node *node = host ().model.GetGraph ().FindNode (id);
    if (node == nullptr)
        return "null";

    const cggraph::NodeDesc &desc = node->GetDesc ();
    std::string j = "{\"id\":" + std::to_string (id);
    j += ",\"type\":\"" + JsonEscape (desc.typeName) + "\"";
    j += ",\"inputs\":[";
    for (std::size_t i = 0; i < desc.inputs.size (); ++i) {
        if (i) j += ',';
        j += "{\"name\":\"" + JsonEscape (desc.inputs[i].name) + "\",\"optional\":"
             + (desc.inputs[i].optional ? "true" : "false") + "}";
    }
    j += "],\"outputs\":[";
    for (std::size_t i = 0; i < desc.outputs.size (); ++i) {
        if (i) j += ',';
        j += "{\"name\":\"" + JsonEscape (desc.outputs[i].name) + "\"}";
    }
    j += "],\"params\":[";
    bool first = true;
    for (const cggraph::ParamEntry &entry : node->GetParams ().GetEntries ()) {
        if (entry.visibility == cggraph::ParamVisibility::Internal)
            continue;
        if (!first) j += ',';
        first = false;
        j += "{\"name\":\"" + JsonEscape (entry.name) + "\",\"type\":\"";
        switch (entry.value.type) {
            case cggraph::ParamType::Int:
                j += "int\",\"value\":" + std::to_string (entry.value.intValue);
                break;
            case cggraph::ParamType::Float: {
                char buf[32];
                std::snprintf (buf, sizeof (buf), "%.6g", (double)entry.value.floatValue);
                j += "float\",\"value\":";
                j += buf;
                break;
            }
            case cggraph::ParamType::Bool:
                j += "bool\",\"value\":";
                j += entry.value.boolValue ? "true" : "false";
                break;
            case cggraph::ParamType::String:
                j += "string\",\"value\":\"" + JsonEscape (entry.value.stringValue) + "\"";
                break;
        }
        j += "}";
    }
    j += "]}";
    return j;
}

// Les quatre setters passent par le MODELE et non par le graphe : un parametre
// change sous l'inspecteur qui pointe dedans, et c'est RefreshInspector qui
// republie les adresses.
namespace {

cggraph::Node *FindForWrite (unsigned int id)
{
    return host ().model.GetGraph ().FindNode (id);
}

void NoteParamChanged (unsigned int id)
{
    if (host ().model.GetSelection () == id)
        host ().model.RefreshInspector ();
}

} // namespace

bool graphSetInt (unsigned int id, const std::string &name, int value)
{
    cggraph::Node *node = FindForWrite (id);
    const bool ok = node != nullptr && node->GetParams ().SetInt (name, value) != nullptr;
    if (ok) NoteParamChanged (id);
    return ok;
}

bool graphSetFloat (unsigned int id, const std::string &name, float value)
{
    cggraph::Node *node = FindForWrite (id);
    const bool ok = node != nullptr && node->GetParams ().SetFloat (name, value) != nullptr;
    if (ok) NoteParamChanged (id);
    return ok;
}

bool graphSetBool (unsigned int id, const std::string &name, bool value)
{
    cggraph::Node *node = FindForWrite (id);
    const bool ok = node != nullptr && node->GetParams ().SetBool (name, value) != nullptr;
    if (ok) NoteParamChanged (id);
    return ok;
}

// Le noeud designe accepte-t-il des octets ? L'hote s'en sert pour choisir, sur
// un import de fichier, entre « pousser dans ce noeud » et « deposer en MEMFS ».
// Sans cette question, l'interface devrait deviner d'apres le nom du type -- une
// liste de plus, a tenir a jour ailleurs.
bool graphAcceptsBytes (unsigned int id)
{
    cggraph::Node *node = FindForWrite (id);
    return node != nullptr && dynamic_cast<cggraph_nodes::ByteSource *> (node) != nullptr;
}

bool graphSetString (unsigned int id, const std::string &name, const std::string &value)
{
    cggraph::Node *node = FindForWrite (id);
    const bool ok = node != nullptr && node->GetParams ().SetString (name, value) != nullptr;
    if (ok) NoteParamChanged (id);
    return ok;
}

// ---------------------------------------------------------------------------
// Document
// ---------------------------------------------------------------------------

std::string graphToJson ()
{
    return cggraph::SaveGraph (host ().model.GetGraph ());
}

// Rend "" en cas de succes, sinon le statut nomme suivi de son detail. Le graphe
// est remis a vide avant la relecture : LoadGraph l'exige, les identifiants du
// document etant repris tels quels.
std::string graphFromJson (const std::string &text)
{
    graphReset ();
    const cggraph_nodes::CatalogFactory factory;
    const cggraph::LoadResult result =
        cggraph::LoadGraph (text, factory, host ().model.GetGraph ());
    if (result.IsOk ())
        return std::string ();
    return std::string (cggraph::ToString (result.status)) + ": " + result.detail;
}

// ---------------------------------------------------------------------------
// Evaluation
// ---------------------------------------------------------------------------

void graphCancel ()
{
    host ().model.CancelEvaluation ();
}

// Budget du cache -- point ouvert 5. Il est un parametre du moteur depuis
// l'etape 1 ; ce qui manquait etait une valeur PAR HOTE, et c'est ici que le
// hote web pose la sienne.
void graphSetMemoryBudget (double bytes)
{
    host ().driver->GetEvaluator ().SetMemoryBudget ((std::size_t)bytes);
}

double graphGetMemoryBudget ()
{
    return (double)host ().driver->GetEvaluator ().GetMemoryBudget ();
}

std::string graphCacheStats ()
{
    const cggraph::CacheStats &s = host ().driver->GetEvaluator ().GetStats ();
    std::string j = "{\"hits\":" + std::to_string (s.hits);
    j += ",\"misses\":" + std::to_string (s.misses);
    j += ",\"evictions\":" + std::to_string (s.evictions);
    j += ",\"entries\":" + std::to_string (s.entries);
    j += ",\"bytes\":" + std::to_string (s.bytes);
    j += ",\"budget\":" + std::to_string (host ().driver->GetEvaluator ().GetMemoryBudget ())
         + "}";
    return j;
}

// Nombre de fois que le collecteur de progression a ete appele depuis le dernier
// graphEvaluate. C'est l'instrument du pompage : sans lui, "la frame est pompee"
// ne s'observe qu'a l'oeil.
unsigned int graphProgressTicks ()
{
    return host ().progressTicks;
}

// Evalue la branche qui alimente `id` et rend un diagnostic JSON. Aucune
// geometrie dans ce qui sort : nv/nf sont des COMPTES, la geometrie se relit par
// graphMeshView, dans le worker.
std::string graphEvaluate (unsigned int id)
{
    maker_graph::Host &h = host ();
    h.progressTicks = 0;

    const double t0 = NowMs ();
    // SYNCHRONE : RequestNow, puis WaitIdle qui POMPE -- il n'y a pas d'autre
    // fil pour avancer, et c'est le pilote en ligne qui le dit.
    const cggraph::EvalResult result = h.model.Evaluate (id);
    const double t1 = NowMs ();

    std::size_t nv = 0, nf = 0;
    if (result.IsOk ()) {
        for (const cggraph::Value &value : h.model.GetLastOutputs ()) {
            const std::shared_ptr<const Mesh> mesh =
                value.Share<Mesh> (cggraph_nodes::Types ().mesh);
            if (mesh != nullptr) {
                nv = mesh->GetNVertices ();
                nf = mesh->GetNFaces ();
                break;
            }
        }
    }

    std::string j = "{\"status\":\"";
    j += NameOf (result.status);
    j += "\",\"node\":" + std::to_string (result.node);
    j += ",\"detail\":\"" + JsonEscape (result.detail) + "\"";
    j += ",\"ms\":" + std::to_string ((long long)(t1 - t0));
    j += ",\"nv\":" + std::to_string (nv);
    j += ",\"nf\":" + std::to_string (nf);
    j += ",\"ticks\":" + std::to_string (h.progressTicks);
    j += "}";
    return j;
}

#ifdef __EMSCRIPTEN__

// Geometrie de la sortie retenue, en vues typees sur le tas de CE module. Elles
// ne traversent aucun postMessage : le renderer qui les lit vit dans le meme
// worker (D27).
//
// Le corps est PARTAGE avec graphMeshData, la facade des pages (mesh_payload.h).
// Il ne l'etait pas, et cela se voyait : cette vue-ci ne transportait que
// positions et indices, si bien qu'un relief d'image reconstruit dans l'editeur
// nodal sortait en bloc uniforme, sans palette ni texture, la ou la page
// « Image to puzzle » le montrait correctement.
static maker::MeshPayloadBuffers g_viewBufs;

emscripten::val graphMeshView (unsigned int port)
{
    maker_graph::Host &h = host ();
    const cggraph::ValueList &outputs = h.model.GetLastOutputs ();
    const std::shared_ptr<const Mesh> mesh =
        (port < outputs.size ())
            ? outputs[port].Share<Mesh> (cggraph_nodes::Types ().mesh)
            : nullptr;
    return maker::BuildMeshPayload (mesh.get (), g_viewBufs);
}

// Le collecteur de progression de l'hote. C'est LUI qui fait du pompage de
// frame une decision d'hote : cggraph appelle ctx.Progress, ce fichier appelle
// le JS, et le JS dessine. Passer une valeur non-fonction desinstalle le
// rappel -- le moteur, lui, ne change pas de comportement.
//
// ⚠ Rend FALSE si le pilote refuse le collecteur -- ce que fait le pilote a
// fil, dont le collecteur tournerait sur le fil de calcul. Sur cette cible le
// pilote est en ligne, donc il accepte ; le booleen existe pour que le jour ou
// quelqu'un portera ce fichier sur un hote a fils, le refus se voie.
bool graphSetFramePump (emscripten::val pump)
{
    maker_graph::Host &h = host ();
    if (pump.typeOf ().as<std::string> () != "function")
        return h.model.SetProgressSink (cggraph::EvalContext::ProgressSink ());

    maker_graph::Host *target = &h;
    return h.model.SetProgressSink ([pump, target] (float t, const char *label) {
        ++target->progressTicks;
        pump (t, std::string (label != nullptr ? label : ""));
    });
}

// Taille du TAS de cette instance, en octets. C'est la mesure du critere 7.5 :
// sous D17 il y a deux instances, donc deux tas, et aucun plafond commun ne
// borne leur somme (-sALLOW_MEMORY_GROWTH=1).
double heapBytes ()
{
    // emscripten_get_heap_size, et non Module.HEAPU8.byteLength : la vue JS
    // n'est pas attachee au Module par defaut, et la mesure du critere 7.5
    // n'a pas a dependre d'un export de runtime.
    return (double)emscripten_get_heap_size ();
}

// Alimente une source par ses OCTETS. Rend false si le noeud n'existe pas ou
// n'est pas une source d'octets -- text.font.load et img.io.load le sont ;
// mesh.io.load ne l'est PAS, il prend un chemin, et l'hote continue de le servir
// par MEMFS (message "loadFile").
//
// Aucune enumeration des types de noeuds ici : le dynamic_cast vers ByteSource
// suffit, exactement comme runner.cpp retrouve un FileSink. Une septieme source
// d'octets marchera sans toucher a ce fichier.
//
// ⚠ Les octets sont COPIES dans le tas WASM par convertJSArrayToNumberVector,
// puis a nouveau dans le noeud. Pour une police (~2 Mio) ou une image c'est sans
// consequence ; ce ne serait pas le cas pour de la geometrie, qui n'a de toute
// facon rien a faire sur cette frontiere (D27).
bool graphSetBytes (unsigned int id, emscripten::val bytes, const std::string &name)
{
    cggraph::Node *node = FindForWrite (id);
    if (node == nullptr)
        return false;

    cggraph_nodes::ByteSource *source = dynamic_cast<cggraph_nodes::ByteSource *> (node);
    if (source == nullptr)
        return false;

    std::vector<unsigned char> data =
        emscripten::convertJSArrayToNumberVector<unsigned char> (bytes);
    if (data.empty ())
        return false;

    source->SetBytes (std::move (data));
    if (!name.empty ())
        source->SetName (name);

    // SetBytes a reecrit le parametre d'identite : l'inspecteur qui pointe sur ce
    // noeud doit republier ses adresses, au meme titre que pour un setter
    // scalaire.
    NoteParamChanged (id);
    return true;
}

EMSCRIPTEN_BINDINGS(maker_graph)
{
    emscripten::function ("graphReset",           &graphReset);
    emscripten::function ("graphCatalog",         &graphCatalog);
    emscripten::function ("graphAddNode",         &graphAddNode);
    emscripten::function ("graphRemoveNode",      &graphRemoveNode);
    emscripten::function ("graphConnect",         &graphConnect);
    emscripten::function ("graphDisconnect",      &graphDisconnect);
    emscripten::function ("graphNodeInfo",        &graphNodeInfo);
    emscripten::function ("graphSetInt",          &graphSetInt);
    emscripten::function ("graphSetFloat",        &graphSetFloat);
    emscripten::function ("graphSetBool",         &graphSetBool);
    emscripten::function ("graphSetString",       &graphSetString);
    emscripten::function ("graphSetBytes",        &graphSetBytes);
    emscripten::function ("graphAcceptsBytes",    &graphAcceptsBytes);
    emscripten::function ("graphToJson",          &graphToJson);
    emscripten::function ("graphFromJson",        &graphFromJson);
    emscripten::function ("graphEvaluate",        &graphEvaluate);
    emscripten::function ("graphCancel",          &graphCancel);
    emscripten::function ("graphSetMemoryBudget", &graphSetMemoryBudget);
    emscripten::function ("graphGetMemoryBudget", &graphGetMemoryBudget);
    emscripten::function ("graphCacheStats",      &graphCacheStats);
    emscripten::function ("graphProgressTicks",   &graphProgressTicks);
    emscripten::function ("graphMeshView",        &graphMeshView);
    emscripten::function ("graphSetFramePump",    &graphSetFramePump);
    emscripten::function ("heapBytes",            &heapBytes);
}

#endif
