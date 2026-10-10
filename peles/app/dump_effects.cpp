// ---------------------------------------------------------------------------
//  L'EFFET D'UN GESTE : --alt-pivot, --drag, --drag-bump, --click
// ---------------------------------------------------------------------------
//
// Ces quatre-la ne routent rien : ils comparent un AVANT et un APRES -- pivot
// au bit pres, angle d'orbite, position de l'oeil, genre des commandes
// produites. L'hote leur remet ce qu'il a releve dans le chemin du produit.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

#include "dump_modes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace peles
{
namespace app
{

// ---------------------------------------------------------------------------
//  --alt-pivot
// ---------------------------------------------------------------------------

namespace
{

// Representation binaire d'un flottant, pour que « au bit pres » soit litteral.
unsigned int FloatBits (float value)
{
	unsigned int bits = 0;
	std::memcpy (&bits, &value, sizeof (bits));
	return bits;
}

void PrintPivot (const char* label, const float pivot[3])
{
	std::printf ("%-24s : (%.9g, %.9g, %.9g)  octets %08x %08x %08x\n", label, pivot[0], pivot[1],
	             pivot[2], FloatBits (pivot[0]), FloatBits (pivot[1]), FloatBits (pivot[2]));
}

const char* PivotStatusName (view3d::PivotPickStatus status)
{
	switch (status)
	{
	case view3d::PivotPickStatus::Ok:            return "Ok (surface touchee)";
	case view3d::PivotPickStatus::NoView:        return "NoView (la vue n'a pas de dimensions)";
	case view3d::PivotPickStatus::OutOfViewport: return "OutOfViewport (pixel hors rectangle)";
	case view3d::PivotPickStatus::NoHit:         break;
	}
	return "NoHit (aucune surface sous le curseur)";
}

} // namespace

bool ReportAltPivot (int windowX, int windowY, bool reachedViewport,
                     view3d::PivotPickStatus status, const float before[3], const float after[3])
{
	const bool identical    = std::memcmp (before, after, 3 * sizeof (float)) == 0;
	const bool surfaceFound = status == view3d::PivotPickStatus::Ok;

	std::printf ("\n--- Alt + clic gauche : le pivot ---\n");
	std::printf ("%-24s : (%d, %d)\n", "pixel vise (FENETRE)", windowX, windowY);
	std::printf ("%-24s : %s\n", "proprietaire retenu", reachedViewport ? "vue" : "PAS LA VUE");
	// ⚠ L'ISSUE N'EST IMPRIMEE QUE SI LE GESTE EST ARRIVE. Hors de la vue, aucun
	// SetPivotFromPixel n'a couru : imprimer une valeur d'issue donnerait a lire
	// une mesure qui n'a pas ete prise.
	std::printf ("%-24s : %s\n", "issue du geste",
	             reachedViewport ? PivotStatusName (status) : "(le geste n'a pas couru)");
	std::printf ("%-24s : %s\n", "surface sous le curseur", surfaceFound ? "OUI" : "NON");
	PrintPivot ("pivot avant", before);
	PrintPivot ("pivot apres", after);
	std::printf ("%-24s : %s\n", "comparaison",
	             identical ? "IDENTIQUE AU BIT PRES" : "MODIFIE");

	// ⚠ LA PREMIERE PORTE EST CELLE DE L'ARRIVEE, PAS CELLE DU RESULTAT. Un
	// geste qui n'a jamais ete attribue a la vue laisse le pivot inchange
	// exactement comme un geste qui a vise le vide : rendre BON sur ce cas
	// reviendrait a declarer conforme la panne totale de l'arbitrage.
	if (!reachedViewport)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : l'enfoncement n'a pas ete attribue a la vue. Le "
		              "pixel (%d, %d) est-il dans le rectangle de la vue ? Rien n'a ete vise, "
		              "donc \"pivot inchange\" ne prouve rien.\n",
		              windowX, windowY);
		return false;
	}

	if (status == view3d::PivotPickStatus::OutOfViewport ||
	    status == view3d::PivotPickStatus::NoView)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : %s. Le geste est arrive jusqu'a la vue mais n'a "
		              "pas pu viser : \"pivot inchange\" ne prouve rien.\n",
		              PivotStatusName (status));
		return false;
	}

	if (!surfaceFound)
	{
		if (identical)
		{
			std::printf ("PIVOT INCHANGE : un clic qui manque ne fait pas sauter la vue.\n");
			return true;
		}
		std::fprintf (stderr,
		              "PIVOT DEPLACE SANS SURFACE : le geste a bouge le centre d'orbite alors "
		              "que rien n'etait vise.\n");
		return false;
	}

	if (!identical)
	{
		std::printf ("PIVOT POSE SUR LA SURFACE : le geste agit quand il vise quelque chose.\n");
		return true;
	}
	std::fprintf (stderr,
	              "PIVOT INCHANGE ALORS QU'UNE SURFACE ETAIT VISEE : le geste ne fait rien, et "
	              "le cas \"sans surface\" ne prouverait alors plus rien.\n");
	return false;
}

// ---------------------------------------------------------------------------
//  --drag
// ---------------------------------------------------------------------------

bool ReportDrag (int fromX, int fromY, int toX, int toY, bool arrivalInView,
                 const float axisBefore[3], const float axisAfter[3], const float pivotBefore[3],
                 const float pivotAfter[3])
{
	const float dot = std::max (-1.f, std::min (1.f, axisBefore[0] * axisAfter[0] +
	                                                     axisBefore[1] * axisAfter[1] +
	                                                     axisBefore[2] * axisAfter[2]));
	const double angleDeg = std::acos ((double)dot) * 180. / 3.14159265358979;

	const bool pivotHeld = std::memcmp (pivotBefore, pivotAfter, 3 * sizeof (float)) == 0;

	std::printf ("\n--- glisser gauche ---\n");
	std::printf ("%-26s : (%d, %d)\n", "depart (FENETRE)", fromX, fromY);
	std::printf ("%-26s : (%d, %d)\n", "arrivee (FENETRE)", toX, toY);
	std::printf ("%-26s : %s\n", "arrivee dans la vue", arrivalInView ? "OUI" : "NON -- panneau");
	std::printf ("%-26s : (%.6f, %.6f, %.6f)\n", "axe de vue avant", axisBefore[0], axisBefore[1],
	             axisBefore[2]);
	std::printf ("%-26s : (%.6f, %.6f, %.6f)\n", "axe de vue apres", axisAfter[0], axisAfter[1],
	             axisAfter[2]);
	std::printf ("%-26s : %.4f deg\n", "rotation", angleDeg);
	PrintPivot ("pivot avant", pivotBefore);
	PrintPivot ("pivot apres", pivotAfter);

	// ⚠ SEUIL EN DEGRES, ET IL SEPARE DEUX ORDRES DE GRANDEUR, PAS DEUX VALEURS
	// VOISINES : un glisser de plusieurs centaines de pixels tourne de dizaines
	// de degres, une capture perdue rend exactement zero.
	const double kMinRotationDeg = 0.1;

	if (angleDeg < kMinRotationDeg)
	{
		std::fprintf (stderr,
		              "VUE NON TOURNEE : le glisser n'a produit aucune rotation (%.4f deg). %s\n",
		              angleDeg,
		              arrivalInView
		                  ? "Le geste n'atteint pas la camera."
		                  : "La capture est tombee des que le curseur a quitte la vue.");
		return false;
	}

	if (!pivotHeld)
	{
		std::fprintf (stderr,
		              "PIVOT DEPLACE PAR UNE ORBITE : le centre de rotation a bouge, ce qui n'est "
		              "pas ce qu'un glisser gauche fait.\n");
		return false;
	}

	// ⚠ LE VERSANT « PRODUIT » DE LA CAPTURE EXIGE UNE ARRIVEE HORS DE LA VUE,
	// et le dire est tout l'objet de cette ligne. Un glisser interne tourne
	// aussi bien sans regle de capture : le declarer conforme reviendrait a
	// laisser l'operateur choisir, par ses coordonnees, si la mesure porte ou
	// non sur ce qu'elle annonce.
	if (arrivalInView)
	{
		std::printf ("ORBITE EFFECTUEE, PIVOT INCHANGE AU BIT PRES. ARRIVEE DANS LA VUE : la "
		             "regle de capture n'a rien eu a tenir, ce glisser ne la mesure pas.\n");
		return true;
	}

	std::printf ("ORBITE EFFECTUEE, PIVOT INCHANGE AU BIT PRES, ARRIVEE HORS DE LA VUE : la "
	             "capture a tenu jusqu'au relachement.\n");
	return true;
}

// ---------------------------------------------------------------------------
//  --drag-bump
// ---------------------------------------------------------------------------

bool ReportDragBump (GestureOwner owner, int fromX, int fromY, int toX, int toY,
                     double rotationDeg, bool selectionChanged, const float eyeBefore[3],
                     const float eyeAfter[3])
{
	const float dx       = eyeAfter[0] - eyeBefore[0];
	const float dy       = eyeAfter[1] - eyeBefore[1];
	const float dz       = eyeAfter[2] - eyeBefore[2];
	const double eyeMove = std::sqrt ((double)(dx * dx + dy * dy + dz * dz));

	std::printf ("\n--- geste a deux boutons ---\n");
	std::printf ("%-26s : %s\n", "bouton qui ouvre",
	             owner == GestureOwner::Left ? "gauche (orbite)" : "droit (dolly)");
	std::printf ("%-26s : %s\n", "bouton intercale",
	             owner == GestureOwner::Left ? "droit, enfonce puis relache"
	                                         : "gauche, enfonce puis relache, IMMOBILE");
	std::printf ("%-26s : (%d, %d) -> (%d, %d)\n", "glisser (FENETRE)", fromX, fromY, toX, toY);
	std::printf ("%-26s : %.4f deg\n", "rotation", rotationDeg);
	std::printf ("%-26s : %.6f\n", "deplacement de l'oeil", eyeMove);
	std::printf ("%-26s : %s\n", "selection changee", selectionChanged ? "OUI" : "non");

	// ⚠ SEUIL EN DEGRES, IDENTIQUE A CELUI DE --drag, ET POUR LA MEME RAISON :
	// il separe deux ordres de grandeur, pas deux valeurs voisines. Un geste tue
	// par le second bouton rend exactement zero.
	const double kMinRotationDeg = 0.1;
	const double kMinEyeMove     = 1e-4;

	if (owner == GestureOwner::Left)
	{
		if (rotationDeg < kMinRotationDeg)
		{
			std::fprintf (stderr,
			              "GESTE GELE PAR LE SECOND BOUTON : le glisser gauche n'a produit "
			              "aucune rotation (%.4f deg). Le relachement du bouton droit a ferme "
			              "l'unique slot d'etat du trackball.\n",
			              rotationDeg);
			return false;
		}
		if (selectionChanged)
		{
			std::fprintf (stderr,
			              "SELECTION PENDANT UN GLISSER : le clic droit intercale a fait "
			              "selectionner un modele.\n");
			return false;
		}
		std::printf ("GESTE INTACT : le clic droit intercale n'a ni tue l'orbite ni "
		             "selectionne.\n");
		return true;
	}

	if (selectionChanged)
	{
		std::fprintf (stderr,
		              "SELECTION AU MILIEU D'UN DOLLY : le clic gauche intercale, immobile, a "
		              "selectionne un modele -- son deplacement nul passe sous le seuil "
		              "clic/glisser.\n");
		return false;
	}

	// ⚠ GARDE D'INUTILITE : sans dolly, « rien de selectionne » serait vrai pour
	// la mauvaise raison -- il n'y aurait simplement pas eu de geste.
	if (eyeMove < kMinEyeMove)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : l'oeil n'a pas bouge (%.6f), donc le dolly n'a "
		              "pas eu lieu et le clic gauche intercale n'a rien eu a perturber.\n",
		              eyeMove);
		return false;
	}

	std::printf ("GESTE INTACT : le dolly a eu lieu et le clic gauche intercale n'a rien "
	             "selectionne.\n");
	return true;
}

// ---------------------------------------------------------------------------
//  --click / --double-click
// ---------------------------------------------------------------------------

namespace
{

const char* CommandName (ui::UiCommandKind kind)
{
	switch (kind)
	{
	case ui::UiCommandKind::ToggleDirectory: return "ToggleDirectory";
	case ui::UiCommandKind::SelectFile:      return "SelectFile";
	case ui::UiCommandKind::OpenFile:        break;
	}
	return "OpenFile";
}

const char* ExpectationName (ClickExpectation expected)
{
	switch (expected)
	{
	case ClickExpectation::Select: return "SelectFile";
	case ClickExpectation::Toggle: return "ToggleDirectory";
	case ClickExpectation::Open:   return "OpenFile";
	case ClickExpectation::Any:    break;
	}
	return "(non precise)";
}

bool Contains (const std::vector<ui::UiCommand>& commands, ui::UiCommandKind kind)
{
	for (const ui::UiCommand& command : commands)
	{
		if (command.kind == kind)
			return true;
	}
	return false;
}

} // namespace

bool ReportClick (bool doubleClick, ClickExpectation expected,
                  const std::vector<ui::UiCommand>& commands, int modelsBefore, int modelsAfter)
{
	std::printf ("%-26s : %s\n", "genre attendu", ExpectationName (expected));
	std::printf ("%-26s : %d -> %d\n", "modeles dans la scene", modelsBefore, modelsAfter);

	if (commands.empty ())
	{
		std::fprintf (stderr,
		              "AUCUNE COMMANDE : le clic n'a atteint aucun `data-event-`. Le cablage de "
		              "l'interface est mort.\n");
		return false;
	}

	if (expected != ClickExpectation::Any)
	{
		const ui::UiCommandKind wanted = expected == ClickExpectation::Toggle
		                                     ? ui::UiCommandKind::ToggleDirectory
		                                     : (expected == ClickExpectation::Open
		                                            ? ui::UiCommandKind::OpenFile
		                                            : ui::UiCommandKind::SelectFile);
		if (!Contains (commands, wanted))
		{
			std::fprintf (stderr,
			              "GENRE DE COMMANDE FAUX : aucune %s parmi les %d commande(s) "
			              "produites. Le clic a atteint un `data-event-`, mais pas celui "
			              "attendu -- deux rappels intervertis se presentent ainsi.\n",
			              CommandName (wanted), (int)commands.size ());
			return false;
		}
	}

	// ⚠ LE DOUBLE-CLIC EXIGE L'OUVERTURE *ET* SON EFFET. Sans
	// `data-event-dblclick`, le geste produit deux `SelectFile`, aucune
	// ouverture et aucun modele : les commandes ne sont pas vides pour autant.
	if (doubleClick)
	{
		if (!Contains (commands, ui::UiCommandKind::OpenFile))
		{
			std::fprintf (stderr,
			              "DOUBLE-CLIC SANS OUVERTURE : aucune commande OpenFile. "
			              "`data-event-dblclick` est absent, mal nomme, ou non lie.\n");
			return false;
		}
		if (modelsAfter <= modelsBefore)
		{
			std::fprintf (stderr,
			              "OUVERTURE SANS EFFET : la commande OpenFile a ete produite, mais la "
			              "scene porte toujours %d modele(s). Une commande jamais honoree se "
			              "presente a l'ecran comme une commande jamais produite.\n",
			              modelsAfter);
			return false;
		}
		std::printf ("DOUBLE-CLIC ROUTE : `data-event-dblclick` a produit une OpenFile, et la "
		             "scene a grandi.\n");
		return true;
	}

	// ⚠ ET LE CLIC SIMPLE EXIGE L'ABSENCE D'OUVERTURE. C'est le versant
	// symetrique : un simple clic qui chargerait rendrait toute traversee de la
	// liste aussi couteuse que le plus lourd des fichiers traverses.
	if (Contains (commands, ui::UiCommandKind::OpenFile))
	{
		std::fprintf (stderr,
		              "CLIC SIMPLE QUI OUVRE : une commande OpenFile a ete produite par un clic "
		              "unique. Un clic designe, un double-clic ouvre.\n");
		return false;
	}
	if (modelsAfter != modelsBefore)
	{
		std::fprintf (stderr,
		              "CLIC SIMPLE QUI CHARGE : la scene est passee de %d a %d modele(s).\n",
		              modelsBefore, modelsAfter);
		return false;
	}

	std::printf ("CLIC ROUTE : le `data-event-` a produit une commande du genre attendu, l'hote "
	             "l'a appliquee, et la scene n'a pas grandi.\n");
	return true;
}

// ---------------------------------------------------------------------------
//  --focus-loss
// ---------------------------------------------------------------------------

bool ReportFocusLoss (int windowX, int windowY, bool ownerWasViewport, bool controlSelected,
                      bool cancelSelected, bool stillHolding)
{
	std::printf ("\n--- perte de focus pendant un appui ---\n");
	std::printf ("pixel (FENETRE)          : (%d, %d)\n", windowX, windowY);
	std::printf ("proprietaire de l'appui  : %s\n", ownerWasViewport ? "vue" : "PAS LA VUE");
	std::printf ("temoin (down puis up)    : %s\n",
	             controlSelected ? "a selectionne" : "N'A RIEN SELECTIONNE");
	std::printf ("avec annulation          : %s\n",
	             cancelSelected ? "A SELECTIONNE" : "n'a rien selectionne");
	std::printf ("bouton encore enfonce    : %s\n", stillHolding ? "OUI" : "non");

	if (!ownerWasViewport)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : l'appui n'a pas ete rendu a la vue 3D. Choisir "
		              "un pixel dans le rectangle de la vue.\n");
		return false;
	}

	if (!controlSelected)
	{
		std::fprintf (stderr,
		              "MESURE NON CONCLUANTE : la sequence NORMALE sur ce pixel ne selectionne "
		              "rien. \"Rien n'a ete selectionne\" serait alors vrai pour la mauvaise "
		              "raison. Choisir un pixel ou un modele est visible.\n");
		return false;
	}

	if (cancelSelected)
	{
		std::fprintf (stderr,
		              "GESTE TERMINE AU LIEU D'ETRE ANNULE : la perte de focus a selectionne le "
		              "modele sous le curseur. Le relachement synthetique de glfw porte un "
		              "deplacement nul, donc il passe pour un clic.\n");
		return false;
	}

	if (stillHolding)
	{
		std::fprintf (stderr,
		              "CAPTURE COINCEE : un bouton reste enfonce apres la perte de focus.\n");
		return false;
	}

	std::printf ("GESTE ANNULE : la perte de focus ferme le geste sans selectionner, et rend "
	             "la capture.\n");
	return true;
}

} // namespace app
} // namespace peles
