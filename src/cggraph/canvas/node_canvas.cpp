#include "node_canvas.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

#include <imgui.h>
#include <imgui_node_editor.h>

#include "../ui/editor_model.h"
#include "canvas_style.h"

namespace ed = ax::NodeEditor;

namespace cggraph_canvas
{

namespace
{

// Identifiants de broche : le noeud dans les bits hauts, le port dans les bas,
// les sorties decalees de 0x80. L'editeur de noeuds ne connait que des entiers
// opaques ; les encoder ainsi evite une table de correspondance a tenir a jour
// a chaque frame, et le decodage est exact tant qu'un noeud a moins de 127
// ports de chaque cote -- le plus charge du depot en a deux.
const unsigned kOutputBit = 0x80u;

std::uintptr_t InputPinId (cggraph::NodeId node, cggraph::PortIdx port)
{
	return (static_cast<std::uintptr_t> (node) << 8) | (static_cast<std::uintptr_t> (port) + 1u);
}

std::uintptr_t OutputPinId (cggraph::NodeId node, cggraph::PortIdx port)
{
	return (static_cast<std::uintptr_t> (node) << 8)
	       | (kOutputBit + static_cast<std::uintptr_t> (port) + 1u);
}

struct PinRef
{
	cggraph::NodeId node = cggraph::kInvalidNodeId;
	cggraph::PortIdx port = 0;
	bool isOutput = false;
};

PinRef DecodePin (std::uintptr_t id)
{
	PinRef ref;
	ref.node = static_cast<cggraph::NodeId> (id >> 8);
	const unsigned slot = static_cast<unsigned> (id & 0xffu);
	ref.isOutput = slot > kOutputBit;
	ref.port = static_cast<cggraph::PortIdx> (ref.isOutput ? slot - kOutputBit - 1u : slot - 1u);
	return ref;
}

// Le vocabulaire de l'interface est celui des statuts nommes du moteur. Les
// traduire ici, et nulle part ailleurs, garde le moteur ignorant de la langue
// d'affichage sans qu'un refus perde son nom en chemin.
const char *Text (cggraph::ConnectStatus status)
{
	switch (status)
	{
	case cggraph::ConnectStatus::Ok: return "lien pose";
	case cggraph::ConnectStatus::UnknownNode: return "noeud inconnu";
	case cggraph::ConnectStatus::UnknownPort: return "port inconnu";
	case cggraph::ConnectStatus::TypeMismatch: return "types incompatibles";
	case cggraph::ConnectStatus::InputAlreadyConnected: return "entree deja alimentee";
	case cggraph::ConnectStatus::Cycle: return "cycle refuse";
	}
	return "refus";
}

const char *Text (cggraph::RemoveStatus status)
{
	switch (status)
	{
	case cggraph::RemoveStatus::Ok: return "noeud supprime";
	case cggraph::RemoveStatus::UnknownNode: return "noeud inconnu";
	}
	return "refus";
}

const char *Text (cggraph::EvalStatus status)
{
	switch (status)
	{
	case cggraph::EvalStatus::Ok: return "calcul termine";
	case cggraph::EvalStatus::UnknownNode: return "noeud inconnu";
	case cggraph::EvalStatus::IncompatibleVersion: return "version de noeud incompatible";
	case cggraph::EvalStatus::MissingInput: return "entree obligatoire non alimentee";
	case cggraph::EvalStatus::DrivenParameter: return "parametre pilote, non evaluable";
	case cggraph::EvalStatus::ComputeFailed: return "echec du calcul";
	case cggraph::EvalStatus::Aborted: return "calcul interrompu";
	case cggraph::EvalStatus::Busy: return "evaluateur deja occupe";
	}
	return "refus";
}

ImVec4 Color (cggraph::PortState state)
{
	switch (state)
	{
	case cggraph::PortState::Connected: return ImVec4 (0.45f, 0.80f, 0.45f, 1.0f);
	case cggraph::PortState::MissingRequired: return ImVec4 (0.90f, 0.45f, 0.35f, 1.0f);
	case cggraph::PortState::MissingOptional: return ImVec4 (0.55f, 0.55f, 0.55f, 1.0f);
	}
	return ImVec4 (1.0f, 1.0f, 1.0f, 1.0f);
}

// Un port obligatoire libre est SIGNALE, un port optionnel libre est GRISE.
// C'est toute la raison d'etre de la validation avant calcul : sans elle, les
// deux se dessineraient de la meme facon.
const char *Marker (cggraph::PortState state)
{
	switch (state)
	{
	case cggraph::PortState::Connected: return "*";
	case cggraph::PortState::MissingRequired: return "!";
	case cggraph::PortState::MissingOptional: return "o";
	}
	return " ";
}

// ---------------------------------------------------------------------------
//  Apparence -- entierement locale au dessin
// ---------------------------------------------------------------------------
//
// ⚠ TOUT CE QUI SUIT VIT DANS LE CANVAS, et il le faut : les deux hotes -- le
// natif et celui de la cible WebAssembly -- partagent ce fichier et rien
// d'autre (D4). Un style pose cote hote donnerait deux editeurs d'apparences
// differentes pour un seul canvas, et la divergence n'apparaitrait qu'a l'oeil.
//
// La seule chose que les hotes gardent en propre est la TRANSPARENCE de leur
// fenetre : l'hote web abaisse ImGuiCol_WindowBg pour laisser voir son apercu
// 3D dessous. Rien ici n'y touche, et l'opacite du fond de l'editeur reste
// celle du defaut pour la meme raison -- la rehausser masquerait cet apercu.

// Diametre de la pastille d'un port, et l'air entre elle et son libelle.
const float kGlyphSize = 11.0f;
const float kGlyphGap = 6.0f;

// Air entre la colonne des entrees et celle des sorties, et largeur plancher
// d'un noeud : sans elle, un noeud a un seul port court serait une vignette.
const float kColumnGap = 20.0f;
const float kMinContentWidth = 156.0f;

// Air sous le titre, a l'interieur du bandeau.
const float kHeaderGap = 4.0f;

// La SEULE couleur ecrite en dur du canvas : celle de ce qui bloque un calcul.
// Tout le reste est hache depuis un nom, donc rien d'autre n'est a tenir a jour
// quand le catalogue ou le registre de types grandissent.
const ImVec4 kAlert (0.95f, 0.42f, 0.30f, 1.0f);

ImVec4 ToVec4 (const Rgb &color, float alpha)
{
	return ImVec4 (color.r, color.g, color.b, alpha);
}

ImU32 ToU32 (const Rgb &color, float alpha)
{
	return ImGui::ColorConvertFloat4ToU32 (ToVec4 (color, alpha));
}

// Teinte d'un port : celle de son TYPE. Un port dont le descripteur ne nomme
// aucun type -- PortDesc::type est nullable -- prend un gris neutre plutot
// qu'une teinte tiree d'un nom vide : une couleur pleine affirmerait une
// information qui n'existe pas.
Rgb PortTint (const cggraph::TypeDesc *type)
{
	if (type == nullptr || type->name.empty ())
	{
		Rgb neutral;
		neutral.r = 0.62f;
		neutral.g = 0.62f;
		neutral.b = 0.66f;
		return neutral;
	}
	return NameTint (type->name);
}

// Le libelle d'un port ne porte PAS la teinte de son type : la pastille la
// porte deja, et deux fois la meme information ferait perdre la lisibilite du
// texte. Il ne porte que l'etat, et seulement quand l'etat a quelque chose a
// dire.
ImVec4 PortLabelColor (cggraph::PortState state)
{
	switch (state)
	{
	case cggraph::PortState::Connected: return ImVec4 (0.87f, 0.88f, 0.91f, 1.0f);
	case cggraph::PortState::MissingRequired: return kAlert;
	case cggraph::PortState::MissingOptional: return ImVec4 (0.55f, 0.55f, 0.59f, 1.0f);
	}
	return ImVec4 (1.0f, 1.0f, 1.0f, 1.0f);
}

// Reserve une largeur sans consommer de hauteur. La mise en deux colonnes fixe
// ainsi la largeur de celle de gauche, faute de quoi SameLine poserait celle de
// droite au bout du plus long libelle et les sorties ne seraient plus alignees
// d'un noeud a l'autre.
void ReserveWidth (float width)
{
	if (width <= 0.5f)
		return;
	ImGui::PushStyleVar (ImGuiStyleVar_ItemSpacing, ImVec2 (0.0f, 0.0f));
	ImGui::Dummy (ImVec2 (width, 0.0f));
	ImGui::PopStyleVar ();
}

// Meme chose, mais le curseur reste sur la ligne : c'est ce qui pousse une
// ligne de sortie vers le bord DROIT de sa colonne.
void Indent (float width)
{
	if (width <= 0.5f)
		return;
	ReserveWidth (width);
	ImGui::SameLine (0.0f, 0.0f);
}

// Un port se DESSINE, il ne s'ecrit pas -- et le glyphe porte trois
// informations sur trois canaux qui ne se recouvrent pas :
//
//   la FORME dit l'optionnalite  : rond obligatoire, losange optionnel (D28) ;
//   le REMPLISSAGE dit l'etat    : plein cable, creux libre ;
//   la COULEUR dit le TYPE       : hachee depuis TypeDesc::name.
//
// ⚠ C'EST UN ARBITRAGE, et il touche au critere 4.6. La couleur d'un port
// disait jusqu'ici son ETAT ; elle dit desormais son TYPE, parce que c'est le
// type qu'un cablage a besoin de lire et qu'aucun autre canal ne le portait,
// alors que l'etat, lui, en a trois autres a sa disposition. Les trois etats
// nommes restent DISTINCTS -- plein, creux terni, creux cercle d'alerte -- et
// ils le restent meme pour un oeil qui ne separe pas les teintes, la forme et
// le remplissage suffisant a les distinguer.
void DrawPortGlyph (const cggraph::TypeDesc *type, cggraph::PortState state, bool optional)
{
	const float line = ImGui::GetTextLineHeight ();
	const ImVec2 origin = ImGui::GetCursorScreenPos ();
	ImGui::Dummy (ImVec2 (kGlyphSize, line));

	ImDrawList *draw = ImGui::GetWindowDrawList ();
	const ImVec2 center (origin.x + kGlyphSize * 0.5f, origin.y + line * 0.5f);
	const float radius = kGlyphSize * 0.5f;

	const bool connected = state == cggraph::PortState::Connected;
	const float alpha = state == cggraph::PortState::MissingOptional ? 0.42f : 1.0f;
	const ImU32 color = ToU32 (PortTint (type), alpha);

	if (optional)
	{
		const ImVec2 top (center.x, center.y - radius);
		const ImVec2 right (center.x + radius, center.y);
		const ImVec2 bottom (center.x, center.y + radius);
		const ImVec2 left (center.x - radius, center.y);
		if (connected)
			draw->AddQuadFilled (top, right, bottom, left, color);
		else
			draw->AddQuad (top, right, bottom, left, color, 1.8f);
	}
	else if (connected)
		draw->AddCircleFilled (center, radius, color, 14);
	else
		draw->AddCircle (center, radius, color, 14, 1.8f);

	// Le halo ne cercle QUE ce qui empeche le calcul. Une entree optionnelle
	// libre est un etat normal : elle est ternie, jamais signalee -- c'est
	// exactement la distinction que PortState::MissingOptional existe pour
	// permettre, et la perdre ici la rendrait sans objet.
	if (state == cggraph::PortState::MissingRequired)
		draw->AddCircle (center, radius + 2.5f, ImGui::ColorConvertFloat4ToU32 (kAlert), 16, 1.6f);
}

// La bande de titre se pose APRES le noeud, jamais pendant : les bornes du
// noeud ne sont connues qu'une fois son groupe ferme, et la liste
// d'arriere-plan la glisse SOUS le contenu deja dessine sans qu'il faille le
// redessiner.
void DrawNodeHeader (cggraph::NodeId id, const ImVec2 &nodeMin, const ImVec2 &nodeMax, float bottom,
                     const Rgb &tint)
{
	ImDrawList *draw = ed::GetNodeBackgroundDrawList (id);
	if (draw == nullptr)
		return;

	const float border = ed::GetStyle ().NodeBorderWidth;
	const ImVec2 topLeft (nodeMin.x + border * 0.5f, nodeMin.y + border * 0.5f);
	const ImVec2 bottomRight (nodeMax.x - border * 0.5f, bottom);
	if (bottomRight.y <= topLeft.y + 1.0f || bottomRight.x <= topLeft.x + 1.0f)
		return;

	const float rounding = ed::GetStyle ().NodeRounding;
	const ImU32 high = ToU32 (tint, 1.0f);
	const ImU32 low = ToU32 (Scaled (tint, 0.52f), 1.0f);

	draw->AddRectFilled (topLeft, bottomRight, high, rounding, ImDrawFlags_RoundCornersTop);

	// Le degrade ne commence qu'au-dessous du rayon d'arrondi : AddRectFilled-
	// MultiColor ne sait pas arrondir, et l'appliquer plus haut redresserait les
	// deux coins que l'appel precedent vient d'arrondir.
	if (bottomRight.y > topLeft.y + rounding)
		draw->AddRectFilledMultiColor (ImVec2 (topLeft.x, topLeft.y + rounding), bottomRight, high,
		                               high, low, low);

	draw->AddLine (ImVec2 (topLeft.x, bottomRight.y - 0.5f),
	               ImVec2 (bottomRight.x, bottomRight.y - 0.5f), IM_COL32 (10, 10, 12, 210), 1.0f);
}

// Categorie d'un type de noeud, telle que la palette la groupe. Elle est lue
// PAR BALAYAGE et non par un champ de PaletteItem, qui n'en porte pas : la
// couche B est hors du perimetre de ce chantier, et le balayage coute
// exactement ce que coutait deja Palette::Find, lineaire lui aussi.
const cggraph_ui::PaletteItem *FindItem (const cggraph_ui::Palette &palette,
                                         const std::string &typeName, std::string &category)
{
	for (const cggraph_ui::PaletteCategory &group : palette.GetCategories ())
		for (const cggraph_ui::PaletteItem &item : group.items)
			if (typeName == item.typeName)
			{
				category = group.name;
				return &item;
			}
	category.clear ();
	return nullptr;
}

// Le style est pose UNE FOIS, a la construction, et non pousse a chaque frame :
// c'est un reglage, pas un etat de dessin.
//
// ⚠ LES DEUX ALPHAS DE FOND SONT CEUX DU DEFAUT, et c'est delibere : l'hote web
// dessine son apercu 3D SOUS l'interface, et rehausser l'opacite du fond de
// l'editeur le masquerait. Seuls le TON et les bordures changent.
void ApplyStyle ()
{
	ed::Style &style = ed::GetStyle ();

	style.NodeRounding = 5.0f;
	style.NodeBorderWidth = 1.0f;
	style.HoveredNodeBorderWidth = 2.5f;
	style.SelectedNodeBorderWidth = 3.0f;
	style.NodePadding = ImVec4 (9.0f, 5.0f, 9.0f, 7.0f);
	style.PinRounding = 3.0f;
	style.LinkStrength = 130.0f;

	style.Colors[ed::StyleColor_Bg] = ImVec4 (0.086f, 0.090f, 0.106f, 200.0f / 255.0f);
	style.Colors[ed::StyleColor_Grid] = ImVec4 (1.0f, 1.0f, 1.0f, 0.045f);
	style.Colors[ed::StyleColor_NodeBg] = ImVec4 (0.145f, 0.153f, 0.176f, 200.0f / 255.0f);
	style.Colors[ed::StyleColor_NodeBorder] = ImVec4 (0.03f, 0.03f, 0.04f, 0.85f);
	style.Colors[ed::StyleColor_HovNodeBorder] = ImVec4 (0.35f, 0.72f, 1.0f, 1.0f);
	style.Colors[ed::StyleColor_SelNodeBorder] = ImVec4 (1.0f, 0.72f, 0.25f, 1.0f);
	style.Colors[ed::StyleColor_PinRect] = ImVec4 (1.0f, 1.0f, 1.0f, 0.10f);
	style.Colors[ed::StyleColor_PinRectBorder] = ImVec4 (1.0f, 1.0f, 1.0f, 0.16f);
}

// Cascade de depot des noeuds crees depuis la palette : ordonnee de depart,
// ordonnee de retour a la ligne.
const float kDropFirstY = 40.0f;
const float kDropWrapY = 600.0f;

// Pose la fenetre suivante sur `rect` si `force`, en la laissant libre sinon.
// La taille est posee sous la meme condition que la position : les deux
// viennent du meme calcul, et n'en appliquer qu'une donnerait un panneau a
// moitie replace.
void ApplyPanelRect (const LayoutRect &rect, bool force)
{
	const ImGuiCond condition = force ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
	ImGui::SetNextWindowPos (ImVec2 (rect.x, rect.y), condition);
	ImGui::SetNextWindowSize (ImVec2 (rect.width, rect.height), condition);
}

void EditString (const char *label, std::string &value)
{
	char buffer[512];
	const std::size_t length = value.size () < sizeof (buffer) - 1 ? value.size () : sizeof (buffer) - 1;
	std::memcpy (buffer, value.c_str (), length);
	buffer[length] = '\0';
	if (ImGui::InputText (label, buffer, sizeof (buffer)))
		value = buffer;
}

} // namespace

NodeCanvas::NodeCanvas ()
{
	ed::Config config;

	// Aucun fichier de reglages : les positions appartiennent au document du
	// graphe, qui sait deja les ecrire. Un second endroit ou elles vivraient
	// finirait par contredire le premier.
	config.SettingsFile = nullptr;
	m_context = ed::CreateEditor (&config);

	// L'apparence appartient au CANVAS et se pose ici, une fois. La poser dans
	// un hote la ferait diverger entre les deux (D4) ; la pousser a chaque frame
	// n'ajouterait qu'une pile a tenir.
	if (m_context != nullptr)
	{
		ed::EditorContext *previous = ed::GetCurrentEditor ();
		ed::SetCurrentEditor (m_context);
		ApplyStyle ();
		ed::SetCurrentEditor (previous);
	}
}

NodeCanvas::~NodeCanvas ()
{
	if (m_context != nullptr)
		ed::DestroyEditor (m_context);
}

void NodeCanvas::Draw (cggraph_ui::EditorModel &model)
{
	// Une frame commence par retirer ce que le fil de calcul a termine. C'est le
	// pendant du §7.3 : l'hote pompe une frame, la frame recolte -- rien ne
	// remonte du fil de calcul vers l'interface autrement que par cette lecture.
	if (model.Poll ())
	{
		const cggraph::EvalResult &result = model.GetLastResult ();
		m_message = Text (result.status);
		if (!result.detail.empty ())
			m_message += " : " + result.detail;
	}

	// La disposition suit l'affichage. Sur l'hote web sa taille vient du
	// navigateur et peut changer a tout moment ; sur l'hote natif elle suit la
	// fenetre. Le recalcul est declenche par le CHANGEMENT, non par la frame :
	// le reposer a chaque frame annulerait tout ajustement manuel.
	const ImVec2 display = ImGui::GetIO ().DisplaySize;
	m_relayout = display.x != m_displayWidth || display.y != m_displayHeight;
	if (m_relayout)
	{
		m_displayWidth = display.x;
		m_displayHeight = display.y;
		m_layout = ComputeLayout (display.x, display.y);
	}

	// LE GRAPHE D'ABORD, et l'ordre compte : il est le fond, et une fenetre
	// ImGui creee apres une autre passe devant elle. Le dessiner en dernier
	// masquerait la palette et l'inspecteur a la premiere frame.
	DrawGraph (model);
	DrawPalette (model);
	DrawInspector (model);
}

void NodeCanvas::RequestFitToContent ()
{
	// Rien de plus : la demande est SERVIE dans DrawGraph, pas ici. Appeler
	// NavigateToContent depuis l'hote serait sans effet -- il n'y a ni editeur
	// courant ni frame commencee a cet instant, et la boite du contenu est
	// celle des noeuds soumis dans la frame en cours, donc vide.
	m_fitPending = true;
}

void NodeCanvas::DrawPalette (cggraph_ui::EditorModel &model)
{
	// Repose UNIQUEMENT quand la taille d'affichage a change : entre deux
	// changements l'utilisateur deplace et redimensionne comme il veut. C'est
	// aussi ce qui empeche un imgui.ini d'une session precedente de ressusciter
	// une disposition calculee pour un autre cadre -- la premiere frame compte
	// comme un changement, la taille memorisee valant zero.
	ApplyPanelRect (m_layout.palette, m_relayout);
	ImGui::Begin ("Palette");

	// ENTIEREMENT derivee du catalogue : pas un nom de type de noeud dans cette
	// fonction, donc rien a ecrire ici quand le catalogue grandit.
	for (const cggraph_ui::PaletteCategory &category : model.GetPalette ().GetCategories ())
	{
		if (!ImGui::CollapsingHeader (category.name.c_str (), ImGuiTreeNodeFlags_DefaultOpen))
			continue;

		for (const cggraph_ui::PaletteItem &item : category.items)
		{
			ImGui::PushID (item.typeName);
			if (ImGui::Button (item.label))
			{
				// A DROITE DE LA PALETTE, et non a une abscisse fixe : le graphe
				// est desormais le fond de l'affichage, la palette flotte
				// dessus, et un noeud depose a x = 40 naitrait cache derriere
				// elle. Le decalage vertical, lui, reste celui du canvas.
				const float left = m_layout.palette.x + m_layout.palette.width + m_dropX;
				const cggraph::NodeId id = model.AddNode (item.typeName, left, m_dropY);
				m_dropY += 90.0f;
				if (m_dropY > kDropWrapY)
				{
					m_dropY = kDropFirstY;
					m_dropX += 240.0f;
				}
				model.Select (id);
				m_message = std::string ("noeud ajoute : ") + item.typeName;
			}

			if (ImGui::IsItemHovered ())
			{
				ImGui::BeginTooltip ();
				ImGui::TextUnformatted (item.typeName);
				if (item.caveat != nullptr)
				{
					// La reserve du catalogue est ce que le corps emballe fait et
					// que sa declaration tait. La taire ici la perdrait pour le
					// seul lecteur qui en a besoin au moment ou il choisit.
					ImGui::Separator ();
					ImGui::PushTextWrapPos (420.0f);
					ImGui::TextUnformatted (item.caveat);
					ImGui::PopTextWrapPos ();
				}
				ImGui::EndTooltip ();
			}
			ImGui::PopID ();
		}
	}

	ImGui::End ();
}

void NodeCanvas::DrawGraph (cggraph_ui::EditorModel &model)
{
	// LE FOND DE L'AFFICHAGE, et non un panneau parmi trois. Origine (0, 0),
	// sans barre de titre, sans bordure et sans marge interieure : c'est la
	// seule configuration ou l'editeur de noeuds dessine la totalite de la zone
	// qu'il s'est allouee -- canvas_layout.h porte la mesure.
	//
	// NoBackground : le fond est deja peint par StyleColor_Bg de l'editeur de
	// noeuds. Un second aplat par-dessous ne se verrait pas sur l'hote natif et
	// masquerait l'apercu 3D sur l'hote web, que ce fond couvre desormais en
	// entier.
	const ImGuiWindowFlags flags =
	    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
	    | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar
	    | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBringToFrontOnFocus
	    | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground
	    | ImGuiWindowFlags_NoSavedSettings;

	ImGui::SetNextWindowPos (ImVec2 (m_layout.graph.x, m_layout.graph.y), ImGuiCond_Always);
	ImGui::SetNextWindowSize (ImVec2 (m_layout.graph.width, m_layout.graph.height),
	                          ImGuiCond_Always);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowPadding, ImVec2 (0.0f, 0.0f));
	ImGui::PushStyleVar (ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar (ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::Begin ("Graphe", nullptr, flags);
	ImGui::PopStyleVar (3);

	// ⚠ AMORCE OBLIGATOIRE, ET ELLE N'EST PAS DECORATIVE. La fenetre du graphe
	// est NoBackground : elle n'emet aucune geometrie avant l'editeur de noeuds.
	// Or imgui-node-editor 0.9.3 ne remet son rectangle de decoupe en
	// coordonnees d'ecran que s'il a insere une commande sentinelle, ce qu'il ne
	// fait QUE si la derniere commande de la liste de dessin est deja non vide
	// (imgui_canvas.cpp, EnterLocalSpace/LeaveLocalSpace). Sans amorce, la
	// decoupe reste en coordonnees LOCALES, et tout ce que l'editeur dessine est
	// rogne des que la vue n'est plus a l'origine ni a l'echelle 1.
	//
	// Mesure faite sur un cadre de 1600 x 1000, vue cadree sur le contenu :
	// decoupe (-262, 13)-(698, 613) sans amorce, (0, 0)-(1600, 1000) avec. La
	// meme mesure vaut pour un simple deplacement a la molette : ce n'est pas le
	// cadrage qui rogne, c'est toute vue deplacee ou zoomee.
	//
	// Un pixel, alpha 1/255 : AddRectFilled abandonne sur un alpha nul, il faut
	// donc de la geometrie reellement emise -- mais elle n'a pas a se voir.
	{
		const ImVec2 corner = ImGui::GetWindowPos ();
		ImGui::GetWindowDrawList ()->AddRectFilled (
		    corner, ImVec2 (corner.x + 1.0f, corner.y + 1.0f), IM_COL32 (0, 0, 0, 1));
	}

	cggraph::Graph &graph = model.GetGraph ();

	ed::SetCurrentEditor (m_context);
	ed::Begin ("cggraph");

	const std::vector<cggraph::NodeId> ids = graph.GetNodeIds ();
	std::size_t submitted = 0;
	for (std::size_t i = 0; i < ids.size (); ++i)
	{
		const cggraph::NodeId id = ids[i];
		const cggraph::Node *node = graph.FindNode (id);
		if (node == nullptr)
			continue;

		bool placed = false;
		for (std::size_t k = 0; k < m_placed.size (); ++k)
			if (m_placed[k] == id)
			{
				placed = true;
				break;
			}

		if (!placed)
		{
			float x = 0.0f;
			float y = 0.0f;
			graph.GetNodePosition (id, x, y);
			ed::SetNodePosition (id, ImVec2 (x, y));
			m_placed.push_back (id);
		}

		// L'etat des ports vient de la validation, jamais d'une seconde lecture
		// des liens faite ici : deux lectures divergeraient.
		const cggraph::NodeValidation validation = cggraph::ValidateNode (graph, id);
		const cggraph::NodeDesc &desc = node->GetDesc ();

		std::string category;
		const cggraph_ui::PaletteItem *item = FindItem (model.GetPalette (), desc.typeName, category);
		const char *label = item != nullptr ? item->label : desc.typeName.c_str ();
		const bool ready = validation.readiness == cggraph::NodeReadiness::Ready;

		// L'en-tete porte la teinte de la CATEGORIE : c'est ce qui fait
		// reconnaitre une famille de noeuds sans lire un titre, et la palette
		// groupe deja par elle. Un noeud hors catalogue n'en a pas ; sa teinte
		// est alors celle du nom vide, uniforme, et c'est le bon aveu.
		const Rgb headerTint = NameTint (category);

		// Les largeurs sont mesurees AVANT le dessin : aligner les sorties sur le
		// bord droit exige de connaitre la colonne de droite, et ImGui ne dispose
		// qu'en avancant.
		float inputsWidth = 0.0f;
		for (std::size_t p = 0; p < validation.inputs.size (); ++p)
		{
			const float width =
			    kGlyphSize + kGlyphGap + ImGui::CalcTextSize (validation.inputs[p].name.c_str ()).x;
			if (width > inputsWidth)
				inputsWidth = width;
		}

		float outputsWidth = 0.0f;
		for (std::size_t p = 0; p < desc.outputs.size (); ++p)
		{
			const float width =
			    kGlyphSize + kGlyphGap + ImGui::CalcTextSize (desc.outputs[p].name.c_str ()).x;
			if (width > outputsWidth)
				outputsWidth = width;
		}

		const float columnGap = inputsWidth > 0.0f && outputsWidth > 0.0f ? kColumnGap : 0.0f;
		float contentWidth = inputsWidth + columnGap + outputsWidth;
		const float titleWidth = ImGui::CalcTextSize (label).x + (ready ? 0.0f : 18.0f);
		if (titleWidth > contentWidth)
			contentWidth = titleWidth;
		if (contentWidth < kMinContentWidth)
			contentWidth = kMinContentWidth;
		const float leftWidth = contentWidth - outputsWidth - columnGap;

		// Un noeud qui bloque le calcul le dit par sa BORDURE, pas seulement par
		// un signe dans son titre : la bordure se voit sur un graphe degroupe, le
		// signe demande de lire.
		if (!ready)
			ed::PushStyleColor (ed::StyleColor_NodeBorder, kAlert);

		ed::BeginNode (id);

		// Titre clair ou sombre selon la teinte du bandeau : celle-ci est hachee,
		// donc elle tombe ou elle veut, et un titre blanc sur un jaune ne se lit
		// pas. Le signe d'alerte suit la meme regle -- mesure faite sur une
		// capture : un `!` en rouge clair sur un bandeau jaune-vert ne se
		// distinguait plus du titre.
		const bool lightHeader = Luminance (headerTint) > 0.62f;

		ImGui::BeginGroup ();
		ImGui::PushStyleColor (ImGuiCol_Text, lightHeader ? ImVec4 (0.07f, 0.07f, 0.09f, 1.0f)
		                                                  : ImVec4 (0.97f, 0.97f, 0.98f, 1.0f));
		ImGui::TextUnformatted (label);
		ImGui::PopStyleColor ();
		if (!ready)
		{
			ImGui::SameLine ();
			ImGui::TextColored (lightHeader ? ImVec4 (0.62f, 0.06f, 0.02f, 1.0f) : kAlert, "!");
		}
		ImGui::EndGroup ();
		const float headerBottom = ImGui::GetItemRectMax ().y;

		// Impose la largeur du noeud, et laisse l'air sous le titre.
		ImGui::Dummy (ImVec2 (contentWidth, kHeaderGap));

		// COLONNE DE GAUCHE -- les entrees.
		ImGui::BeginGroup ();
		for (std::size_t p = 0; p < validation.inputs.size (); ++p)
		{
			const cggraph::InputStatus &input = validation.inputs[p];
			const bool optional = static_cast<std::size_t> (input.port) < desc.inputs.size ()
			                      && desc.inputs[input.port].optional;

			ed::BeginPin (InputPinId (id, input.port), ed::PinKind::Input);
			// Le lien s'accroche au bord GAUCHE de la ligne, la ou est la
			// pastille, et non au centre du libelle : sans ce pivot, le trait
			// traverserait le nom du port.
			ed::PinPivotAlignment (ImVec2 (0.0f, 0.5f));
			ed::PinPivotSize (ImVec2 (0.0f, 0.0f));
			ImGui::BeginGroup ();
			DrawPortGlyph (input.type, input.state, optional);
			ImGui::SameLine (0.0f, kGlyphGap);
			ImGui::TextColored (PortLabelColor (input.state), "%s", input.name.c_str ());
			ImGui::EndGroup ();
			ed::EndPin ();
		}
		ReserveWidth (leftWidth);
		ImGui::EndGroup ();

		// COLONNE DE DROITE -- les sorties, alignees sur le bord du noeud.
		if (!desc.outputs.empty ())
		{
			ImGui::SameLine (0.0f, columnGap);
			ImGui::BeginGroup ();
			for (std::size_t p = 0; p < desc.outputs.size (); ++p)
			{
				const cggraph::PortDesc &output = desc.outputs[p];
				const float rowWidth =
				    kGlyphSize + kGlyphGap + ImGui::CalcTextSize (output.name.c_str ()).x;
				Indent (outputsWidth - rowWidth);

				ed::BeginPin (OutputPinId (id, static_cast<cggraph::PortIdx> (p)),
				              ed::PinKind::Output);
				ed::PinPivotAlignment (ImVec2 (1.0f, 0.5f));
				ed::PinPivotSize (ImVec2 (0.0f, 0.0f));
				ImGui::BeginGroup ();
				ImGui::TextColored (PortLabelColor (cggraph::PortState::Connected), "%s",
				                    output.name.c_str ());
				ImGui::SameLine (0.0f, kGlyphGap);
				// Une sortie n'a pas d'etat : elle est lue ou elle ne l'est pas,
				// et la validation ne se prononce que sur les entrees. La
				// dessiner « libre » exigerait une seconde lecture des liens,
				// que ce fichier s'interdit.
				DrawPortGlyph (output.type, cggraph::PortState::Connected, output.optional);
				ImGui::EndGroup ();
				ed::EndPin ();
			}
			ImGui::EndGroup ();
		}

		ed::EndNode ();
		++submitted;

		const ImVec2 nodeMin = ImGui::GetItemRectMin ();
		const ImVec2 nodeMax = ImGui::GetItemRectMax ();
		DrawNodeHeader (id, nodeMin, nodeMax, headerBottom + kHeaderGap, headerTint);

		if (!ready)
			ed::PopStyleColor ();
	}

	// CADRAGE APRES CHARGEMENT -- ici, et pas ailleurs dans la frame.
	//
	// L'editeur de noeuds cadre la boite des noeuds qu'il tient pour VIVANTS,
	// et un noeud ne l'est que de son BeginNode jusqu'au Begin suivant : la
	// boucle ci-dessus est donc le plus tot ou cette boite existe. Appeler
	// avant, c'est cadrer la frame precedente -- vide au premier affichage d'un
	// document, ou les positions viennent tout juste d'etre poussees.
	//
	// Une SEULE fois : la demande est retiree ici meme. Sans ce retrait, chaque
	// frame reprendrait la vue a l'utilisateur.
	//
	// ⚠ Duree nulle, donc pas d'animation : un chargement n'a pas de vue de
	// depart que l'oeil aurait a suivre, et une transition rendrait la premiere
	// capture d'ecran dependante du nombre de frames ecoulees.
	if (m_fitPending)
	{
		// GRAPHE VIDE : la demande est retiree SANS cadrer. Sur
		// imgui-node-editor 0.9.3 l'appel serait deja sans effet -- la vue
		// mesuree avant et apres est la meme, echelle 1 et origine (0, 0) --,
		// mais ce refus ne doit pas dependre d'un detail d'implementation
		// tierce : une version ou la boite vide vaudrait (0, 0, 0, 0) cadrerait
		// sur un point.
		if (submitted > 0)
			ed::NavigateToContent (0.0f);
		m_fitPending = false;
	}

	const std::vector<cggraph::Link> &links = graph.GetLinks ();
	for (std::size_t i = 0; i < links.size (); ++i)
	{
		// Un lien porte la couleur de son port SOURCE. C'est ce qui rend un
		// graphe lisible d'un coup d'oeil : la donnee qui circule se lit sur le
		// trait, sans avoir a en suivre l'extremite jusqu'a un libelle.
		const cggraph::Node *source = graph.FindNode (links[i].from);
		const cggraph::TypeDesc *type = nullptr;
		if (source != nullptr
		    && static_cast<std::size_t> (links[i].fromPort) < source->GetDesc ().outputs.size ())
			type = source->GetDesc ().outputs[links[i].fromPort].type;

		ed::Link (i + 1, OutputPinId (links[i].from, links[i].fromPort),
		          InputPinId (links[i].to, links[i].toPort), ToVec4 (PortTint (type), 0.95f), 2.4f);
	}

	if (ed::BeginCreate ())
	{
		ed::PinId startId;
		ed::PinId endId;
		if (ed::QueryNewLink (&startId, &endId) && startId && endId)
		{
			const PinRef start = DecodePin (startId.Get ());
			const PinRef end = DecodePin (endId.Get ());

			// Le geste part indifferemment de la sortie ou de l'entree ; le sens
			// du lien, lui, n'est pas negociable.
			const PinRef *from = start.isOutput ? &start : &end;
			const PinRef *to = start.isOutput ? &end : &start;

			if (start.isOutput == end.isOutput)
			{
				ed::RejectNewItem ();
				m_message = "un lien va d'une sortie vers une entree";
			}
			else if (ed::AcceptNewItem ())
			{
				const cggraph::ConnectStatus status =
				    model.Connect (from->node, from->port, to->node, to->port);
				m_message = Text (status);
			}
		}
	}
	ed::EndCreate ();

	if (ed::BeginDelete ())
	{
		ed::LinkId linkId;
		while (ed::QueryDeletedLink (&linkId))
		{
			const std::size_t index = static_cast<std::size_t> (linkId.Get ());
			if (index >= 1 && index <= links.size () && ed::AcceptDeletedItem ())
			{
				const cggraph::Link &link = links[index - 1];
				model.Disconnect (link.to, link.toPort);
				m_message = "lien retire";
			}
		}

		ed::NodeId nodeId;
		while (ed::QueryDeletedNode (&nodeId))
		{
			if (!ed::AcceptDeletedItem ())
				continue;

			const cggraph::NodeId id = static_cast<cggraph::NodeId> (nodeId.Get ());
			m_message = Text (model.RemoveNode (id));

			// La trace de placement suit le noeud : la laisser ferait croire
			// place un identifiant que le graphe ne porte plus.
			for (std::size_t k = 0; k < m_placed.size (); ++k)
				if (m_placed[k] == id)
				{
					m_placed.erase (m_placed.begin () + static_cast<std::ptrdiff_t> (k));
					break;
				}
		}
	}
	ed::EndDelete ();

	ed::NodeId selected;
	if (ed::GetSelectedNodes (&selected, 1) == 1)
	{
		const cggraph::NodeId id = static_cast<cggraph::NodeId> (selected.Get ());
		if (id != model.GetSelection ())
			model.Select (id);
	}

	// Les positions redescendent dans le graphe, qui est ce que la
	// serialisation ecrit : sans ce report, deplacer une boite ne survivrait pas
	// a une sauvegarde.
	for (std::size_t i = 0; i < ids.size (); ++i)
	{
		const ImVec2 position = ed::GetNodePosition (ids[i]);
		graph.SetNodePosition (ids[i], position.x, position.y);
	}

	ed::End ();
	ed::SetCurrentEditor (nullptr);

	ImGui::End ();
}

void NodeCanvas::DrawInspector (cggraph_ui::EditorModel &model)
{
	ApplyPanelRect (m_layout.inspector, m_relayout);
	ImGui::Begin ("Inspecteur");

	const cggraph_ui::Inspector &inspector = model.GetInspector ();
	if (inspector.GetNode () == cggraph::kInvalidNodeId)
	{
		ImGui::TextUnformatted ("aucun noeud selectionne");
		if (!m_message.empty ())
		{
			ImGui::Separator ();
			ImGui::TextWrapped ("%s", m_message.c_str ());
		}
		ImGui::End ();
		return;
	}

	ImGui::TextUnformatted (inspector.GetLabel ().c_str ());
	ImGui::TextDisabled ("%s", inspector.GetTypeName ().c_str ());
	if (inspector.GetCaveat () != nullptr)
	{
		ImGui::Separator ();
		ImGui::TextWrapped ("Reserve : %s", inspector.GetCaveat ());
	}

	ImGui::Separator ();
	ImGui::TextUnformatted ("Entrees");
	for (const cggraph_ui::PortField &port : inspector.GetInputs ())
		ImGui::TextColored (Color (port.state), "%s %s", Marker (port.state), port.name.c_str ());
	for (const cggraph_ui::PortField &port : inspector.GetOutputs ())
		ImGui::Text ("-> %s", port.name.c_str ());

	ImGui::Separator ();
	ImGui::TextUnformatted ("Parametres");

	// ⚠ GEL PENDANT UN CALCUL, et c'est le seul endroit du depot ou la
	// discipline du §7.3 se traduit en code. Les champs qui suivent ecrivent
	// DIRECTEMENT dans les parametres du noeud (field.value->intValue, ...), que
	// le fil de calcul lit au meme moment. Les laisser actifs serait une course
	// -- pas une course theorique : une chaine editee pendant que la signature la
	// hache. Le modele ne peut pas l'empecher, l'adresse etant deja distribuee ;
	// c'est donc l'hote qui grise.
	//
	// ⚠ NON COUVERT PAR LA SUITE DE TESTS : ce fichier n'est bati que sous
	// ENABLE_CGGRAPH_BOILERPLATE et n'est lie par aucun test (etape 4). Ce qui est
	// verifie ailleurs, c'est que EditorModel::IsEvaluating dit vrai ; que ce
	// BeginDisabled soit ecrit ne l'est pas.
	const bool frozen = model.IsEvaluating ();
	ImGui::BeginDisabled (frozen);
	if (frozen)
		ImGui::TextDisabled ("(calcul en cours : parametres geles)");

	// Un widget par TYPE de valeur, jamais par type de noeud : c'est la
	// difference entre une interface qui grandit avec le catalogue et une
	// interface qui ne grandit pas.
	for (const cggraph_ui::ParamField &field : inspector.GetParams ())
	{
		ImGui::PushID (field.name.c_str ());

		if (field.kind == cggraph::ParamKind::Driven)
		{
			// Personne ne sait evaluer une expression, et le dire vaut mieux que
			// d'offrir un champ dont le reglage n'aurait aucun effet.
			ImGui::TextDisabled ("%s = %s", field.name.c_str (), field.value->expression.c_str ());
			ImGui::SameLine ();
			ImGui::TextColored (Color (cggraph::PortState::MissingRequired), "(non evaluable)");
		}
		else
		{
			switch (field.type)
			{
			case cggraph::ParamType::Int:
				ImGui::InputInt (field.name.c_str (), &field.value->intValue);
				break;
			case cggraph::ParamType::Float:
				ImGui::InputFloat (field.name.c_str (), &field.value->floatValue);
				break;
			case cggraph::ParamType::Bool:
				ImGui::Checkbox (field.name.c_str (), &field.value->boolValue);
				break;
			case cggraph::ParamType::String:
				EditString (field.name.c_str (), field.value->stringValue);
				break;
			}

			// Un parametre non semantique n'invalide aucun calcul : le taire
			// laisserait croire que le regler recalcule la branche.
			if (field.role == cggraph::ParamRole::NonSemantic)
			{
				ImGui::SameLine ();
				ImGui::TextDisabled ("(sans effet sur le calcul)");
			}
		}

		ImGui::PopID ();
	}

	ImGui::EndDisabled ();

	ImGui::Separator ();

	// Le bouton se decide AVANT tout calcul, sur la branche entiere : c'est la
	// requete que l'evaluation ne sait pas servir, elle qui ne rend que le
	// premier refus rencontre pendant qu'elle calcule.
	const cggraph::BranchValidation branch = model.Validate (inspector.GetNode ());
	const bool ready = branch.readiness == cggraph::NodeReadiness::Ready;
	if (!ready)
	{
		ImGui::TextColored (Color (cggraph::PortState::MissingRequired), "%d port(s) a cabler",
		                    static_cast<int> (branch.GetMissingRequiredCount ()));
		for (const cggraph::NodeValidation &node : branch.nodes)
			for (const cggraph::InputStatus &input : node.inputs)
				if (input.state == cggraph::PortState::MissingRequired)
					ImGui::BulletText ("noeud %u : %s", static_cast<unsigned> (node.node),
					                   input.name.c_str ());
	}

	// PENDANT UN CALCUL, le bouton devient « Annuler » -- il n'y a rien d'autre
	// a demander tant que le fil travaille, et proposer un second calcul ferait
	// attendre l'interface exactement comme avant le fil separe.
	if (model.IsEvaluating ())
	{
		const cggraph::AsyncEvaluator::Progress progress = model.GetProgress ();
		ImGui::ProgressBar (progress.t, ImVec2 (-1.0f, 0.0f),
		                    progress.label.empty () ? "calcul en cours" : progress.label.c_str ());
		if (ImGui::Button ("Annuler"))
			model.CancelEvaluation ();
	}
	else
	{
		ImGui::BeginDisabled (!ready);
		if (ImGui::Button ("Calculer"))
		{
			model.RequestEvaluation (inspector.GetNode ());
			m_message = "calcul demande";
		}
		ImGui::EndDisabled ();
	}

	if (!m_message.empty ())
	{
		ImGui::Separator ();
		ImGui::TextWrapped ("%s", m_message.c_str ());
	}

	ImGui::End ();
}

} // namespace cggraph_canvas
