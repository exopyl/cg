#pragma once

#include "splitter_bounds.h"

#include <array>
#include <memory>

namespace Rml
{
class Element;
}

namespace peles
{
namespace ui
{

// ---------------------------------------------------------------------------
//  LES SEPARATEURS DE PANNEAUX
// ---------------------------------------------------------------------------
//
// Trois separateurs, chacun lie a UN panneau : gauche et droit reglent une
// largeur, bas une hauteur. La vue 3D paie la difference, et le C++ n'en
// calcule jamais le rectangle : il pose l'etendue du panneau, RmlUi refait la
// mise en page, et la phase 3 relit #viewport comme a chaque image.
//
// ⚠ LE GLISSER EST CELUI DE RmlUi, PAS UN GESTE DE L'HOTE. Un separateur porte
// `drag: drag` dans la feuille de style : RmlUi emet `dragstart`, `drag` puis
// `dragend` sur lui, et cette classe les ecoute. L'enfoncement sur un
// separateur est donc arbitre `Owner::Ui` par InputRouter, sans aucune regle
// nouvelle : la capture fige ce proprietaire jusqu'au relachement, et un
// glisser commence sur un separateur ne peut PAS faire tourner la camera, meme
// quand le pointeur traverse la vue. Inversement, un glisser commence dans la
// vue n'est jamais remis a RmlUi, qui ne voit donc aucun `dragstart`.
//
// ⚠ LA TAILLE PAR DEFAUT RESTE DANS LA FEUILLE DE STYLE. Un glisser pose une
// propriete `flex-basis` EN LIGNE sur le panneau, qui l'emporte sur la regle
// de peles.rcss ; le double-clic la RETIRE, et la regle reprend. Un modele de
// donnees (`data-style-flex-basis`) obligerait a recopier ici la taille par
// defaut, et a ne jamais lier une valeur vide -- RmlUi la refuse en
// LT_WARNING.
//
// ⚠ L'ETENDUE DEMANDEE EST CONSERVEE, ET C'EST ELLE QUI EST BORNEE A CHAQUE
// IMAGE, PAS L'ETENDUE AFFICHEE. Une fenetre qui retrecit reduit le panneau
// pour garder kMinViewportExtent a la vue ; la meme fenetre rendue a sa taille
// lui rend l'etendue que l'utilisateur avait choisie.
//
// ⚠ DUREE DE VIE : les ecouteurs sont ATTACHES AUX ELEMENTS DU DOCUMENT. Cet
// objet doit mourir AVANT le document, et son destructeur les detache -- sans
// quoi la destruction du document appellerait `OnDetach` sur un ecouteur
// libere.
class PanelSplitters
{
public:
	enum class Axis
	{
		Horizontal, // le separateur se deplace en x et regle une largeur
		Vertical    // le separateur se deplace en y et regle une hauteur
	};

	// UN SEPARATEUR DU DOCUMENT. Les identifiants sont ceux de peles.rml.
	struct Spec
	{
		const char*        name;        // nom court, pour les instruments
		const char*        splitterId;  // element tire a la souris
		const char*        panelId;     // element dont l'etendue change
		Axis               axis;
		core::SplitterSide side;
	};

	static constexpr int kCount = 3;
	static const std::array<Spec, kCount>& Specs ();

	PanelSplitters ();
	~PanelSplitters ();

	PanelSplitters (const PanelSplitters&)            = delete;
	PanelSplitters& operator= (const PanelSplitters&) = delete;

	// Attache les ecouteurs aux separateurs presents sous `root` et rend leur
	// nombre. `viewportElementId` designe la vue 3D, qui partage son etendue
	// avec chaque panneau.
	//
	// ⚠ UN SEPARATEUR ABSENT N'EST PAS UNE FAUTE : une mise en page d'epreuve
	// (`--document layout_probe.rml`) n'en a aucun. L'appelant qui les exige
	// compare le nombre rendu a kCount.
	int Attach (Rml::Element* root, const char* viewportElementId);

	// REAPPLIQUE LES BORNES AUX PANNEAUX REDIMENSIONNES, a partir de la mise en
	// page resolue par le dernier Update(). ⚠ A APPELER APRES CHAQUE Update() :
	// c'est aussi ce qui signale qu'une remise a zero est devenue visible dans
	// la geometrie. Rend true si une etendue a change : l'appelant refait alors
	// Update() et rappelle. Chaque axe est borne en une passe, donc le second
	// appel rend false ; l'appelant borne quand meme le nombre de passes.
	//
	// ⚠ NE TOUCHE QU'AUX PANNEAUX QU'UN GLISSER A REGLES. Un panneau a sa taille
	// par defaut garde la regle de la feuille de style, quelle que soit la
	// taille de la fenetre.
	bool Enforce ();

	// Etendue demandee par le dernier glisser sur le separateur `index`, apres
	// bornage ; -1 si le panneau a sa taille par defaut.
	int RequestedExtent (int index) const;

	// Nombre d'evenements `drag` traites, tous separateurs confondus. Un
	// instrument s'en sert pour distinguer « le glisser n'a pas eu lieu » de
	// « le glisser n'a rien change ».
	int DragEvents () const { return m_dragEvents; }

	// Nombre de remises a la taille par defaut (double-clic).
	int Resets () const { return m_resets; }

	// Rend l'index du separateur de ce nom, -1 s'il n'existe pas.
	static int IndexOf (const char* name);

private:
	class Listener;

	struct Slot
	{
		Rml::Element* splitter = nullptr;
		Rml::Element* panel    = nullptr;

		// Etendue demandee, bornee au moment du glisser ; -1 : taille par
		// defaut de la feuille de style.
		int requested = -1;

		// Etat du glisser en cours, pris a `dragstart`.
		bool dragging      = false;

		// Double-clic traite, mise en page pas encore refaite : l'etendue lue
		// est perimee, un glisser qui commencerait maintenant est ignore.
		bool resetPending  = false;
		int  startPointer  = 0;
		int  startExtent   = 0;
		int  sharedExtent  = 0;
	};

	void OnDragStart (int index, int pointer);
	void OnDrag (int index, int pointer);
	void OnReset (int index);

	int  PanelExtent (int index) const;
	int  ViewportExtent (Axis axis) const;
	// Etendue que CE panneau et la vue se partagent, les autres panneaux de
	// l'axe restant fixes -- l'etendue que borne un glisser.
	int  SharedExtent (int index) const;
	bool EnforceAxis (Axis axis);

	// Debordement de `container` le long de l'axe : somme des boites de marge
	// de ses enfants moins sa boite de contenu, zero s'il ne deborde pas.
	static int Overflow (Rml::Element* container, Axis axis);
	void ApplyExtent (int index, int extent);

	Rml::Element*                        m_viewport = nullptr;
	std::array<Slot, kCount>             m_slots;
	std::unique_ptr<Listener>            m_listener;
	int                                  m_dragEvents = 0;
	int                                  m_resets     = 0;
};

} // namespace ui
} // namespace peles
