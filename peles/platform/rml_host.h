#pragma once

#include "rml_system_interface.h"

#include <memory>
#include <string>

namespace Rml
{
class Context;
class Element;
class ElementDocument;
}

namespace peles
{
namespace platform
{

class RenderInterfaceGL2;

// Cycle de vie de RmlUi : interfaces, initialisation, contexte, document.
//
// ⚠ LE PIEGE DOCUMENTE, ET IL EST TENU PAR L'ORDRE DE DECLARATION DES MEMBRES.
// RmlUi ne prend que des POINTEURS NON PROPRIETAIRES sur les interfaces
// (peles.md §2.2) : elles doivent survivre jusqu'APRES Rml::Shutdown(). Les
// membres sont detruits dans l'ordre INVERSE de leur declaration, donc
// m_system et m_render -- declares en PREMIER -- meurent en DERNIER, apres le
// Rml::Shutdown() du destructeur. Reordonner ces deux lignes suffirait a
// produire une liberation d'objet encore reference, sans aucun diagnostic.
class RmlHost
{
public:
	struct Config
	{
		std::string assetsDir;
		int         framebufferWidth  = 0;
		int         framebufferHeight = 0;
		LogSink     log;
		// Remis a SystemInterface ; vide : la propriete `cursor` est ignoree.
		CursorSink  cursor;
		bool        debugger = false;
	};

	// Interfaces, initialisation, police et CONTEXTE. Le document n'est PAS
	// charge : voir LoadDocument.
	//
	// Rend nullptr et renseigne `error` si une police ou le contexte ne peuvent
	// pas etre obtenus. ⚠ Exige un contexte OpenGL COURANT.
	static std::unique_ptr<RmlHost> Create (const Config& config, std::string& error);

	// Charge et affiche le document `documentFile` du repertoire d'assets.
	// Vide : le document du produit.
	//
	// ⚠ LE NOM EST UN PARAMETRE PARCE QU'UNE MISE EN PAGE D'EPREUVE EN A BESOIN.
	// Le document du produit colle la vue au bord HAUT -- `rect.y` vaut 0 --
	// donc le terme Y de `ui::WindowToView` n'y est exerce par aucun critere.
	// `assets/layout_probe.rml` le rend non nul, et c'est la seule chose qu'il
	// fasse.
	//
	// ⚠ ETAPE SEPAREE DE Create, ET L'ORDRE EST UNE OBLIGATION DE RmlUi : un
	// element resout son attribut `data-model` A SON ATTACHEMENT. Un modele cree
	// APRES le chargement n'est jamais retrouve -- RmlUi journalise
	// « Could not locate data model » et affiche un document VIDE, sans echouer.
	// Tout modele de donnees doit donc etre attache au contexte entre Create et
	// cet appel.
	bool LoadDocument (const std::string& documentFile, std::string& error);

	~RmlHost ();

	RmlHost (const RmlHost&)            = delete;
	RmlHost& operator= (const RmlHost&) = delete;

	// Taille du TAMPON DE DESSIN. Le lot 1 n'emploie aucune unite `dp` : pixels
	// du tampon partout, et pas de HiDPI -- consequence assumee.
	void SetDimensions (int framebufferWidth, int framebufferHeight);

	void Update ();
	void Render ();

	// Racine du document, pour lire une geometrie resolue APRES Update().
	Rml::Element* DocumentRoot () const;

	// Le contexte, pour y attacher un modele de donnees.
	//
	// ⚠ NON PROPRIETAIRE, ET SA DUREE DE VIE EST CELLE DE CET OBJET : le
	// contexte est libere par Rml::Shutdown(), appele dans le destructeur. Un
	// modele lie a ce contexte doit donc etre delie AVANT que cet hote ne
	// meure.
	Rml::Context* Context () const { return m_context; }

	// ---------------------------------------------------------------------
	//  CE QUE RmlUi A REFUSE, ET QU'IL N'A DIT QUE PAR SON JOURNAL
	// ---------------------------------------------------------------------
	//
	// ⚠ L'HOTE DOIT LIRE CE CANAL. RmlUi ne rend d'erreur ni sur un `data-model`
	// introuvable, ni sur un `data-for` dont la source n'est pas liee, ni sur un
	// attribut `data-` inconnu : il journalise en LT_ERROR et poursuit. Le
	// resultat est un document VIDE sous un code de sortie nul.
	int                ErrorCount () const;
	int                WarningCount () const;
	const std::string& FirstProblem () const;

	// CE QUE RmlUi A DEMANDE DE COPIER -- voir SystemInterface.
	int                ClipboardWrites () const;
	const std::string& LastClipboardText () const;

	// DERNIER CURSEUR DEMANDE PAR RmlUi -- voir SystemInterface.
	const std::string& LastCursorName () const;

private:
	RmlHost ();

	// ⚠ ORDRE CRITIQUE -- voir l'encadre de la classe.
	std::unique_ptr<SystemInterface>    m_system;
	std::unique_ptr<RenderInterfaceGL2> m_render;

	// Possedes par RmlUi, libere par Rml::Shutdown().
	Rml::Context*         m_context  = nullptr;
	Rml::ElementDocument* m_document = nullptr;

	// Conserve entre Create et LoadDocument, qui sont deux etapes.
	std::string m_assetsDir;

	bool m_initialised = false;
	bool m_debugger    = false;
};

} // namespace platform
} // namespace peles
