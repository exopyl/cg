// ===========================================================================
//  maker - HOTE WEB du canvas nodal : contexte, frame, entrees, et rien d'autre
// ===========================================================================
//
// Ce fichier est a la cible WebAssembly ce que cggraph-boilerplate/main.cpp est au
// natif : la SEULE partie de l'editeur qui soit propre a un hote. Il ne dessine
// aucun widget et ne connait ni la palette, ni l'inspecteur, ni le graphe. C'est
// exactement ce que l'en-tete de src/cggraph/canvas/node_canvas.h annonce --
// « un second hote remplace ce fichier et rien d'autre » --, et c'est la
// premiere fois que la phrase est mise a l'epreuve.
//
// ⚠ PAS DE GLFW ICI, alors que l'hote natif en a un et que la chaine Emscripten
// en fournit un port (-sUSE_GLFW=3). Motif : ce port s'adresse au DOM
// (`document`, un <canvas> retrouve par selecteur, des ecouteurs d'evenements),
// et il n'y a pas de DOM dans un Web Worker. D27 met le rendu DANS le worker ;
// GLFW y est donc hors sujet, et les evenements arrivent par postMessage. Le
// choix GLFW + OpenGL 3 de l'etape 4 n'est pas perdu pour autant : c'est le
// BACKEND OpenGL 3 d'ImGui qui sert ici, inchange, en GLES3/WebGL2.
//
// LE CONTEXTE GRAPHIQUE EST PARTAGE AVEC LE JS DU WORKER, et la propriete qui
// le permet est celle de getContext() : deux appels de meme identifiant sur un
// meme canvas rendent LE MEME objet. Le JS cree son contexte WebGL2 pour
// l'apercu 3D ; emscripten_webgl_create_context () retrouve le meme a travers
// specialHTMLTargets. Un canvas, un contexte, deux appelants -- la scene
// dessous, l'interface dessus.
//
// LE CALCUL N'A PAS DE FIL, et c'est le pilote en ligne qui le porte
// (graph_host.h). graphEditorPump () est appele HORS FRAME par la boucle du
// worker : c'est la, et nulle part ailleurs, qu'une demande devient un calcul,
// et c'est de la que le collecteur de progression peut redessiner sans se
// retrouver au milieu d'une frame commencee.
//
// ===========================================================================

#include <cfloat>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

#include <emscripten/bind.h>
#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
// GLES3 : le backend OpenGL 3 d'ImGui l'utilise deja, mais il inclut son propre
// chargeur en interne. Le televerseur de vignette appelle glGenTextures /
// glTexImage2D directement, donc il lui faut l'en-tete.
#include <GLES3/gl3.h>
#include <emscripten/val.h>

#include <imgui.h>
#include <imgui_impl_opengl3.h>

#include "../src/cggraph/canvas/node_canvas.h"
#include "../src/cggraph/nodes/node_support.h"   // ByteSource, pour la sonde
#include "../src/cggraph/ui/editor_model.h"
#include "graph_host.h"

namespace
{

// Le canvas transfere est depose par le JS du worker dans globalThis, puis
// publie ici sous un nom de cible que emscripten_webgl_create_context sait
// resoudre. specialHTMLTargets est le mecanisme documente pour designer un
// objet que document.querySelector ne peut pas atteindre -- et dans un worker,
// document.querySelector ne peut atteindre rien du tout.
EM_JS (int, BindTransferredCanvas, (), {
	if (!globalThis.__makerCanvas)
		return 0;
	specialHTMLTargets['!maker_canvas'] = globalThis.__makerCanvas;
	return 1;
});
EM_JS_DEPS (maker_graph_editor, "$specialHTMLTargets");

struct Editor
{
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE gl = 0;
	ImGuiContext *imgui = nullptr;
	cggraph_canvas::NodeCanvas *canvas = nullptr;
	bool overlay = true;
	// SEPARATEUR, garde par l'HOTE et non par le canvas : graphEditorReset en
	// construit un neuf a chaque document, et un reglage de vue n'a aucune raison
	// de repartir a zero quand le document change. C'est le meme motif que le
	// televerseur de vignette et la sonde d'octets, reposes juste apres.
	float split = 1.0f;
	int width = 1;
	int height = 1;
	std::string error;
};

Editor g_editor;

// SONDE « CE NOEUD PREND-IL DES OCTETS ? ». C'est un dynamic_cast vers
// ByteSource, exactement celui que graphAcceptsBytes fait pour la facade
// procedurale -- et c'est pourquoi il est ici et non dans le canvas : l'y
// descendre lui donnerait une dependance vers la couche des noeuds, alors que
// l'hote la connait deja.
static bool AcceptsBytes (cggraph::NodeId id)
{
	cggraph::Node *node = maker_graph::host ().model.GetGraph ().FindNode (id);
	return node != nullptr && dynamic_cast<cggraph_nodes::ByteSource *> (node) != nullptr;
}

// TELEVERSEUR DE VIGNETTE. Le canvas rend des octets RGBA et ne connait aucune
// API graphique ; c'est ici, cote hote, que GL existe.
//
// REUTILISE la texture quand elle existe : une vignette est refaite a chaque
// evaluation, et en creer une neuve a chaque fois fuirait un objet GL par calcul.
static void *UploadThumbnail (const unsigned char *rgba, int width, int height, void *reuse)
{
	if (rgba == nullptr || width <= 0 || height <= 0)
		return reuse;

	GLuint texture = (GLuint)(std::uintptr_t)reuse;
	if (texture == 0)
	{
		glGenTextures (1, &texture);
		if (texture == 0)
			return nullptr;
		glBindTexture (GL_TEXTURE_2D, texture);
		// LINEAR et CLAMP : une vignette est affichee a sa taille exacte, mais
		// ImGui peut l'echelonner sur un ecran a densite elevee.
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri (GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	else
	{
		glBindTexture (GL_TEXTURE_2D, texture);
	}

	// glTexImage2D et non glTexSubImage2D : les dimensions changent d'une
	// vignette a l'autre (l'image source n'a pas toujours le meme rapport).
	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA,
	              GL_UNSIGNED_BYTE, rgba);
	glBindTexture (GL_TEXTURE_2D, 0);
	return (void *)(std::uintptr_t)texture;
}


// --------------------------------------------------------------------------
//  Clavier : event.code -> ImGuiKey
// --------------------------------------------------------------------------
//
// La table est ECRITE EN C++, et les codes DOM la traversent tels quels. La
// tenir cote JS aurait oblige a y recopier les valeurs de l'enumeration
// d'ImGui, qui n'est pas stable d'une version a l'autre : une montee de version
// aurait donne un clavier faux et silencieux.
struct KeyMap
{
	const char *code;
	ImGuiKey key;
};

const KeyMap kKeys[] = {
	{ "ArrowLeft", ImGuiKey_LeftArrow },   { "ArrowRight", ImGuiKey_RightArrow },
	{ "ArrowUp", ImGuiKey_UpArrow },       { "ArrowDown", ImGuiKey_DownArrow },
	{ "PageUp", ImGuiKey_PageUp },         { "PageDown", ImGuiKey_PageDown },
	{ "Home", ImGuiKey_Home },             { "End", ImGuiKey_End },
	{ "Insert", ImGuiKey_Insert },         { "Delete", ImGuiKey_Delete },
	{ "Backspace", ImGuiKey_Backspace },   { "Space", ImGuiKey_Space },
	{ "Enter", ImGuiKey_Enter },           { "NumpadEnter", ImGuiKey_KeypadEnter },
	{ "Escape", ImGuiKey_Escape },         { "Tab", ImGuiKey_Tab },
	{ "ControlLeft", ImGuiKey_LeftCtrl },  { "ControlRight", ImGuiKey_RightCtrl },
	{ "ShiftLeft", ImGuiKey_LeftShift },   { "ShiftRight", ImGuiKey_RightShift },
	{ "AltLeft", ImGuiKey_LeftAlt },       { "AltRight", ImGuiKey_RightAlt },
	{ "MetaLeft", ImGuiKey_LeftSuper },    { "MetaRight", ImGuiKey_RightSuper },
	{ "Minus", ImGuiKey_Minus },           { "Equal", ImGuiKey_Equal },
	{ "BracketLeft", ImGuiKey_LeftBracket }, { "BracketRight", ImGuiKey_RightBracket },
	{ "Backslash", ImGuiKey_Backslash },   { "Semicolon", ImGuiKey_Semicolon },
	{ "Quote", ImGuiKey_Apostrophe },      { "Backquote", ImGuiKey_GraveAccent },
	{ "Comma", ImGuiKey_Comma },           { "Period", ImGuiKey_Period },
	{ "Slash", ImGuiKey_Slash },           { "CapsLock", ImGuiKey_CapsLock },
};

ImGuiKey KeyFromCode (const std::string &code)
{
	for (const KeyMap &entry : kKeys)
		if (code == entry.code)
			return entry.key;

	// Les familles regulieres se derivent plutot que de s'epeler : trois
	// prefixes couvrent 48 touches, et une table de 48 lignes de plus se serait
	// trompee une fois.
	if (code.size () == 4 && code.compare (0, 3, "Key") == 0 && code[3] >= 'A' && code[3] <= 'Z')
		return (ImGuiKey) (ImGuiKey_A + (code[3] - 'A'));
	if (code.size () == 6 && code.compare (0, 5, "Digit") == 0 && code[5] >= '0' && code[5] <= '9')
		return (ImGuiKey) (ImGuiKey_0 + (code[5] - '0'));
	if (code.size () == 7 && code.compare (0, 6, "Numpad") == 0 && code[6] >= '0' && code[6] <= '9')
		return (ImGuiKey) (ImGuiKey_Keypad0 + (code[6] - '0'));
	if (code.size () >= 2 && code[0] == 'F' && code[1] >= '1' && code[1] <= '9')
	{
		const int n = std::atoi (code.c_str () + 1);
		if (n >= 1 && n <= 12)
			return (ImGuiKey) (ImGuiKey_F1 + (n - 1));
	}
	return ImGuiKey_None;
}

} // namespace

// ---------------------------------------------------------------------------
//  Cycle de vie
// ---------------------------------------------------------------------------

// Rend "" en cas de succes, sinon le motif. Un refus NET vaut mieux qu'un
// editeur a moitie monte : la page affiche ce que ce texte dit.
std::string graphEditorInit (int width, int height)
{
	if (g_editor.canvas != nullptr)
		return std::string ();

	if (!BindTransferredCanvas ())
		return "globalThis.__makerCanvas absent : le canvas transfere n'a pas ete publie";

	EmscriptenWebGLContextAttributes attributes;
	emscripten_webgl_init_context_attributes (&attributes);
	attributes.majorVersion = 2;
	attributes.minorVersion = 0;
	attributes.alpha = false;
	attributes.depth = true;
	attributes.antialias = true;

	// ⚠ Ce contexte est CELUI DU JS s'il l'a deja cree : getContext() rend le
	// meme objet pour un meme identifiant sur un meme canvas. Les attributs
	// ci-dessus ne servent donc que si personne n'a demande avant nous.
	g_editor.gl = emscripten_webgl_create_context ("!maker_canvas", &attributes);
	if (g_editor.gl <= 0)
		return "emscripten_webgl_create_context a refuse le canvas transfere";
	if (emscripten_webgl_make_context_current (g_editor.gl) != EMSCRIPTEN_RESULT_SUCCESS)
		return "emscripten_webgl_make_context_current a echoue";

	IMGUI_CHECKVERSION ();
	g_editor.imgui = ImGui::CreateContext ();
	ImGuiIO &io = ImGui::GetIO ();

	// Aucun fichier de reglages : le document du graphe porte deja les
	// positions, et un second endroit finirait par contredire le premier. Le
	// canvas fait le meme choix pour l'editeur de noeuds.
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	// L'hote FOURNIT le curseur souris : sans ce drapeau, ImGui croirait la
	// souris absente et aucun survol ne repondrait.
	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendPlatformName = "maker-worker";

	ImGui::StyleColorsDark ();

	// ⚠ COEXISTENCE AVEC L'APERCU 3D -- c'est ici qu'elle se decide, et nulle
	// part dans le canvas. La scene est dessinee SOUS l'interface, sur le meme
	// canvas ; des fenetres opaques la cacheraient entierement. Un fond
	// legerement transparent la laisse voir en permanence, et le basculement
	// d'overlay (graphEditorSetOverlay) la rend en plein cadre. L'hote natif
	// n'a pas d'apercu et garde donc le style par defaut : le reglage est bien
	// une affaire d'hote.
	ImGuiStyle &style = ImGui::GetStyle ();
	style.Colors[ImGuiCol_WindowBg].w = 0.86f;
	style.Colors[ImGuiCol_ChildBg].w = 0.0f;
	style.Colors[ImGuiCol_TitleBg].w = 0.90f;
	style.Colors[ImGuiCol_TitleBgActive].w = 0.90f;

	if (!ImGui_ImplOpenGL3_Init ("#version 300 es"))
	{
		ImGui::DestroyContext (g_editor.imgui);
		g_editor.imgui = nullptr;
		return "ImGui_ImplOpenGL3_Init a echoue";
	}

	g_editor.canvas = new cggraph_canvas::NodeCanvas ();
	g_editor.canvas->SetTextureUploader (&UploadThumbnail);
	g_editor.canvas->SetByteSourceProbe (&AcceptsBytes);
	g_editor.canvas->SetSplit (g_editor.split);
	g_editor.width = width > 0 ? width : 1;
	g_editor.height = height > 0 ? height : 1;
	io.DisplaySize = ImVec2 ((float)g_editor.width, (float)g_editor.height);
	return std::string ();
}

void graphEditorResize (int width, int height)
{
	g_editor.width = width > 0 ? width : 1;
	g_editor.height = height > 0 ? height : 1;
	if (g_editor.imgui != nullptr)
		ImGui::GetIO ().DisplaySize = ImVec2 ((float)g_editor.width, (float)g_editor.height);
}

bool graphEditorReady ()
{
	return g_editor.canvas != nullptr;
}

// ⚠ A APPELER APRES graphReset (). Le canvas garde la trace des noeuds dont il
// a deja pousse la position vers l'editeur de noeuds ; un document neuf reprend
// les identifiants a zero, et cette trace ferait tenir pour deja placees des
// boites qui ne le sont pas. Le contexte ImGui, lui, survit : ce sont les
// identifiants du GRAPHE qui repartent, pas ceux de l'interface.
void graphEditorReset ()
{
	if (g_editor.canvas == nullptr)
		return;
	delete g_editor.canvas;
	g_editor.canvas = new cggraph_canvas::NodeCanvas ();
	g_editor.canvas->SetTextureUploader (&UploadThumbnail);
	g_editor.canvas->SetByteSourceProbe (&AcceptsBytes);
	g_editor.canvas->SetSplit (g_editor.split);
}

// ⚠ A APPELER APRES graphFromJson () -- et apres graphEditorReset (), qui rend
// un canvas neuf et perdrait la demande. C'est l'HOTE qui sait qu'un document
// vient d'arriver ; le canvas ne voit qu'un graphe, et un graphe qui grandit
// d'un noeud a la souris ne doit pas recadrer la vue. Le cadrage lui-meme est
// dans le canvas (D4) : ce qui traverse ici, c'est l'evenement, pas le geste.
void graphEditorFitToContent ()
{
	if (g_editor.canvas != nullptr)
		g_editor.canvas->RequestFitToContent ();
}

// L'overlay coupe, l'apercu 3D occupe le cadre entier. C'est la reponse simple
// a « ne pas perdre l'apercu » : plutot que de negocier la place avec trois
// fenetres, on rend le basculement explicite et gratuit.
void graphEditorSetOverlay (bool on)
{
	g_editor.overlay = on;
}

bool graphEditorGetOverlay ()
{
	return g_editor.overlay;
}

// SEPARATEUR. La fraction est celle de l'EDITEUR ; la vue 3D prend le reste, et
// c'est le JS qui y pose son viewport. La valeur ne vit qu'a un endroit -- ici
// elle ne fait que descendre dans le canvas, qui en tire sa disposition.
void graphEditorSetSplit (float fraction)
{
	// RETENU MEME SANS CANVAS : la page peut poser sa fraction avant que
	// l'editeur soit construit, et la perdre alors laisserait la vue 3D dessinee
	// a droite pendant que l'editeur s'etale sur toute la largeur.
	g_editor.split = fraction;
	if (g_editor.canvas != nullptr)
		g_editor.canvas->SetSplit (fraction);
}

float graphEditorGetSplit ()
{
	return g_editor.split;
}

// ---------------------------------------------------------------------------
//  Frame
// ---------------------------------------------------------------------------

// Construit la frame : NewFrame, le canvas, Render. AUCUN appel de dessin GL
// n'en sort -- ImGui produit une liste, et c'est graphEditorRenderDrawData qui
// l'emet. Les deux sont separes parce que le JS du worker doit pouvoir
// intercaler l'apercu 3D ENTRE les deux, et re-emettre la meme liste autant de
// fois qu'il le veut : la liste reste valide jusqu'au NewFrame suivant, et
// c'est ce qui rend le pompage de frame possible pendant un calcul.
void graphEditorFrame (double deltaSeconds)
{
	if (g_editor.canvas == nullptr)
		return;

	ImGuiIO &io = ImGui::GetIO ();
	io.DisplaySize = ImVec2 ((float)g_editor.width, (float)g_editor.height);
	io.DeltaTime = deltaSeconds > 0.0 ? (float)deltaSeconds : 1.0f / 60.0f;

	ImGui_ImplOpenGL3_NewFrame ();
	ImGui::NewFrame ();
	if (g_editor.overlay)
		g_editor.canvas->Draw (maker_graph::host ().model);
	ImGui::Render ();
}

void graphEditorRenderDrawData ()
{
	if (g_editor.canvas == nullptr || !g_editor.overlay)
		return;
	ImDrawData *data = ImGui::GetDrawData ();
	if (data != nullptr)
		ImGui_ImplOpenGL3_RenderDrawData (data);
}

// ⚠ HORS FRAME. Le seul endroit ou une demande devient un calcul sur cette
// cible ; sur l'hote natif le meme appel ne fait rien, le fil de calcul n'ayant
// besoin de personne. C'est le point unique ou la divergence entre les deux
// hotes pourrait se loger, et c'est pour cela qu'il en existe UN.
void graphEditorPump ()
{
	if (g_editor.canvas != nullptr)
		maker_graph::host ().model.Pump ();
}

// Demande une evaluation PAR LE MODELE, comme le fait le bouton de
// l'inspecteur. Elle est coalescee et differee comme n'importe quelle autre :
// c'est graphEditorPump () qui la servira, hors frame. Elle existe pour que la
// page -- et la sonde -- puissent declencher le chemin du tick sans passer par
// un clic dont personne ne connait les coordonnees.
//
// ⚠ A ne pas confondre avec graphEvaluate () de graph_api.cpp, qui est
// SYNCHRONE et rend son diagnostic sur place. Les deux servent le meme
// document ; celle-ci passe par la boucle de frames, l'autre non.
void graphEditorRequest (unsigned int id)
{
	if (g_editor.canvas != nullptr)
		maker_graph::host ().model.RequestEvaluation (id);
}

unsigned int graphEditorResultRevision ()
{
	return maker_graph::host ().model.GetResultRevision ();
}

// Ce que la page affiche a cote du canvas : selection, etat, dernier
// diagnostic. Des chaines et des nombres -- rien qui ressemble a de la
// geometrie.
std::string graphEditorState ()
{
	const cggraph_ui::EditorModel &model = maker_graph::host ().model;
	std::string j = "{\"selection\":" + std::to_string (model.GetSelection ());
	j += ",\"nodes\":" + std::to_string (model.GetGraph ().GetNodeCount ());
	j += ",\"links\":" + std::to_string (model.GetGraph ().GetLinks ().size ());
	j += ",\"evaluating\":";
	j += model.IsEvaluating () ? "true" : "false";
	j += ",\"revision\":" + std::to_string (model.GetResultRevision ());
	j += ",\"overlay\":";
	j += g_editor.overlay ? "true" : "false";

	// Ce que l'interface reclame. L'hote s'en sert pour decider s'il avale un
	// raccourci du navigateur ou s'il le laisse passer -- et c'est aussi le seul
	// observable, depuis l'exterieur, qui dise qu'un geste a bien atteint ImGui.
	const ImGuiIO &io = ImGui::GetIO ();
	j += ",\"wantMouse\":";
	j += io.WantCaptureMouse ? "true" : "false";
	j += ",\"wantKeyboard\":";
	j += io.WantCaptureKeyboard ? "true" : "false";
	j += ",\"wantText\":";
	j += io.WantTextInput ? "true" : "false";

	// DEMANDE DE SELECTION DE FICHIER posee par le bouton « Parcourir... » de
	// l'inspecteur. Le canvas ne peut pas ouvrir de selecteur -- il est dessine
	// par ImGui, dans un Worker, sans acces au DOM -- il DEMANDE, et l'hote s'en
	// charge. Meme separation que le pompage de frame : le canvas dit ce qu'il
	// veut, l'hote sait comment.
	//
	// La lecture EFFACE la demande cote canvas, donc ce champ ne peut apparaitre
	// qu'une fois par clic. C'est aussi pourquoi il est lu ICI, dans l'etat que
	// l'hote consulte a chaque frame, plutot que par un appel separe qu'il
	// faudrait penser a faire.
	// Le canvas peut ne pas exister encore (etat interroge avant graphEditorInit).
	const cggraph_canvas::NodeCanvas::FileRequest request =
		g_editor.canvas != nullptr ? g_editor.canvas->TakeFileRequest ()
		                           : cggraph_canvas::NodeCanvas::FileRequest ();
	if (request.node != 0)
	{
		j += ",\"fileRequest\":{\"node\":" + std::to_string (request.node);
		j += ",\"param\":\"" + request.param + "\"}";
	}

	// DESCENTE. Publiee parce que les compteurs de la page portent sur le
	// document RACINE : sans cette mention, la barre annoncerait « 2 noeuds »
	// pendant qu'on regarde un corps qui en a trois.
	if (g_editor.canvas != nullptr && g_editor.canvas->GetDepth () > 0)
	{
		j += ",\"descent\":{\"depth\":"
		     + std::to_string ((unsigned long long)g_editor.canvas->GetDepth ());
		j += ",\"reference\":\"" + maker_graph::JsonEscape (g_editor.canvas->GetCurrentReference ()) + "\"}";
	}

	j += "}";
	return j;
}

// Curseur voulu par ImGui, rendu a la page qui pose le CSS correspondant. Sans
// lui, le redimensionnement d'une fenetre et les champs de texte ne donneraient
// aucun retour visuel -- le canvas etant dans le worker, le navigateur ne sait
// rien de ce qu'ImGui survole.
int graphEditorCursor ()
{
	if (g_editor.imgui == nullptr)
		return ImGuiMouseCursor_Arrow;
	return (int)ImGui::GetMouseCursor ();
}

// ---------------------------------------------------------------------------
//  Entrees -- routees depuis le thread UI par postMessage
// ---------------------------------------------------------------------------

void graphEditorMouseMove (float x, float y)
{
	if (g_editor.imgui == nullptr)
		return;
	ImGui::GetIO ().AddMousePosEvent (x, y);
}

void graphEditorMouseButton (int button, bool down)
{
	if (g_editor.imgui == nullptr || button < 0 || button > 4)
		return;
	ImGui::GetIO ().AddMouseButtonEvent (button, down);
}

void graphEditorMouseWheel (float dx, float dy)
{
	if (g_editor.imgui == nullptr)
		return;
	ImGui::GetIO ().AddMouseWheelEvent (dx, dy);
}

// Le pointeur QUITTE le cadre : sans cet appel, ImGui garderait la derniere
// position et continuerait de surligner ce qui s'y trouve.
void graphEditorMouseLeave ()
{
	if (g_editor.imgui == nullptr)
		return;
	ImGui::GetIO ().AddMousePosEvent (-FLT_MAX, -FLT_MAX);
}

void graphEditorKey (const std::string &code, bool down, bool ctrl, bool shift, bool alt,
                     bool meta)
{
	if (g_editor.imgui == nullptr)
		return;
	ImGuiIO &io = ImGui::GetIO ();

	// Les modificateurs AVANT la touche : ImGui lit l'etat des modificateurs au
	// moment ou il traite l'evenement de touche, et les poser apres ferait
	// arriver Ctrl+C comme un C nu.
	io.AddKeyEvent (ImGuiMod_Ctrl, ctrl);
	io.AddKeyEvent (ImGuiMod_Shift, shift);
	io.AddKeyEvent (ImGuiMod_Alt, alt);
	io.AddKeyEvent (ImGuiMod_Super, meta);

	const ImGuiKey key = KeyFromCode (code);
	if (key != ImGuiKey_None)
		io.AddKeyEvent (key, down);
}

// ⚠ LE TEXTE EST UN EVENEMENT A PART, et ce n'est pas une redondance avec la
// touche. Un code DOM designe une POSITION sur le clavier ; le caractere saisi
// depend de la disposition, des modificateurs et de la composition. Les champs
// de l'inspecteur ont besoin du second, pas du premier.
void graphEditorText (unsigned int codepoint)
{
	if (g_editor.imgui == nullptr || codepoint == 0)
		return;
	ImGui::GetIO ().AddInputCharacter (codepoint);
}

// Le cadre perd le focus : toutes les touches sont relachees. Sans cela, une
// touche maintenue au moment ou l'on change d'onglet reste enfoncee pour
// toujours du point de vue d'ImGui.
void graphEditorFocus (bool focused)
{
	if (g_editor.imgui == nullptr)
		return;
	ImGuiIO &io = ImGui::GetIO ();
	if (!focused)
		io.ClearInputKeys ();
	io.AddFocusEvent (focused);
}

EMSCRIPTEN_BINDINGS (maker_graph_editor_bindings)
{
	emscripten::function ("graphEditorInit", &graphEditorInit);
	emscripten::function ("graphEditorReady", &graphEditorReady);
	emscripten::function ("graphEditorReset", &graphEditorReset);
	emscripten::function ("graphEditorFitToContent", &graphEditorFitToContent);
	emscripten::function ("graphEditorResize", &graphEditorResize);
	emscripten::function ("graphEditorSetOverlay", &graphEditorSetOverlay);
	emscripten::function ("graphEditorGetOverlay", &graphEditorGetOverlay);
	emscripten::function ("graphEditorSetSplit", &graphEditorSetSplit);
	emscripten::function ("graphEditorGetSplit", &graphEditorGetSplit);
	emscripten::function ("graphEditorFrame", &graphEditorFrame);
	emscripten::function ("graphEditorRenderDrawData", &graphEditorRenderDrawData);
	emscripten::function ("graphEditorPump", &graphEditorPump);
	emscripten::function ("graphEditorRequest", &graphEditorRequest);
	emscripten::function ("graphEditorResultRevision", &graphEditorResultRevision);
	emscripten::function ("graphEditorState", &graphEditorState);
	emscripten::function ("graphEditorCursor", &graphEditorCursor);
	emscripten::function ("graphEditorMouseMove", &graphEditorMouseMove);
	emscripten::function ("graphEditorMouseButton", &graphEditorMouseButton);
	emscripten::function ("graphEditorMouseWheel", &graphEditorMouseWheel);
	emscripten::function ("graphEditorMouseLeave", &graphEditorMouseLeave);
	emscripten::function ("graphEditorKey", &graphEditorKey);
	emscripten::function ("graphEditorText", &graphEditorText);
	emscripten::function ("graphEditorFocus", &graphEditorFocus);
}
