//
//  peles -- hote RmlUi pour cgre.
//
// Fenetre glfw, document RML, vue 3D cgre au centre. L'interface est decrite en
// RML/RCSS ; le C++ ne connait aucune largeur de panneau.
//
// CODES DE SORTIE
//   0  execution normale, et tout ce qui a ete demande a reussi
//   1  echec d'initialisation, releve d'etat GL divergent ou non concluant,
//      GL_VIEWPORT divergent du rectangle lu, lecture de pixel hors tolerance,
//      ou moins d'images rendues que demandees
//   2  argument de ligne de commande invalide ou manquant
//   3  faute de CONFIGURATION : repertoire d'assets introuvable, ou document
//      RML sans l'element de la vue 3D ou sans l'un des trois separateurs
//
// ⚠ LE CODE 3 EST LE MODE DE PANNE LE PLUS PROBABLE DU LOT, et il doit etre
// bruyant : aucune fenetre n'est ouverte quand les assets manquent, et le
// message NOMME ce qui a ete cherche.
//

#include "application.h"
#include "asset_paths.h"
#include "dump_modes.h"
#include "panel_splitters.h"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <memory>
#include <string>

namespace
{

// Deplacement maximal de --drag-splitter, en pixels. Tres au-dela de tout
// ecran, tres en deca du debordement d'un `int`.
constexpr int kMaxSplitterDelta = 100000;

void PrintUsage ()
{
	std::printf (
	    "peles [options] [fichier]\n"
	    "  --assets <dir>        repertoire d'assets ; l'echec n'est PAS rattrape\n"
	    "  --frames <n>          rend n images puis sort (desactive la synchro verticale)\n"
	    "  --dump-timing         imprime le temps median d'une image\n"
	    "  --dump-layout <l>x<h> imprime le rectangle resolu de la vue 3D pour un cadre\n"
	    "                        de cette taille, sans rendre d'image, puis sort\n"
	    "  --dump-tree <dir>     imprime l'arbre de repertoires de <dir> puis sort.\n"
	    "                        N'OUVRE NI FENETRE NI CONTEXTE GL\n"
	    "  --dump-files <dir>    imprime les fichiers importables de <dir> puis sort.\n"
	    "                        N'OUVRE NI FENETRE NI CONTEXTE GL\n"
	    "  --layout-element <id> element dont --dump-layout imprime le rectangle ;\n"
	    "                        par defaut celui de la vue 3D\n"
	    "  --dump-info           imprime les panneaux d'informations du fichier passe en\n"
	    "                        argument -- les 14 proprietes et les deux arbres --\n"
	    "                        puis sort. SANS FICHIER, les 14 lignes sortent vides.\n"
	    "                        N'OUVRE NI FENETRE NI CONTEXTE GL\n"
	    "  --document <f.rml>    document RML charge, dans le repertoire d'assets ; par\n"
	    "                        defaut celui du produit. `layout_probe.rml` place la vue\n"
	    "                        SOUS une barre : c'est la seule mise en page ou le terme\n"
	    "                        Y de la conversion fenetre -> vue est non nul\n"
	    "  --browse <dir>        ouvre l'arbre sur ce repertoire au demarrage\n"
	    "  --browse-depth <n>    ouvre l'arbre jusqu'a cette profondeur (0 : la racine)\n"
	    "  --churn-tree <n>      replie et deplie la racine n fois, une par image :\n"
	    "                        le vecteur lie RETRECIT puis regrandit\n"
	    "  --reopen <fichier>    ouvre ce second fichier APRES la premiere image\n"
	    "                        rendue, ce qui exerce la destruction de la scene\n"
	    "                        avec un maillage reellement enregistre\n"
	    "  --reselect <n>        reouvre n fois le MEME fichier, une par image rendue,\n"
	    "                        et releve l'empreinte memoire apres chacune\n"
	    "  --open-second <f>     ouvre un second fichier APRES la premiere image, par le\n"
	    "                        chemin de l'interface : il s'AJOUTE a la scene\n"
	    "  --resize <l>x<h>      redimensionne la fenetre APRES la premiere image, puis\n"
	    "                        verifie que tampon, contexte RmlUi et rectangle de la\n"
	    "                        vue suivent, et que le rappel de rafraichissement a\n"
	    "                        dessine la nouvelle taille avant l'image suivante\n"
	    "  --dump-pick <l>x<h> <x> <y>\n"
	    "                        imprime le rayon monde du pixel FENETRE (x, y) pour un\n"
	    "                        cadre de cette taille, puis celui du pixel central de la\n"
	    "                        vue, et verifie qu'il est colineaire a l'axe de vue.\n"
	    "                        LA FENETRE EST OUVERTE A CETTE TAILLE et UNE IMAGE EST\n"
	    "                        RENDUE : les dimensions verifiees sont celles que le\n"
	    "                        PRODUIT a posees, pas celles de l'instrument\n"
	    "  --input-script        deroule une sequence d'evenements souris INJECTES et\n"
	    "                        imprime le proprietaire retenu pour chacun, puis sort\n"
	    "  --alt-pivot <x> <y>   injecte un Alt + clic gauche sur ce pixel FENETRE et\n"
	    "                        compare le pivot avant et apres, AU BIT PRES. Puis sort\n"
	    "  --drag <x0> <y0> <x1> <y1>\n"
	    "                        injecte un glisser gauche entre ces deux pixels FENETRE et\n"
	    "                        mesure la rotation obtenue. L'arrivee DOIT etre sur un\n"
	    "                        panneau : c'est le cas que la regle de capture couvre,\n"
	    "                        et une arrivee INTERNE ne mesure pas cette regle\n"
	    "  --drag-bump <left|right>\n"
	    "                        avec --drag : un SECOND bouton est enfonce puis relache\n"
	    "                        au milieu du geste. `left` ouvre par le gauche -- l'orbite\n"
	    "                        doit survivre au clic droit ; `right` ouvre par le droit\n"
	    "                        -- le clic gauche immobile ne doit rien selectionner\n"
	    "  --drag-splitter <left|right|bottom|pair> <delta>\n"
	    "                        tire ce separateur de panneau de <delta> pixels par\n"
	    "                        evenements injectes, apres un glisser TEMOIN parti de la\n"
	    "                        vue ; verifie l'etendue du panneau, le rectangle de la\n"
	    "                        vue et l'orientation de la camera, puis les bornes a la\n"
	    "                        reduction de la fenetre et le double-clic. Puis sort\n"
	    "                        `pair` : tire les panneaux gauche ET droit de <delta>,\n"
	    "                        puis reduit la fenetre en trois paliers\n"
	    "  --click <x> <y>       injecte un clic sur ce pixel FENETRE et imprime la chaine\n"
	    "                        complete : survol, commande produite, effet. Puis sort\n"
	    "  --double-click <x> <y>\n"
	    "                        idem, en double-clic -- ce qui OUVRE un fichier de la\n"
	    "                        liste. Le verdict EXIGE une commande OpenFile ET un\n"
	    "                        modele de plus dans la scene\n"
	    "  --click-expect <select|toggle|open>\n"
	    "                        genre de commande que --click doit produire. Sans elle,\n"
	    "                        seul le genre IMPLIQUE par --click / --double-click est\n"
	    "                        verifie\n"
	    "  --dump-scroll         fait defiler un panneau a la molette, par evenements\n"
	    "                        injectes, et mesure si sa derniere ligne devient\n"
	    "                        atteignable. Puis sort\n"
	    "  --scroll-element <conteneur> <contenu>\n"
	    "                        les deux identifiants que --dump-scroll mesure ; par\n"
	    "                        defaut panel-right et material-tree\n"
	    "  --focus-loss <x> <y>  injecte un appui immobile interrompu par une perte de\n"
	    "                        focus, et le compare a la sequence NORMALE sur le meme\n"
	    "                        pixel -- laquelle doit, elle, selectionner. Puis sort\n"
	    "  --dump-copy           clique sur la ligne copiable, injecte Ctrl+A puis\n"
	    "                        Ctrl+C, et relit le presse-papier DU SYSTEME. Puis sort\n"
	    "  --copy-element <selecteur>\n"
	    "                        selecteur RCSS de la ligne que --dump-copy mesure ; par\n"
	    "                        defaut .pcopy\n"
	    "  --dump-status         imprime la zone de statut a la fin de la boucle et\n"
	    "                        verifie qu'elle rend compte du chargement demande\n"
	    "  --readpixel <x> <y>   lit un pixel du tampon arriere -- coordonnees GL,\n"
	    "                        ORIGINE EN BAS A GAUCHE -- et le compare a la couleur\n"
	    "                        predite pour le materiau par defaut. Impose la pose de\n"
	    "                        mesure : rotation frontale et ombrage plat\n"
	    "  --state-audit         releve l'etat GL de part et d'autre de chaque phase sous\n"
	    "                        cloture (vue 3D et interface) et compare les deux\n"
	    "  --debugger            ouvre le debogueur d'interface de RmlUi\n"
	    "  --help, -h            cette aide\n");
	// ⚠ AUCUN NOMBRE D'ETATS ICI. Le releve derive du contexte GL -- le nombre
	// d'unites de texture en depend -- et il n'y a pas de contexte a l'heure ou
	// --help s'imprime : tout chiffre ecrit ici serait une recopie, donc faux
	// un jour.
}

// Rend false et ecrit sur stderr si l'argument est absent OU VIDE.
//
// ⚠ UNE VALEUR VIDE EST UNE ERREUR, PAS « OPTION NON DEMANDEE ». Une valeur vide
// est indiscernable d'une option absente, et un mode « vider et sortir » devenu
// absent fait ouvrir une fenetre a la place du vidage. C'est la meme regle que
// le nom de fichier vide, refuse plus bas.
bool ParseString (int argc, char** argv, int& index, const char* option, std::string& value)
{
	if (index + 1 >= argc)
	{
		std::fprintf (stderr, "%s attend une valeur.\n", option);
		return false;
	}
	if (argv[index + 1][0] == '\0')
	{
		std::fprintf (stderr, "%s : valeur vide.\n", option);
		return false;
	}
	value = argv[++index];
	return true;
}

// Entier decimal borne par le bas. Rend false et ecrit sur stderr si l'argument
// est absent, non numerique, hors bornes ou suivi de caracteres parasites.
//
// ⚠ LA BORNE N'EST PAS DECORATIVE. `(int)strtol` tronque en silence, et les
// deux plateformes ne tronquent pas pareil : `long` fait 64 bits sous Linux --
// 4294967297 y devient 1 -- et 32 bits sous Windows, ou strtol sature a
// LONG_MAX. Meme ligne de commande, deux comportements, aucun message.
//
// `minimum` distingue les deux usages : un nombre d'images part de 1, une
// coordonnee de pixel part de 0 -- zero y est le premier pixel, pas une valeur
// manquante.
bool ParseBoundedInt (int argc, char** argv, int& index, const char* option, long minimum,
                      int& value, long maximum = (long)INT_MAX)
{
	std::string text;
	if (!ParseString (argc, argv, index, option, text))
		return false;

	errno                 = 0;
	char*      end        = nullptr;
	const long parsed     = std::strtol (text.c_str (), &end, 10);
	const bool outOfRange = errno == ERANGE || parsed < minimum || parsed > maximum;
	if (end == text.c_str () || *end != '\0' || outOfRange)
	{
		std::fprintf (stderr, "%s : valeur invalide \"%s\" (attendu : entier de %ld a %ld).\n",
		              option, text.c_str (), minimum, maximum);
		return false;
	}
	value = (int)parsed;
	return true;
}

// Taille de cadre, sous la forme <largeur>x<hauteur>. Les deux termes passent
// par le meme controle que les autres entiers : une saisie partielle comme
// "1400x" ou "1400" doit etre refusee, pas completee par un defaut.
bool ParseSize (int argc, char** argv, int& index, const char* option, int& width, int& height)
{
	std::string text;
	if (!ParseString (argc, argv, index, option, text))
		return false;

	const std::size_t separator    = text.find_first_of ("xX");
	bool              valid        = separator != std::string::npos;
	long              parsedWidth  = 0;
	long              parsedHeight = 0;
	if (valid)
	{
		const std::string widthText  = text.substr (0, separator);
		const std::string heightText = text.substr (separator + 1);

		char* end   = nullptr;
		errno       = 0;
		parsedWidth = std::strtol (widthText.c_str (), &end, 10);
		valid       = valid && !widthText.empty () && *end == '\0' && errno != ERANGE &&
		        parsedWidth > 0 && parsedWidth <= (long)INT_MAX;

		errno        = 0;
		parsedHeight = std::strtol (heightText.c_str (), &end, 10);
		valid        = valid && !heightText.empty () && *end == '\0' && errno != ERANGE &&
		        parsedHeight > 0 && parsedHeight <= (long)INT_MAX;
	}

	if (!valid)
	{
		std::fprintf (stderr,
		              "%s : valeur invalide \"%s\" (attendu : <largeur>x<hauteur>, entiers "
		              "strictement positifs).\n",
		              option, text.c_str ());
		return false;
	}

	width  = (int)parsedWidth;
	height = (int)parsedHeight;
	return true;
}

// ---------------------------------------------------------------------------
//  LA TABLE DES OPTIONS
// ---------------------------------------------------------------------------
//
// ⚠ UNE TABLE PLUTOT QU'UNE CHAINE DE `else if`, ET LA RAISON EST UNE RAISON DE
// CROISSANCE, PAS DE VOLUME. La chaine faisait 113 lignes pour 14 noms
// d'option, soit environ sept lignes par option ET une branche de plus dans la
// fonction. Ici, une option nouvelle ajoute UNE ENTREE -- une ligne pour un
// drapeau, trois a six pour une action composee -- et la boucle qui parcourt la
// table ne bouge pas. C'est cette derniere propriete qui compte : les etapes 5
// a 7 ajoutent des options, pas des facons de les lire.
//
// ⚠ `arity` NE CONSOMME RIEN : c'est l'action qui lit ses valeurs, parce que
// c'est elle seule qui en connait le TYPE. Le champ sert au diagnostic et
// documente la table -- l'affirmer sans l'utiliser serait pire que de l'omettre,
// donc il est verifie ci-dessous.
struct ParseContext
{
	int                  argc;
	char**               argv;
	int*                 index;
	peles::app::Options* options;
	std::string*         assetsDir;
};

struct OptionSpec
{
	const char* name;

	// Nombre de VALEURS attendues apres le nom.
	int arity;

	// Captureless : la conversion vers un pointeur de fonction est ce qui permet
	// a la table d'etre `constexpr`-compatible et sans allocation.
	bool (*apply) (ParseContext&);
};

const OptionSpec kOptions[] = {
    { "--assets", 1,
      [] (ParseContext& c)
      { return ParseString (c.argc, c.argv, *c.index, "--assets", *c.assetsDir); } },
    { "--frames", 1,
      [] (ParseContext& c)
      { return ParseBoundedInt (c.argc, c.argv, *c.index, "--frames", 1, c.options->frames); } },
    { "--dump-timing", 0,
      [] (ParseContext& c) { c.options->dumpTiming = true; return true; } },
    { "--dump-layout", 1,
      [] (ParseContext& c)
      {
	      if (!ParseSize (c.argc, c.argv, *c.index, "--dump-layout", c.options->layoutWidth,
	                      c.options->layoutHeight))
		      return false;
	      c.options->dumpLayout = true;
	      return true;
      } },
    { "--readpixel", 2,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--readpixel", 0, c.options->pixelX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--readpixel", 0, c.options->pixelY))
		      return false;
	      c.options->readPixel = true;
	      return true;
      } },
    { "--dump-tree", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--dump-tree", c.options->dumpTreeDir);
      } },
    { "--dump-files", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--dump-files", c.options->dumpFilesDir);
      } },
    { "--layout-element", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--layout-element",
	                          c.options->layoutElement);
      } },
    { "--document", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--document", c.options->documentFile);
      } },
    { "--dump-info", 0,
      [] (ParseContext& c) { c.options->dumpInfo = true; return true; } },
    { "--reselect", 1,
      [] (ParseContext& c)
      { return ParseBoundedInt (c.argc, c.argv, *c.index, "--reselect", 2, c.options->reselect); } },
    { "--browse", 1,
      [] (ParseContext& c)
      { return ParseString (c.argc, c.argv, *c.index, "--browse", c.options->browseDir); } },
    { "--browse-depth", 1,
      [] (ParseContext& c) {
	      return ParseBoundedInt (c.argc, c.argv, *c.index, "--browse-depth", 0,
	                              c.options->browseDepth);
      } },
    { "--churn-tree", 1,
      [] (ParseContext& c) {
	      return ParseBoundedInt (c.argc, c.argv, *c.index, "--churn-tree", 1,
	                              c.options->churnTree);
      } },
    { "--reopen", 1,
      [] (ParseContext& c)
      { return ParseString (c.argc, c.argv, *c.index, "--reopen", c.options->reopenPath); } },
    { "--dump-pick", 3,
      [] (ParseContext& c)
      {
	      if (!ParseSize (c.argc, c.argv, *c.index, "--dump-pick", c.options->pickWidth,
	                      c.options->pickHeight) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--dump-pick", 0, c.options->pickX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--dump-pick", 0, c.options->pickY))
		      return false;
	      c.options->dumpPick = true;
	      return true;
      } },
    { "--input-script", 0,
      [] (ParseContext& c) { c.options->inputScript = true; return true; } },
    { "--click", 2,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--click", 0, c.options->clickX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--click", 0, c.options->clickY))
		      return false;
	      c.options->click = true;
	      return true;
      } },
    { "--double-click", 2,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--double-click", 0,
	                            c.options->clickX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--double-click", 0, c.options->clickY))
		      return false;
	      c.options->click       = true;
	      c.options->clickDouble = true;
	      // ⚠ LE GENRE EST IMPLIQUE, PAS SUPPOSE : un double-clic OUVRE. Il
	      // reste surchargeable par --click-expect, qui peut suivre.
	      c.options->clickExpect = peles::app::ClickExpectation::Open;
	      return true;
      } },
    { "--click-expect", 1,
      [] (ParseContext& c)
      {
	      std::string value;
	      if (!ParseString (c.argc, c.argv, *c.index, "--click-expect", value))
		      return false;
	      if (value == "select")
		      c.options->clickExpect = peles::app::ClickExpectation::Select;
	      else if (value == "toggle")
		      c.options->clickExpect = peles::app::ClickExpectation::Toggle;
	      else if (value == "open")
		      c.options->clickExpect = peles::app::ClickExpectation::Open;
	      else
	      {
		      std::fprintf (stderr,
		                    "--click-expect : valeur inconnue \"%s\" (select, toggle ou "
		                    "open).\n",
		                    value.c_str ());
		      return false;
	      }
	      return true;
      } },
    { "--drag", 4,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--drag", 0, c.options->dragFromX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--drag", 0, c.options->dragFromY) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--drag", 0, c.options->dragToX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--drag", 0, c.options->dragToY))
		      return false;
	      c.options->drag = true;
	      return true;
      } },
    { "--drag-bump", 1,
      [] (ParseContext& c)
      {
	      std::string value;
	      if (!ParseString (c.argc, c.argv, *c.index, "--drag-bump", value))
		      return false;
	      if (value == "left")
		      c.options->dragBump = peles::app::GestureOwner::Left;
	      else if (value == "right")
		      c.options->dragBump = peles::app::GestureOwner::Right;
	      else
	      {
		      std::fprintf (stderr, "--drag-bump : valeur inconnue \"%s\" (left ou right).\n",
		                    value.c_str ());
		      return false;
	      }
	      return true;
      } },
    { "--drag-splitter", 2,
      [] (ParseContext& c)
      {
	      if (!ParseString (c.argc, c.argv, *c.index, "--drag-splitter", c.options->splitterName))
		      return false;
	      if (c.options->splitterName != "pair" &&
	          peles::ui::PanelSplitters::IndexOf (c.options->splitterName.c_str ()) < 0)
	      {
		      std::fprintf (stderr,
		                    "--drag-splitter : separateur inconnu \"%s\" (left, right, bottom "
		                    "ou pair).\n",
		                    c.options->splitterName.c_str ());
		      return false;
	      }
	      // LE DEPLACEMENT EST SIGNE : il agrandit le panneau gauche vers la
	      // droite, les panneaux droit et bas vers la gauche et vers le haut.
	      // BORNE A +/- kMaxSplitterDelta : l'instrument ajoute ce deplacement a
	      // des coordonnees de pixel, et un entier proche de INT_MAX y deborderait.
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--drag-splitter",
	                            -(long)kMaxSplitterDelta, c.options->splitterDelta,
	                            (long)kMaxSplitterDelta))
		      return false;
	      c.options->dragSplitter = true;
	      return true;
      } },
    { "--alt-pivot", 2,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--alt-pivot", 0,
	                            c.options->altPivotX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--alt-pivot", 0,
	                            c.options->altPivotY))
		      return false;
	      c.options->altPivot = true;
	      return true;
      } },
    { "--dump-scroll", 0,
      [] (ParseContext& c) { c.options->dumpScroll = true; return true; } },
    { "--focus-loss", 2,
      [] (ParseContext& c)
      {
	      if (!ParseBoundedInt (c.argc, c.argv, *c.index, "--focus-loss", 0,
	                            c.options->focusLossX) ||
	          !ParseBoundedInt (c.argc, c.argv, *c.index, "--focus-loss", 0,
	                            c.options->focusLossY))
		      return false;
	      c.options->focusLoss = true;
	      return true;
      } },
    { "--dump-copy", 0,
      [] (ParseContext& c) { c.options->dumpCopy = true; return true; } },
    { "--copy-element", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--copy-element",
	                          c.options->copySelector);
      } },
    { "--dump-status", 0,
      [] (ParseContext& c) { c.options->dumpStatus = true; return true; } },
    { "--scroll-element", 2,
      [] (ParseContext& c)
      {
	      return ParseString (c.argc, c.argv, *c.index, "--scroll-element",
	                          c.options->scrollContainer) &&
	             ParseString (c.argc, c.argv, *c.index, "--scroll-element",
	                          c.options->scrollContent);
      } },
    { "--open-second", 1,
      [] (ParseContext& c) {
	      return ParseString (c.argc, c.argv, *c.index, "--open-second",
	                          c.options->openSecondPath);
      } },
    { "--resize", 1,
      [] (ParseContext& c)
      {
	      if (!ParseSize (c.argc, c.argv, *c.index, "--resize", c.options->resizeWidth,
	                      c.options->resizeHeight))
		      return false;
	      c.options->resize = true;
	      return true;
      } },
    { "--state-audit", 0,
      [] (ParseContext& c) { c.options->stateAudit = true; return true; } },
    { "--debugger", 0,
      [] (ParseContext& c) { c.options->debugger = true; return true; } },
};

// ANALYSE DE LA LIGNE DE COMMANDE, separee de main : main ne fait plus que
// enchainer analyse, resolution des assets, creation et boucle.
//
// Rend le CODE DE SORTIE du processus si le programme doit s'arreter la
// (`stop` a true) -- 0 pour --help, 2 pour un argument invalide -- et laisse
// `stop` a false quand l'analyse a reussi.
int ParseArguments (int argc, char** argv, peles::app::Options& options,
                    std::string& explicitAssetsDir, bool& stop)
{
	stop = true;

	ParseContext context { argc, argv, nullptr, &options, &explicitAssetsDir };

	for (int i = 1; i < argc; ++i)
	{
		// ⚠ --help RESTE HORS TABLE, et ce n'est pas un oubli : c'est la seule
		// option qui ARRETE le programme avec le code 0, donc la seule dont
		// l'action ne se resume pas a « rendre vrai ou faux ». Deux noms pour une
		// action, de surcroit.
		if (std::strcmp (argv[i], "--help") == 0 || std::strcmp (argv[i], "-h") == 0)
		{
			PrintUsage ();
			return 0;
		}

		const OptionSpec* matched = nullptr;
		for (const OptionSpec& spec : kOptions)
		{
			if (std::strcmp (argv[i], spec.name) == 0)
			{
				matched = &spec;
				break;
			}
		}

		if (matched != nullptr)
		{
			// LE CONTROLE D'ARITE EST FAIT ICI, AVANT L'ACTION : il rend le meme
			// diagnostic pour toutes les options, et il donne au champ `arity` un
			// LECTEUR -- un champ qui ne serait lu par personne se mettrait a
			// mentir sans que rien ne le dise.
			if (i + matched->arity >= argc)
			{
				std::fprintf (stderr, "%s attend %d valeur(s).\n", matched->name, matched->arity);
				return 2;
			}

			context.index = &i;
			if (!matched->apply (context))
				return 2;
			continue;
		}

		if (argv[i][0] == '-')
		{
			std::fprintf (stderr, "argument inconnu : %s\n", argv[i]);
			return 2;
		}

		if (argv[i][0] == '\0')
		{
			// ⚠ UN NOM DE FICHIER VIDE EST UNE ERREUR, PAS « AUCUN FICHIER ».
			// Accepte en silence, il laisse modelPath vide, donc une vue vide
			// indiscernable d'un lancement sans argument -- alors que l'operateur,
			// lui, a bien passe un argument.
			std::fprintf (stderr, "peles : nom de fichier vide.\n");
			return 2;
		}
		else if (options.modelPath.empty ())
			options.modelPath = argv[i];
		else
		{
			// UN SEUL FICHIER DANS CE LOT. Accepter le second en silence
			// laisserait croire que les deux sont charges.
			std::fprintf (stderr,
			              "peles : un seul fichier peut etre passe en argument (\"%s\" apres "
			              "\"%s\").\n",
			              argv[i], options.modelPath.c_str ());
			return 2;
		}
	}

	// UN SEUL VIDAGE PAR EXECUTION, ET LE SECOND EST REFUSE AVEC UN MESSAGE.
	//
	// ⚠ CHAQUE MODE DE CETTE LISTE EST UN MODE « VIDER ET SORTIR », et l'ordre
	// dans lequel ils sont examines en ferait courir UN SEUL : les autres ne
	// seraient jamais atteints, sans un mot. Le refus explicite est ce qu'un
	// second fichier en argument se voit deja opposer.
	const int dumpsRequested = (options.dumpTreeDir.empty () ? 0 : 1) +
	                           (options.dumpFilesDir.empty () ? 0 : 1) +
	                           (options.dumpInfo ? 1 : 0) + (options.dumpLayout ? 1 : 0) +
	                           (options.dumpPick ? 1 : 0) + (options.inputScript ? 1 : 0) +
	                           (options.dumpScroll ? 1 : 0) + (options.dumpCopy ? 1 : 0) +
	                           (options.focusLoss ? 1 : 0) +
	                           (options.altPivot ? 1 : 0) +
	                           (options.click ? 1 : 0) + (options.drag ? 1 : 0) +
	                           (options.dragSplitter ? 1 : 0);
	if (dumpsRequested > 1)
	{
		std::fprintf (stderr,
		              "peles : --dump-tree, --dump-files, --dump-info, --dump-layout, "
		              "--dump-pick, --input-script, --dump-scroll, --dump-copy, --focus-loss, "
		              "--alt-pivot, "
		              "--drag, --drag-splitter, --click "
		              "et --double-click ne peuvent pas etre demandes ensemble : chacun sort "
		              "apres son vidage.\n");
		return 2;
	}

	// ⚠ --dump-pick OUVRE LA FENETRE A LA TAILLE DEMANDEE, ET C'EST CE QUI LE
	// FAIT PASSER PAR LE CHEMIN DU PRODUIT. Il ne pose plus aucune dimension
	// lui-meme : il rend une image et relit celles que la phase 5 a posees. Sans
	// cette ligne, le cadre mesure serait celui de la fenetre par defaut et
	// l'argument <l>x<h> serait muet.
	if (options.dumpPick)
	{
		options.windowWidth  = options.pickWidth;
		options.windowHeight = options.pickHeight;
	}

	// --drag-bump N'A DE SENS QU'AVEC --drag. Accepte seul, il ne ferait rien et
	// l'operateur croirait avoir mesure un geste a deux boutons.
	if (options.dragBump != peles::app::GestureOwner::None && !options.drag)
	{
		std::fprintf (stderr, "peles : --drag-bump exige --drag.\n");
		return 2;
	}

	// LES DEUX ELEMENTS DU VIDAGE DE DEFILEMENT ONT UN DEFAUT, et ce sont ceux
	// que le critere nomme : l'arbre des materiaux, dans la zone de droite.
	if (options.dumpScroll && options.scrollContainer.empty ())
	{
		options.scrollContainer = "panel-right";
		options.scrollContent   = "material-tree";
	}

	// LA SEULE LIGNE COPIABLE DU DOCUMENT est celle que `InfoRow::copyable`
	// marque -- la ligne « Path », qui remplace l'infobulle de chemin de l'hote
	// historique. Le selecteur reste une option pour que le detecteur puisse
	// etre pointe ailleurs sans recompiler.
	if (options.dumpCopy && options.copySelector.empty ())
		options.copySelector = ".pcopy";

	// --copy-element N'A DE SENS QU'AVEC --dump-copy : accepte seul, il ne ferait
	// rien et l'operateur croirait avoir change la cible d'une mesure.
	if (!options.copySelector.empty () && !options.dumpCopy)
	{
		std::fprintf (stderr, "peles : --copy-element exige --dump-copy.\n");
		return 2;
	}

	// Le releve d'etat n'a de sens que sur un nombre fini d'images ; sans
	// --frames, il rendrait son verdict a la fermeture de la fenetre par un
	// operateur.
	if (options.stateAudit && options.frames == 0)
		options.frames = 3;

	// Meme raison pour le releve de statut : il est rendu APRES la boucle.
	if (options.dumpStatus && options.frames == 0)
		options.frames = 3;

	// Meme raison pour la lecture de pixel : sans borne, elle rendrait sa valeur
	// a la fermeture de la fenetre par un operateur.
	if (options.readPixel && options.frames == 0)
		options.frames = 1;

	// DEUX IMAGES PAR SELECTION : celle qui charge, puis celle qui dessine -- et
	// c'est a la SECONDE que le releve est pris, un maillage n'etant enregistre
	// aupres du moteur qu'a son premier dessin. Sans ces images, l'instrument
	// rendrait « non concluant » sur une ligne de commande pourtant correcte.
	if (options.reselect > 0 && options.frames == 0)
		options.frames = 2 * options.reselect + 1;

	// TROIS IMAGES POUR UNE SECONDE OUVERTURE, ET AUCUNE N'EST DE TROP : la
	// premiere pose le statut, la deuxieme EST l'image de transition, la
	// troisieme suit le chargement et porte le releve.
	if (!options.openSecondPath.empty () && options.frames == 0)
		options.frames = 3;

	// TROIS IMAGES POUR UN REDIMENSIONNEMENT : la premiere precede la demande, la
	// deuxieme est relevee, la troisieme laisse passer un rappel tardif sans
	// changer le verdict.
	if (options.resize && options.frames == 0)
		options.frames = 3;

	stop = false;
	return 0;
}

int Main (int argc, char** argv)
{
	peles::app::Options options;
	std::string         explicitAssetsDir;

	bool      stop     = false;
	const int exitCode = ParseArguments (argc, argv, options, explicitAssetsDir, stop);
	if (stop)
		return exitCode;

	// LES VIDAGES DE L'ARBRE ET DE LA LISTE SORTENT ICI, AVANT TOUT LE RESTE.
	//
	// ⚠ AVANT MEME LA RESOLUTION DES ASSETS, et ce n'est pas un raccourci : ils
	// n'appellent que peles_core, donc ils ne lisent aucun document RML et
	// n'ouvrent aucun contexte GL. Les faire dependre des assets les rendrait
	// inexecutables la ou ils sont precisement utiles -- une machine sans
	// interface graphique.
	if (!options.dumpTreeDir.empty ())
		return peles::app::DumpTree (options.dumpTreeDir);
	if (!options.dumpFilesDir.empty ())
		return peles::app::DumpFiles (options.dumpFilesDir);
	// ⚠ IL PREND SON FICHIER DANS L'ARGUMENT POSITIONNEL, ET IL ACCEPTE QU'IL
	// SOIT ABSENT : « aucun fichier charge » est l'un des deux cas que le
	// panneau doit savoir rendre, et le seul que quatorze lignes vides
	// decrivent.
	if (options.dumpInfo)
		return peles::app::DumpInfo (options.modelPath);

	// LA RESOLUTION DES ASSETS PRECEDE L'OUVERTURE DE LA FENETRE, et c'est le
	// critere d'acceptation lui-meme : une resolution faite apres produirait une
	// fenetre vide au lieu d'un diagnostic.
	const peles::app::AssetsResolution assets = peles::app::ResolveAssetsDir (explicitAssetsDir);
	if (!assets.error.empty ())
	{
		std::fprintf (stderr, "peles : %s\n", assets.error.c_str ());
		return 3;
	}
	std::printf ("assets : %s (%s)\n", assets.directory.c_str (), assets.origin.c_str ());
	options.assetsDir = assets.directory;

	peles::app::CreateError                  error;
	std::unique_ptr<peles::app::Application> application =
	    peles::app::Application::Create (options, error);
	if (!application)
	{
		std::fprintf (stderr, "peles : %s\n", error.message.c_str ());
		return error.exitCode;
	}

	return application->Run ();
}

} // namespace

int main (int argc, char** argv)
{
	// ⚠ RATTRAPAGE DE DERNIER RECOURS, ET IL A UN MOTIF PRECIS : un maillage
	// lourd fait jeter std::bad_alloc par les conteneurs de cgmesh, tres loin
	// d'ici. Sans ce bloc, le processus sort par std::terminate -- code de sortie
	// dependant du systeme, aucun message, et sous Windows une boite de dialogue
	// de plantage. Le code 1 le range avec les autres pannes d'execution.
	try
	{
		return Main (argc, argv);
	}
	catch (const std::exception& exception)
	{
		std::fprintf (stderr, "peles : exception non rattrapee : %s\n", exception.what ());
		return 1;
	}
	catch (...)
	{
		std::fprintf (stderr, "peles : exception non rattrapee, de type inconnu.\n");
		return 1;
	}
}
