#pragma once

#include <RmlUi/Core/SystemInterface.h>

#include <functional>
#include <string>

namespace peles
{
namespace platform
{

// Puits de journalisation de l'hote. Le type est un std::function et non un
// appel direct a cgre::Log : peles_platform NE LIE PAS cgre, et ne doit pas le
// lier -- c'est le pare-feu d'inclusion qui l'interdit.
using LogSink = std::function<void (const std::string&)>;

// Puits du curseur : recoit le nom RCSS demande par RmlUi (`ew-resize`,
// `ns-resize`, `text`, ... ; chaine vide pour le curseur par defaut). Meme
// raison que LogSink pour un std::function : cette interface ne connait pas la
// fenetre, c'est l'hote qui relie les deux.
using CursorSink = std::function<void (const std::string&)>;

// SystemInterface minimale : horloge, journal, presse-papier et curseur.
//
// ⚠ DUREE DE VIE. RmlUi ne prend qu'un POINTEUR NON PROPRIETAIRE sur cet objet
// (Rml::SetSystemInterface). Il doit survivre jusqu'APRES Rml::Shutdown().
// C'est RmlHost qui tient ce contrat, par son ordre de declaration de membres.
class SystemInterface final : public Rml::SystemInterface
{
public:
	SystemInterface (LogSink sink, CursorSink cursor);

	double GetElapsedTime () override;
	bool   LogMessage (Rml::Log::Type type, const Rml::String& message) override;

	// ---------------------------------------------------------------------
	//  LE PRESSE-PAPIER DU SYSTEME, ET NON CELUI DE RmlUi
	// ---------------------------------------------------------------------
	//
	// ⚠ SANS CES DEUX SURCHARGES, Ctrl+C N'EST PAS UNE COPIE. L'implementation
	// par defaut de Rml::SystemInterface ecrit dans une chaine MEMBRE : le texte
	// selectionne dans un controle de RmlUi ne sort jamais du processus, et
	// l'utilisateur constate que « ca ne colle rien » sans qu'aucun canal ne le
	// signale. C'est la moitie de la regression D-5 ; l'autre moitie est le
	// routage clavier, qui est ce qui fait arriver Ctrl+C jusqu'ici.
	void SetClipboardText (const Rml::String& text) override;
	void GetClipboardText (Rml::String& text) override;

	// CE QUE RmlUi A DEMANDE DE COPIER, ET COMBIEN DE FOIS.
	//
	// ⚠ CES DEUX RELEVES NE SONT PAS UN DOUBLON DU PRESSE-PAPIER DU SYSTEME, ET
	// ILS NE LE REMPLACENT PAS. Ils etablissent que le clic a donne le FOCUS,
	// que les touches ont ete ROUTEES et que la SELECTION etait non vide -- sans
	// quoi `WidgetTextInput::CopySelection` n'est pas appelee du tout. Ce qu'ils
	// n'etablissent pas est le dernier maillon, glfw vers le systeme : il se
	// mesure en relisant le presse-papier, ce que Window sait faire.
	int                ClipboardWrites () const { return m_clipboardWrites; }
	const std::string& LastClipboardText () const { return m_lastClipboardText; }

	// ---------------------------------------------------------------------
	//  LE CURSEUR
	// ---------------------------------------------------------------------
	//
	// ⚠ SANS CETTE SURCHARGE, LA PROPRIETE `cursor` DE LA FEUILLE DE STYLE EST
	// SANS EFFET : l'implementation par defaut de Rml::SystemInterface ne fait
	// rien. RmlUi l'appelle quand le nom calcule change -- celui de l'element
	// survole, ou celui de l'element TIRE pendant un glisser
	// (`Context::UpdateHoverChain`) : un separateur garde donc son curseur de
	// redimensionnement meme quand le pointeur passe sur la vue 3D.
	void SetMouseCursor (const Rml::String& cursor_name) override;

	// Dernier nom demande par RmlUi, pour l'instrument. Il etablit que la
	// feuille de style est lue ; la traduction vers le curseur du systeme se
	// relit sur la fenetre.
	const std::string& LastCursorName () const { return m_lastCursorName; }

	// ---------------------------------------------------------------------
	//  LE JOURNAL EST LE SEUL CANAL DE DIAGNOSTIC DE RmlUi
	// ---------------------------------------------------------------------
	//
	// ⚠ RmlUi NE REND PAS D'ERREUR SUR UNE FAUTE DE DOCUMENT : il journalise en
	// LT_ERROR et POURSUIT. Un `data-model` mal orthographie fait afficher
	// quatre panneaux vides, et tout le reste -- creation, chargement, boucle,
	// concordance du viewport -- reste vert. Tant que l'hote ne LIT pas ce
	// canal, toute faute de document est silencieuse, et les etapes suivantes
	// ajoutent du RML a chaque fois.
	//
	// ⚠ LT_INFO N'EST PAS COMPTE, ET C'EST LA SEULE EXCLUSION. RmlUi emet des
	// LT_INFO en fonctionnement normal -- le chargement de chaque police en est
	// un, debogueur compris -- donc les compter rendrait un faux positif a la
	// premiere execution normale.
	//
	// ⚠ LT_WARNING EST COMPTE, ET CE N'EST PAS UN EXCES DE ZELE : MESURE au
	// niveau du journal, un `data-for` dont la source n'est PAS LIEE produit
	// « Could not add data-for view to element » en LT_WARNING, PAS en LT_ERROR
	// -- et son effet est un panneau VIDE, exactement la panne que ce compteur
	// existe pour attraper. MESURE du faux positif : sur le document du produit,
	// `--frames 2` et `--frames 2 --debugger` rendent ZERO message de niveau
	// LT_WARNING ou superieur.
	int ErrorCount () const { return m_errors; }
	int WarningCount () const { return m_warnings; }
	// ⚠ LES COMPTEURS NE SONT JAMAIS REMIS A ZERO, ET C'EST UNE DECISION. Ils
	// couvrent la VIE ENTIERE de l'hote : un message emis a la creation doit
	// encore peser sur le code de sortie rendu apres la boucle d'images. Une
	// remise a zero n'aurait d'appelant que si un controle voulait ignorer ce
	// qui precede -- ce qu'aucun n'a le droit de faire.

	// Premier message compte depuis la creation de l'hote.
	// ⚠ LE PREMIER, PAS LE DERNIER : une faute en produit souvent une cascade,
	// et c'est la tete de la cascade qui nomme la cause.
	const std::string& FirstProblem () const { return m_firstProblem; }

private:
	LogSink     m_sink;
	CursorSink  m_cursor;
	std::string m_lastCursorName;
	int         m_errors   = 0;
	int         m_warnings = 0;
	std::string m_firstProblem;

	int         m_clipboardWrites = 0;
	std::string m_lastClipboardText;
};

} // namespace platform
} // namespace peles
