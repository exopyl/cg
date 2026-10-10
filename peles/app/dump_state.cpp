// ---------------------------------------------------------------------------
//  LE RELEVE D'ETAT : --state-audit, GL_VIEWPORT, --readpixel, --dump-timing
// ---------------------------------------------------------------------------
//
// Ces quatre instruments LISENT L'ETAT DE LA MACHINE A DESSINER et n'injectent
// rien : l'etat GL de part et d'autre d'une cloture, le cadre pose par la
// phase 5, un pixel du tampon arriere, le temps d'une image. C'est la seule
// unite de l'instrument qui appelle GL directement.
//
// Les declarations sont dans app/dump_modes.h, commun aux six unites.

// ORDRE D'INCLUSION CONTRAINT : gl_wrapper.h est le SEUL en-tete GL de cette
// unite et il vient en premier.
#include "gl_wrapper.h"

#include "dump_modes.h"
#include "viewport_rect.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace peles
{
namespace app
{

namespace
{

// VALEUR PREDITE DU PIXEL, et non valeur relevee. Le materiau par defaut de
// cgre a une diffuse (0.1, 0.5, 0.8), une ambiante, une speculaire et une
// emission nulles (MaterialRenderer::ActivateDefaultMaterial). Sous l'ECLAIRAGE
// DE MESURE de la vue -- lampe directionnelle unique colineaire a l'axe de vue,
// tous les autres termes noirs -- et avec N.L = 1 sur une face axiale vue de
// face, la couleur vaut la diffuse seule : round(255 x {0.1, 0.5, 0.8}).
//
// ⚠ CETTE PREDICTION NE VAUT QUE SOUS LA POSE DE MESURE COMPLETE : rotation
// frontale, ombrage plat, ET eclairage de mesure. Sous l'eclairage produit --
// deux lampes obliques, ambiante globale 0,12 -- elle ne s'applique pas, et
// c'est voulu : l'oracle ne doit pas prescrire l'eclairage du produit.
//
// C'est la meme prediction que celle de peles_probe, et c'est la condition pour
// que les deux mesures se comparent.
const unsigned char kPredictedPixel[3] = { 26, 128, 204 };

// ⚠ TOLERANCE ESTIMEE, NON CALIBREE : elle couvre l'arrondi vers l'entier, pas
// une dispersion mesuree sur un parc de pilotes. La valeur brute est imprimee a
// chaque execution, c'est par elle que la tolerance definitive se fixera.
const int kPixelTolerance = 2;

double Median (std::vector<double> values)
{
	if (values.empty ())
		return 0.;
	std::sort (values.begin (), values.end ());
	const size_t middle = values.size () / 2;
	if ((values.size () % 2) == 1)
		return values[middle];
	return 0.5 * (values[middle - 1] + values[middle]);
}

std::string JoinNames (const std::vector<std::string>& names)
{
	std::string text;
	for (const std::string& name : names)
	{
		if (!text.empty ())
			text += ", ";
		text += name;
	}
	return text;
}

} // namespace


// ---------------------------------------------------------------------------
//  --state-audit
// ---------------------------------------------------------------------------

void FrameAudit::BeginFrame ()
{
	scene.measured = false;
	ui.measured    = false;
}

void FrameAudit::Accumulate (int frame)
{
	++frames;

	bool                           diverging = false;
	const GlStatePhaseAudit* const phases[]  = { &scene, &ui };
	for (const GlStatePhaseAudit* phase : phases)
	{
		if (!phase->measured)
			continue;

		lastMeasured      = *phase;
		lastMeasuredFrame = frame;

		const std::vector<std::string> differing = phase->before.DifferingStates (phase->after);
		if (differing.empty ())
			continue;

		diverging = true;
		if (first.captured)
			continue;

		first.captured = true;
		first.frame    = frame;
		first.phase    = phase->phase;
		first.states   = JoinNames (differing);
		first.before   = phase->before;
		first.after    = phase->after;
	}

	if (diverging)
		++divergingFrames;
}

bool ReportStateAudit (const FrameAudit& audit)
{
	std::printf ("\n--- releve d'etat GL ---\n");

	if (audit.frames == 0)
	{
		std::printf ("AUDIT NON CONCLUANT : aucune image n'a ete RENDUE -- aucune des deux "
		             "phases sous cloture n'a couru, il n'y a rien a comparer.\n");
		return false;
	}

	const bool             diverged = audit.divergingFrames > 0;
	const GlStateSnapshot& before   = diverged ? audit.first.before : audit.lastMeasured.before;
	const GlStateSnapshot& after    = diverged ? audit.first.after : audit.lastMeasured.after;
	const int              frame    = diverged ? audit.first.frame : audit.lastMeasuredFrame;
	const std::string      phase =
	    diverged ? audit.first.phase : std::string (audit.lastMeasured.phase);

	std::printf ("%d etat(s) releves, %d image(s) rendue(s), 2 phases sous cloture par image.\n",
	             before.StateCount (), audit.frames);
	// Le point de mesure de GL_CURRENT_PROGRAM n'est pas celui des autres, et un
	// releve qui ne le dirait pas se lirait de travers.
	std::printf ("GL_CURRENT_PROGRAM du releve APRES est lu A LA SORTIE DU MOTEUR, avant que la "
	             "cloture ne pose glUseProgram(0) ; tout le reste est lu apres depilement.\n");
	std::printf ("releve imprime : image no %d, %s (%s)\n", frame, phase.c_str (),
	             diverged ? "PREMIERE image divergente" : "derniere image mesuree");

	std::printf ("AVANT :\n%s", before.Describe ().c_str ());
	std::printf ("APRES :\n%s", after.Describe ().c_str ());

	if (!diverged)
	{
		std::printf ("AUDIT OK : les %d etats releves sont identiques avant et apres, sur les %d "
		             "image(s) rendue(s) et sur les deux phases.\n",
		             before.StateCount (), audit.frames);
		return true;
	}

	std::printf ("AUDIT ECHOUE : %d image(s) sur %d divergent -- CE COMPTE N'EST PAS UN TAUX, "
	             "la comparaison est intra-image et un etat qui fuit est deja present des DEUX "
	             "cotes des l'image suivante.\n",
	             audit.divergingFrames, audit.frames);
	std::printf ("premiere divergence : image no %d, %s. Etats fautifs : %s\n", audit.first.frame,
	             audit.first.phase.c_str (), audit.first.states.c_str ());
	return false;
}

// ---------------------------------------------------------------------------
//  Concordance du GL_VIEWPORT
// ---------------------------------------------------------------------------

void ViewportTracker::Check (const ui::GlViewport& expected)
{
	GLint actual[4] = { 0, 0, 0, 0 };
	glGetIntegerv (GL_VIEWPORT, actual);

	++m_checked;

	const int wanted[4] = { expected.x, expected.y, expected.width, expected.height };

	m_hasLastActual = true;
	for (int i = 0; i < 4; ++i)
		m_lastActual[i] = (int)actual[i];

	bool same = true;
	for (int i = 0; i < 4; ++i)
		same = same && ((int)actual[i] == wanted[i]);
	if (same)
		return;

	++m_mismatches;
	if (m_captured)
		return;

	m_captured = true;
	m_frame    = m_checked;
	for (int i = 0; i < 4; ++i)
	{
		m_expected[i] = wanted[i];
		m_actual[i]   = (int)actual[i];
	}
}

bool ViewportTracker::Report () const
{
	if (m_mismatches == 0)
	{
		std::printf ("GL_VIEWPORT concordant avec le rectangle lu sur %d image(s).\n", m_checked);
		return true;
	}

	std::fprintf (stderr,
	              "peles : GL_VIEWPORT DIVERGENT du rectangle lu sur %d image(s) sur %d. "
	              "Premiere divergence a l'image no %d : attendu (%d, %d, %d, %d), releve "
	              "(%d, %d, %d, %d).\n",
	              m_mismatches, m_checked, m_frame, m_expected[0], m_expected[1], m_expected[2],
	              m_expected[3], m_actual[0], m_actual[1], m_actual[2], m_actual[3]);
	return false;
}

// ---------------------------------------------------------------------------
//  --readpixel
// ---------------------------------------------------------------------------

void PixelProbe::Read (int x, int y)
{
	glFinish ();
	// LE TAMPON ARRIERE, AVANT TOUT ECHANGE. Le tampon avant est soumis au test
	// d'appartenance des pixels : une fenetre partiellement recouverte y rendrait
	// des valeurs indefinies.
	glReadBuffer (GL_BACK);
	glPixelStorei (GL_PACK_ALIGNMENT, 1);
	glReadPixels (x, y, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, m_pixel);
	m_read = true;
}

bool PixelProbe::Report (int x, int y, int modelCount, const int* viewport) const
{
	std::printf ("\n--- lecture de pixel ---\n");

	if (!m_read)
	{
		std::printf ("AUCUN PIXEL LU : aucune image n'a ete rendue avec une vue 3D.\n");
		return false;
	}

	// ⚠ UNE SCENE VIDE REND LA COULEUR D'EFFACEMENT, ET RIEN NE LE DIRAIT. Le
	// dire ici est ce qui empeche « le fichier n'a pas ete charge » de se lire
	// comme une mesure.
	std::printf ("modeles dans le document : %d\n", modelCount);
	std::printf ("pose de mesure           : rotation frontale (identite), ombrage plat, "
	             "eclairage de mesure\n");

	// ⚠ LE RECTANGLE IMPRIME EST LE RELEVE, PAS L'ATTENDU. Sous divergence du
	// GL_VIEWPORT, l'attendu decrirait autre chose que ce qui a servi a dessiner.
	bool inside = false;
	if (viewport != nullptr)
	{
		std::printf ("rectangle GL de la vue   : x=%d y=%d w=%d h=%d (RELEVE) -- centre (%d, %d)\n",
		             viewport[0], viewport[1], viewport[2], viewport[3],
		             viewport[0] + viewport[2] / 2, viewport[1] + viewport[3] / 2);
		inside = x >= viewport[0] && x < viewport[0] + viewport[2] && y >= viewport[1] &&
		         y < viewport[1] + viewport[3];
	}
	else
	{
		std::printf ("rectangle GL de la vue   : inconnu -- aucune image rendue avec une vue.\n");
	}

	std::printf ("pixel lu                 : (%d, %d) = (%d,%d,%d)\n", x, y, (int)m_pixel[0],
	             (int)m_pixel[1], (int)m_pixel[2]);
	std::printf ("valeur predite           : (%d,%d,%d)\n", (int)kPredictedPixel[0],
	             (int)kPredictedPixel[1], (int)kPredictedPixel[2]);

	int worst = 0;
	for (int c = 0; c < 3; ++c)
		worst = std::max (worst, std::abs ((int)m_pixel[c] - (int)kPredictedPixel[c]));

	std::printf ("ecart max                : %d/255 (tolerance ESTIMEE +-%d/255)\n", worst,
	             kPixelTolerance);

	if (modelCount == 0)
	{
		std::printf ("LECTURE NON CONCLUANTE : le document est vide, ce pixel ne mesure que la "
		             "couleur d'effacement de l'hote.\n");
		return false;
	}
	// ⚠ HORS DU RECTANGLE, LE VERDICT SERAIT JUSTE ET LE DIAGNOSTIC FAUX : le
	// pixel mesure appartient au panneau de l'interface, pas a la vue 3D, et
	// « HORS TOLERANCE » se lirait comme un defaut de rendu.
	if (!inside)
	{
		std::printf ("LECTURE NON CONCLUANTE : (%d, %d) est HORS du rectangle de la vue 3D -- ce "
		             "pixel appartient a l'interface, pas au rendu.\n",
		             x, y);
		return false;
	}
	if (worst > kPixelTolerance)
	{
		std::printf ("PIXEL HORS TOLERANCE.\n");
		return false;
	}
	std::printf ("PIXEL CONFORME.\n");
	return true;
}



void ReportTiming (const std::vector<double>& frameSeconds,
                   const std::vector<double>& updateSeconds, int treeRows, int excluded)
{
	std::printf ("\n--- temps par image ---\n");
	if (frameSeconds.empty ())
	{
		// Une mediane de 0 ms sous un repere de 16,6 ms se lirait comme un
		// succes confortable : il n'y a pas de mediane d'un ensemble vide.
		std::printf ("aucune image mesuree (%d ecartee(s), image du rappel) : il n'y a pas de "
		             "mediane a rendre.\n",
		             excluded);
		return;
	}

	const double median = Median (frameSeconds);
	double       worst  = 0.;
	for (const double value : frameSeconds)
		worst = std::max (worst, value);

	std::printf ("lignes d'arbre liees   : %d\n", treeRows);
	std::printf ("images mesurees        : %d\n", (int)frameSeconds.size ());
	std::printf ("ecartees (image rappel): %d\n", excluded);
	std::printf ("image, 1re             : %.3f ms\n", frameSeconds.front () * 1000.);
	std::printf ("image, mediane         : %.3f ms\n", median * 1000.);
	std::printf ("image, maximum         : %.3f ms\n", worst * 1000.);

	if (!updateSeconds.empty ())
	{
		double updateWorst = 0.;
		for (const double value : updateSeconds)
			updateWorst = std::max (updateWorst, value);

		// ⚠ LA PREMIERE IMAGE EST IMPRIMEE A PART, ET C'EST ELLE QUI PORTE LA
		// MESURE. C'est la seule ou les lignes sont CONSTRUITES et MISES EN
		// PAGE : les suivantes ne trouvent rien de sali, donc leur Update()
		// n'est pas le cout d'un `data-for` de N lignes -- c'est le cout de
		// constater qu'il n'y a rien a faire. Lire la mediane seule ferait
		// conclure que N n'a aucune influence.
		std::printf ("Update(), 1re          : %.3f ms\n", updateSeconds.front () * 1000.);
		std::printf ("Update(), mediane      : %.3f ms\n", Median (updateSeconds) * 1000.);
		std::printf ("Update(), maximum      : %.3f ms\n", updateWorst * 1000.);
	}

	// ⚠ LE SEUIL DE 16,6 ms EST UNE ESTIMATION, PAS UNE EXIGENCE PRODUIT. Son
	// role est de detecter un ordre de grandeur aberrant -- un reflow complet par
	// image -- pas de qualifier la performance. La valeur mesuree est imprimee
	// quelle qu'elle soit.
	std::printf ("reference (ESTIMEE, non normative) : 16,600 ms\n");
}


} // namespace app
} // namespace peles
