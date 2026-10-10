// ---------------------------------------------------------------------------
//  LES GESTES INJECTES : --input-script, --dump-scroll, --dump-copy
// ---------------------------------------------------------------------------
//
// Les trois INJECTENT une sequence d'evenements PAR L'ARBITRE, et les trois
// exigent d'abord une mise en page resolue -- d'ou PrepareGeometry, qui leur
// est commune. Ce qu'ils mesurent est le ROUTAGE : qui recoit l'evenement, et
// ce que le destinataire en fait.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

#include "dump_modes.h"
#include "input_router.h"
#include "viewport_rect.h"
#include "rml_host.h"
#include "window_glfw.h"

#include <RmlUi/Core/Box.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Types.h>

#include <cstdio>

namespace peles
{
namespace app
{

namespace
{

// Etat commun aux trois vidages geometriques : imposer la taille, mettre a
// jour, lire le rectangle. Rend le code de sortie a rendre en cas d'echec, 0
// sinon.
//
// ⚠ LES DEUX ECHECS GARDENT LEUR CODE. Element absent = faute de configuration
// (3) ; element de taille nulle = rien a mesurer (1). Les confondre ferait lire
// une feuille de style a corriger la ou c'est la fenetre qui est trop petite.
int PrepareGeometry (platform::RmlHost& rml, const char* elementId, int width, int height,
                     ui::ViewportRect& rect)
{
	rml.SetDimensions (width, height);
	rml.Update ();

	const ui::ViewportRectStatus status = ui::ReadViewportRect (rml.DocumentRoot (), elementId,
	                                                            rect);
	if (status == ui::ViewportRectStatus::ElementMissing)
	{
		std::fprintf (stderr, "peles : aucun element d'identifiant \"%s\" dans le document RML.\n",
		              elementId);
		return 3;
	}
	if (status == ui::ViewportRectStatus::DegenerateSize)
	{
		std::fprintf (stderr,
		              "peles : l'element \"%s\" n'occupe aucune surface pour ce cadre.\n",
		              elementId);
		return 1;
	}
	return 0;
}

} // namespace

// ---------------------------------------------------------------------------
//  --input-script
// ---------------------------------------------------------------------------

namespace
{

const char* OwnerName (ui::InputRouter::Owner owner)
{
	switch (owner)
	{
	case ui::InputRouter::Owner::Viewport: return "vue";
	case ui::InputRouter::Owner::Ui:       return "interface";
	case ui::InputRouter::Owner::None:     break;
	}
	return "aucun";
}

const char* KindName (ui::MouseEvent::Kind kind)
{
	switch (kind)
	{
	case ui::MouseEvent::Kind::Move:    return "move   ";
	case ui::MouseEvent::Kind::Press:   return "down   ";
	case ui::MouseEvent::Kind::Release: return "up     ";
	case ui::MouseEvent::Kind::Leave:   return "leave  ";
	case ui::MouseEvent::Kind::Wheel:   break;
	}
	return "wheel  ";
}

ui::MouseEvent MakeEvent (ui::MouseEvent::Kind kind, int x, int y)
{
	ui::MouseEvent event;
	event.kind    = kind;
	event.button  = ui::MouseButton::Left;
	event.windowX = x;
	event.windowY = y;
	return event;
}

} // namespace

int DumpInputScript (platform::RmlHost& rml, ui::InputRouter& router,
                     const char* viewportElementId, int width, int height)
{
	ui::ViewportRect rect;
	if (const int failure = PrepareGeometry (rml, viewportElementId, width, height, rect))
		return failure;

	router.SetViewportRect (rect);

	// DEUX POINTS, ET ILS SONT LUS DE LA GEOMETRIE, PAS ECRITS EN DUR. Le centre
	// du rectangle est dans la vue ; la moitie de la marge gauche est dans le
	// panneau qui la precede.
	const int viewX  = rect.x + rect.width / 2;
	const int viewY  = rect.y + rect.height / 2;
	const int panelX = rect.x / 2;
	const int panelY = rect.y + rect.height / 2;

	if (rect.x < 2)
	{
		std::fprintf (stderr,
		              "peles : la vue commence a x=%d : il n'y a pas de panneau a sa gauche ou "
		              "viser. SEQUENCE NON CONCLUANTE.\n",
		              rect.x);
		return 1;
	}

	struct Step
	{
		const char*             label;
		ui::MouseEvent          event;
		ui::InputRouter::Owner  expected;
	};

	const Step steps[] = {
		{ "move@vue", MakeEvent (ui::MouseEvent::Kind::Move, viewX, viewY),
		  ui::InputRouter::Owner::Viewport },
		{ "down@vue", MakeEvent (ui::MouseEvent::Kind::Press, viewX, viewY),
		  ui::InputRouter::Owner::Viewport },
		{ "move@panneau", MakeEvent (ui::MouseEvent::Kind::Move, panelX, panelY),
		  ui::InputRouter::Owner::Viewport },
		{ "up@panneau", MakeEvent (ui::MouseEvent::Kind::Release, panelX, panelY),
		  ui::InputRouter::Owner::Viewport },
		// LE SECOND VERSANT : les memes gestes, entierement sur le panneau.
		// ⚠ SANS LUI, UN ARBITRE QUI RENDRAIT TOUJOURS « vue » PASSERAIT.
		{ "move@panneau", MakeEvent (ui::MouseEvent::Kind::Move, panelX, panelY),
		  ui::InputRouter::Owner::Ui },
		{ "down@panneau", MakeEvent (ui::MouseEvent::Kind::Press, panelX, panelY),
		  ui::InputRouter::Owner::Ui },
		{ "up@panneau", MakeEvent (ui::MouseEvent::Kind::Release, panelX, panelY),
		  ui::InputRouter::Owner::Ui },
		// TROISIEME VERSANT : LA SORTIE DE CADRE.
		//
		// ⚠ ELLE EST ROUTEE COMME LES AUTRES, et c'est ce que cette etape
		// etablit : sans bouton enfonce elle ne designe personne, et elle fait
		// OUBLIER son survol a RmlUi -- la colonne « survol » passe a
		// « (aucun) ». Traitee hors de la file, elle s'appliquerait avant les
		// evenements deja en attente et le survol serait aussitot retabli.
		{ "leave", MakeEvent (ui::MouseEvent::Kind::Leave, panelX, panelY),
		  ui::InputRouter::Owner::None },
		{ "move@panneau", MakeEvent (ui::MouseEvent::Kind::Move, panelX, panelY),
		  ui::InputRouter::Owner::Ui },
	};

	std::printf ("\n--- arbitrage souris, sequence injectee ---\n");
	std::printf ("cadre                    : %dx%d\n", width, height);
	std::printf ("rectangle de la vue      : x=%d y=%d w=%d h=%d\n", rect.x, rect.y, rect.width,
	             rect.height);
	std::printf ("point dans la vue        : (%d, %d)\n", viewX, viewY);
	std::printf ("point dans le panneau    : (%d, %d)\n", panelX, panelY);
	std::printf ("\n%-14s %-8s %-12s %-22s %-10s %-10s\n", "etape", "type", "fenetre", "survol",
	             "attendu", "retenu");

	bool ok            = true;
	bool hoverLeftView = false;

	for (int i = 0; i < (int)(sizeof (steps) / sizeof (steps[0])); ++i)
	{
		const Step&                     step     = steps[i];
		const ui::InputRouter::Decision decision = router.Route (step.event);
		const std::string               hover    = router.HoverName ();

		char position[32];
		std::snprintf (position, sizeof (position), "(%d,%d)", step.event.windowX,
		               step.event.windowY);

		std::printf ("%-14s %-8s %-12s %-22s %-10s %-10s%s\n", step.label,
		             KindName (step.event.kind), position, hover.c_str (),
		             OwnerName (step.expected), OwnerName (decision.owner),
		             decision.owner == step.expected ? "" : "   <-- ECART");

		if (decision.owner != step.expected)
			ok = false;

		// ⚠ GARDE D'INUTILITE : a l'etape 3 le survol doit avoir QUITTE la vue.
		// S'il y restait, « vue pour les trois » serait vrai sans que la regle de
		// capture ait rien a voir avec le resultat.
		if (i == 2)
			hoverLeftView = hover != std::string ("#") + viewportElementId;
	}

	std::printf ("\n");
	if (!hoverLeftView)
	{
		std::fprintf (stderr,
		              "SEQUENCE NON CONCLUANTE : au deplacement sur le panneau, RmlUi survole "
		              "encore la vue. La regle de capture n'a donc rien eu a tenir.\n");
		return 1;
	}

	if (!ok)
	{
		std::fprintf (stderr,
		              "ARBITRAGE FAUX : au moins une etape n'a pas le proprietaire attendu.\n");
		return 1;
	}

	std::printf ("ARBITRAGE CONFORME : capture tenue du down sur la vue jusqu'au up sur le "
	             "panneau, et rendue ensuite.\n");
	return 0;
}


// ---------------------------------------------------------------------------
//  --dump-scroll
// ---------------------------------------------------------------------------

namespace
{

// Vrai si l'element porte une barre de defilement verticale. ⚠ Elle n'est PAS
// un enfant du DOM : RmlUi l'instancie hors document, donc il faut la demander.
bool HasVerticalScrollbar (Rml::Element* element)
{
	const int children = element->GetNumChildren (true);
	for (int i = 0; i < children; ++i)
	{
		Rml::Element* const child = element->GetChild (i);
		if (child != nullptr && child->GetTagName () == "scrollbarvertical")
			return true;
	}
	return false;
}

// Nombre maximal de crans injectes. Il borne l'instrument, pas le produit : une
// boucle sans borne sur un contenu qui ne defile pas ne terminerait jamais.
const int kMaxScrollSteps = 400;

// DERNIERE LIGNE REELLE D'UN `data-for`, ET CE N'EST PAS GetLastChild().
//
// ⚠ RmlUi INSERE LES LIGNES ENGENDREES *AVANT* L'ELEMENT DE PATRON, et laisse
// ce patron dans le DOM en `display: none` (DataViewFor::Update :
// `InsertBefore(..., element)`). Le dernier enfant est donc TOUJOURS le patron,
// de taille nulle : le mesurer rendrait « derniere ligne visible » vrai en
// permanence, quel que soit le defilement.
Rml::Element* LastLaidOutChild (Rml::Element* parent, int& laidOutCount)
{
	laidOutCount              = 0;
	Rml::Element* last        = nullptr;
	const int     children    = parent->GetNumChildren ();
	for (int i = 0; i < children; ++i)
	{
		Rml::Element* const child = parent->GetChild (i);
		if (child == nullptr || child->GetBox ().GetSize (Rml::BoxArea::Border).y <= 0.f)
			continue;
		++laidOutCount;
		last = child;
	}
	return last;
}

} // namespace

int DumpScroll (platform::RmlHost& rml, ui::InputRouter& router, const char* viewportElementId,
                int width, int height, const char* containerId, const char* contentId)
{
	ui::ViewportRect rect;
	if (const int failure = PrepareGeometry (rml, viewportElementId, width, height, rect))
		return failure;

	router.SetViewportRect (rect);

	Rml::Element* const root      = rml.DocumentRoot ();
	Rml::Element* const container = root != nullptr ? root->GetElementById (containerId) : nullptr;
	Rml::Element* const content   = root != nullptr ? root->GetElementById (contentId) : nullptr;
	if (container == nullptr || content == nullptr)
	{
		std::fprintf (stderr, "peles : element \"%s\" ou \"%s\" introuvable dans le document.\n",
		              containerId, contentId);
		return 3;
	}

	int                 rowCount = 0;
	Rml::Element* const lastRow  = LastLaidOutChild (content, rowCount);
	if (lastRow == nullptr)
	{
		std::fprintf (stderr,
		              "peles : \"%s\" n'a aucune ligne mise en page : il n'y a rien a rendre "
		              "atteignable. MESURE NON CONCLUANTE.\n",
		              contentId);
		return 1;
	}

	const float visibleHeight = container->GetClientHeight ();
	const float contentHeight = container->GetScrollHeight ();
	const bool  scrollbar     = HasVerticalScrollbar (container);

	std::printf ("\n--- defilement d'un panneau ---\n");
	std::printf ("cadre                    : %dx%d\n", width, height);
	std::printf ("conteneur                : #%s\n", containerId);
	std::printf ("contenu                  : #%s (%d ligne(s) mise(s) en page)\n", contentId,
	             rowCount);
	std::printf ("hauteur du contenu       : %.0f px\n", contentHeight);
	std::printf ("hauteur visible          : %.0f px\n", visibleHeight);
	std::printf ("barre de defilement      : %s\n", scrollbar ? "PRESENTE" : "ABSENTE");

	if (contentHeight <= visibleHeight + 1.f)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : le contenu ne deborde pas (%.0f dans %.0f). La "
		              "derniere ligne serait visible sans qu'aucun defilement existe.\n",
		              contentHeight, visibleHeight);
		return 1;
	}

	// LE POINT DE DEFILEMENT EST AU CENTRE DU CONTENEUR, EN COORDONNEES FENETRE,
	// et il passe par l'ARBITRE -- le meme que la souris reelle. Un appel direct
	// a ProcessMouseWheel mesurerait le defilement de RmlUi, pas son routage.
	const Rml::Vector2f containerOffset = container->GetAbsoluteOffset (Rml::BoxArea::Border);
	const Rml::Vector2f containerSize   = container->GetBox ().GetSize (Rml::BoxArea::Border);
	const int wheelX = (int)(containerOffset.x + 0.5f * containerSize.x);
	const int wheelY = (int)(containerOffset.y + 0.5f * containerSize.y);

	// Le survol doit etre pose avant la molette : RmlUi ne defile que sous son
	// element survole, et celui-ci n'existe qu'apres un deplacement.
	router.Route (MakeEvent (ui::MouseEvent::Kind::Move, wheelX, wheelY));
	rml.Update ();

	ui::MouseEvent wheel;
	wheel.kind    = ui::MouseEvent::Kind::Wheel;
	wheel.windowX = wheelX;
	wheel.windowY = wheelY;
	// ⚠ NEGATIF : un cran positif est un cran VERS L'AVANT, qui rapproche l'oeil
	// de la vue 3D et fait REMONTER un document. Pour descendre, il faut tirer la
	// molette vers soi.
	wheel.notches = -3.f;

	const ui::InputRouter::Decision routed = router.Route (wheel);
	rml.Update ();
	std::printf ("proprietaire de la molette : %s (survol : %s)\n", OwnerName (routed.owner),
	             router.HoverName ().c_str ());

	if (routed.owner != ui::InputRouter::Owner::Ui)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : la molette au-dessus de #%s n'a pas ete routee "
		              "vers l'interface.\n",
		              containerId);
		return 1;
	}

	float lastScroll = container->GetScrollTop ();
	int   steps      = 1;
	for (; steps < kMaxScrollSteps; ++steps)
	{
		router.Route (wheel);
		rml.Update ();
		const float scroll = container->GetScrollTop ();
		if (scroll <= lastScroll + 0.5f)
			break;
		lastScroll = scroll;
	}

	const float        scrollTop  = container->GetScrollTop ();
	const Rml::Vector2f rowOffset = lastRow->GetAbsoluteOffset (Rml::BoxArea::Border);
	const Rml::Vector2f rowSize   = lastRow->GetBox ().GetSize (Rml::BoxArea::Border);
	const float        visibleTop = container->GetAbsoluteOffset (Rml::BoxArea::Border).y;
	const float        visibleBottom = visibleTop + visibleHeight;

	std::printf ("crans injectes           : %d\n", steps);
	std::printf ("defilement atteint       : %.0f px (maximum %.0f)\n", scrollTop,
	             contentHeight - visibleHeight);
	std::printf ("bande visible (FENETRE)  : y de %.0f a %.0f\n", visibleTop, visibleBottom);
	std::printf ("derniere ligne (FENETRE) : y de %.0f a %.0f\n", rowOffset.y,
	             rowOffset.y + rowSize.y);

	const bool reachable = rowOffset.y >= visibleTop - 0.5f &&
	                       rowOffset.y + rowSize.y <= visibleBottom + 0.5f;

	if (!scrollbar)
	{
		std::fprintf (stderr,
		              "DEFILEMENT INCOMPLET : aucune barre de defilement sur #%s. La molette "
		              "seule laisse l'operateur sans prise ni reperage.\n",
		              containerId);
		return 1;
	}

	if (!reachable)
	{
		std::fprintf (stderr,
		              "DERNIERE LIGNE INATTEIGNABLE : elle reste hors de la bande visible apres "
		              "%d cran(s). %.0f px de contenu dans %.0f px de zone.\n",
		              steps, contentHeight, visibleHeight);
		return 1;
	}

	std::printf ("DERNIERE LIGNE ATTEIGNABLE : barre presente et molette routee.\n");
	return 0;
}

// ---------------------------------------------------------------------------
//  --dump-copy
// ---------------------------------------------------------------------------

namespace
{

// TEXTE SEME DANS LE PRESSE-PAPIER AVANT LE GESTE. Il n'a besoin que d'etre
// different de toute valeur que le geste pourrait y mettre.
const char* const kClipboardSentinel = "peles : sentinelle de --dump-copy";

// PREMIER ELEMENT MIS EN PAGE parmi ceux que le selecteur designe.
//
// ⚠ `QuerySelector` SEUL NE SUFFIT PAS, ET LA MESURE L'A MONTRE. `data-for`
// clone son patron pour CHAQUE ligne, et `data-if` ne retire pas l'element du
// DOM -- il lui pose `display: none`. Les quatorze lignes du tableau portent
// donc une zone copiable ; treize sont invisibles, et la premiere du document
// n'est pas celle qu'on voit. Prendre la premiere MISE EN PAGE est le seul
// critere qui designe celle de l'ecran.
Rml::Element* FirstLaidOutMatch (Rml::Element* root, const char* selector, int& matchCount)
{
	matchCount = 0;
	if (root == nullptr)
		return nullptr;

	Rml::ElementList matches;
	root->QuerySelectorAll (matches, selector);
	matchCount = (int)matches.size ();

	for (Rml::Element* const candidate : matches)
	{
		if (candidate != nullptr && candidate->GetBox ().GetSize (Rml::BoxArea::Border).y > 0.f)
			return candidate;
	}
	return nullptr;
}

std::string FocusName (Rml::Element* element)
{
	if (element == nullptr)
		return "(aucun)";
	const Rml::String& id = element->GetId ();
	if (!id.empty ())
		return std::string ("#") + std::string (id);
	return std::string (element->GetTagName ());
}

} // namespace

int DumpCopy (platform::RmlHost& rml, platform::Window& window, ui::InputRouter& router,
              const KeySink& sendKey, const char* viewportElementId, int width, int height,
              const char* selector)
{
	ui::ViewportRect rect;
	if (const int failure = PrepareGeometry (rml, viewportElementId, width, height, rect))
		return failure;

	router.SetViewportRect (rect);

	int                 matches = 0;
	Rml::Element* const element = FirstLaidOutMatch (rml.DocumentRoot (), selector, matches);

	std::printf ("\n--- selection et copie d'une ligne ---\n");
	std::printf ("cadre                    : %dx%d\n", width, height);
	std::printf ("selecteur                : %s (%d dans le DOM)\n", selector, matches);

	if (element == nullptr)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : aucun element MIS EN PAGE ne repond a \"%s\" "
		              "dans le document (%d dans le DOM).\n",
		              selector, matches);
		return 1;
	}

	// LA VALEUR ATTENDUE EST CELLE QUE LE MODELE LIE A POSEE SUR L'ELEMENT, et
	// non une chaine que l'instrument fabriquerait : comparer la copie a une
	// valeur de son propre cru ne dirait rien du contenu reellement affiche.
	const std::string expected = element->GetAttribute<Rml::String> ("value", "");
	std::printf ("valeur affichee          : \"%s\"\n", expected.c_str ());
	if (expected.empty ())
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : la ligne est vide, il n'y a rien a copier. "
		              "Passer un fichier en argument.\n");
		return 1;
	}

	// ⚠ LA SENTINELLE PEUT NE PAS S'INSTALLER, ET CE N'EST PAS UN ECHEC DU
	// PRODUIT. Une session sans bureau interactif refuse `OpenClipboard` a tout
	// le monde. Le dernier maillon -- glfw vers le systeme -- devient alors NON
	// MESURE, et l'instrument le dit au lieu de rendre un verdict sur rien ;
	// les trois autres maillons, eux, restent mesures.
	window.SetClipboardText (kClipboardSentinel);
	const bool systemClipboard = window.ClipboardText () == kClipboardSentinel;
	std::printf ("presse-papier du systeme : %s\n",
	             systemClipboard ? "accessible" : "INACCESSIBLE DANS CETTE SESSION");

	// LE POINT DE CLIC EST DANS L'ELEMENT, EN COORDONNEES FENETRE, et il passe
	// par l'ARBITRE : c'est le chemin de la souris reelle, focus compris.
	const Rml::Vector2f offset = element->GetAbsoluteOffset (Rml::BoxArea::Border);
	const Rml::Vector2f size   = element->GetBox ().GetSize (Rml::BoxArea::Border);
	const int clickX = (int)(offset.x + 0.5f * size.x);
	const int clickY = (int)(offset.y + 0.5f * size.y);
	std::printf ("clic injecte (FENETRE)   : (%d, %d)\n", clickX, clickY);

	router.Route (MakeEvent (ui::MouseEvent::Kind::Move, clickX, clickY));
	rml.Update ();
	router.Route (MakeEvent (ui::MouseEvent::Kind::Press, clickX, clickY));
	router.Route (MakeEvent (ui::MouseEvent::Kind::Release, clickX, clickY));
	rml.Update ();

	std::printf ("survol                   : %s\n", router.HoverName ().c_str ());
	Rml::Context* const context = rml.Context ();
	std::printf ("element focalise         : %s\n",
	             FocusName (context != nullptr ? context->GetFocusElement () : nullptr).c_str ());

	// Ctrl+A PUIS Ctrl+C, PAR LE CHEMIN COMPLET DU PRODUIT.
	//
	// ⚠ `sendKey` EST LA FILE DE L'HOTE, PAS L'ARBITRE. Appeler
	// `router.RouteKey` ici sauterait le drainage de la file -- le premier des
	// deux maillons du routage clavier -- et MESURE : avec ce drainage
	// neutralise, l'instrument restait VERT.
	for (const ui::Key key : { ui::Key::A, ui::Key::C })
	{
		ui::KeyEvent event;
		event.key       = key;
		event.modifiers = ui::modifier::kControl;
		event.pressed   = true;
		sendKey (event);
		event.pressed = false;
		sendKey (event);
		rml.Update ();
	}

	const int         writes    = rml.ClipboardWrites ();
	const std::string copied    = rml.LastClipboardText ();
	const std::string clipboard = window.ClipboardText ();

	std::printf ("copies demandees a l'interface : %d\n", writes);
	std::printf ("dernier texte copie      : \"%s\"\n", copied.c_str ());
	std::printf ("presse-papier apres      : \"%s\"\n", clipboard.c_str ());

	// PREMIER VERDICT : LE CHEMIN QUI VA DU CLIC A `SetClipboardText`.
	//
	// ⚠ ZERO COPIE DEMANDEE EST LA SIGNATURE COMMUNE DE TROIS PANNES, et c'est
	// pourquoi le compte est releve : `WidgetTextInput::CopySelection` n'est
	// appelee que si le controle a le FOCUS, si la touche lui a ete ROUTEE et si
	// la SELECTION est non vide. Aucun autre verdict du lot ne les distingue de
	// « Ctrl+C ne fait rien ».
	if (writes == 0)
	{
		std::fprintf (stderr,
		              "LIGNE NI SELECTIONNABLE NI COPIABLE : l'interface n'a demande AUCUNE "
		              "copie. Le clic n'a pas donne le focus, les touches n'ont pas ete "
		              "routees, ou la selection est restee vide.\n");
		return 1;
	}

	if (copied != expected)
	{
		std::fprintf (stderr,
		              "SELECTION PARTIELLE : l'interface a demande la copie de \"%s\" la ou la "
		              "ligne affiche \"%s\".\n",
		              copied.c_str (), expected.c_str ());
		return 1;
	}

	// SECOND VERDICT : LE DERNIER MAILLON, DE glfw VERS LE SYSTEME.
	if (!systemClipboard)
	{
		std::printf ("LIGNE SELECTIONNABLE ET COPIABLE, SOUS UNE RESERVE NOMMEE : le texte "
		             "affiche a bien ete remis a SetClipboardText. Le dernier maillon -- glfw "
		             "vers le presse-papier du systeme -- N'EST PAS MESURE ici, la session ne "
		             "donnant pas acces au presse-papier.\n");
		return 0;
	}

	if (clipboard != expected)
	{
		std::fprintf (stderr,
		              "COPIE NON TRANSMISE AU SYSTEME : l'interface a demande \"%s\", le "
		              "presse-papier porte \"%s\".\n",
		              copied.c_str (), clipboard.c_str ());
		return 1;
	}

	std::printf ("LIGNE SELECTIONNABLE ET COPIABLE : le presse-papier du systeme porte la "
	             "valeur affichee.\n");
	return 0;
}

} // namespace app
} // namespace peles
