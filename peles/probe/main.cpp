//
//  peles_probe -- sonde de contexte OpenGL.
//
// CE QU'ELLE MESURE. cgre est lie au PROFIL DE COMPATIBILITE d'OpenGL : son
// GLSL est en `#version 120` et lit gl_ModelViewProjectionMatrix, gl_NormalMatrix,
// gl_LightSource[] et gl_FrontMaterial, et son chemin VBO est alimente par
// glVertexPointer / glEnableClientState (cf. src/cgre/surface_program.h). Une
// application batie sur cgre ne peut donc vivre que dans un contexte de
// compatibilite. Cette sonde etablit, sur la machine cible et avant qu'une ligne
// d'interface ne soit ecrite, que glfw en delivre un et que cgre y dessine.
//
// ⚠ DEUX VALEURS ACCEPTABLES POUR GLFW_OPENGL_PROFILE, UNE SEULE INTERDITE.
// GLFW_OPENGL_ANY_PROFILE laisse le pilote choisir -- c'est ce que P1 mesure ;
// GLFW_OPENGL_COMPAT_PROFILE l'exige -- c'est ce que P3 mesure, et le repli
// documente. GLFW_OPENGL_CORE_PROFILE est INTERDIT : un contexte core n'expose
// ni le pipeline fixe ni les variables integrees du GLSL 1.20, et cgre y rend
// un ecran noir SANS AUCUN MESSAGE. La regle vaut pour tout hote de ce depot
// qui lie cgre.
//
// SORTIE. Une ligne par assertion sur stdout, PASS ou FAIL suivi de la valeur
// mesuree, puis un code de sortie. Aucun jugement a l'oeil n'est demande.
//
//   0  toutes les assertions du mode courant passent
//   1  au moins une assertion a echoue
//   2  glfwInit a echoue : rien n'a pu etre mesure
//   3  argument de ligne de commande inconnu
//
// Le mode par defaut porte sur P1 a P7, qui n'exigent aucun operateur. P8 --
// les evenements souris hors fenetre, bouton enfonce -- demande un geste humain
// et vit derriere `--probe-drag`.
//

// ORDRE D'INCLUSION CONTRAINT, et ce n'est pas une preference de style.
//
// gl_wrapper.h impose <windows.h> puis glad/wgl.h puis glad/gl.h. Or glad/gl.h
// ne declare pas seulement des pointeurs : il REDEFINIT CHAQUE ENTREE GL PAR
// MACRO (`#define glPopAttrib glad_glPopAttrib`). Un second en-tete GL, quel
// qu'il soit, entrerait en collision avec ces macros. D'ou la regle : dans une
// unite qui touche cgre, gl_wrapper.h est le SEUL en-tete GL, et il vient en
// premier.
#include "gl_wrapper.h"

// GLFW_INCLUDE_NONE interdit a glfw3.h de tirer son propre en-tete GL, ce qu'il
// fait par defaut. C'est l'application de la regle ci-dessus a glfw.
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "cgre.h"
#include "mesh.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace
{

// ---------------------------------------------------------------------------
//  Journal des assertions
// ---------------------------------------------------------------------------

int g_failures = 0;
int g_total    = 0;

void Assert (const char* id, const char* claim, bool pass, const std::string& measured)
{
	++g_total;
	if (!pass)
		++g_failures;
	std::printf ("%-3s %-4s  %s\n            %s\n", id, pass ? "PASS" : "FAIL", claim,
	             measured.c_str ());
	std::fflush (stdout);
}

// ASSERTION NON MESUREE, et elle ne compte ni pour ni contre. Une assertion qui
// n'a pas recu son entree n'est pas infirmee : la confondre avec un FAIL ferait
// passer une absence d'operateur pour un defaut de la plateforme.
//
// LA RECIPROQUE EST VRAIE AUSSI : une entree GL absente est un defaut de la
// plateforme, donc un FAIL. Le SKIP est reserve a ce qui manque HORS de la
// plateforme -- un operateur, une donnee d'entree, une surface de dessin.
void Skip (const char* id, const char* claim, const std::string& why)
{
	std::printf ("%-3s %-4s  %s\n            %s\n", id, "SKIP", claim, why.c_str ());
	std::fflush (stdout);
}

// FORMATEUR DE DIAGNOSTIC, et la seule fonction variadique de ce fichier.
//
// Une fonction en `...` ne fait verifier aucun de ses arguments variables : le
// compilateur cesse de controler la correspondance entre la chaine de format et
// ce qui suit. L'attribut `format` la lui rend, ce qui retire l'angle mort au
// lieu de l'accepter. MSVC ne le connait pas : sur cette chaine, la couverture
// vient de la construction GCC.
#if defined(__GNUC__)
#define CG_PRINTF_LIKE(fmt, first) __attribute__ ((format (printf, fmt, first)))
#else
#define CG_PRINTF_LIKE(fmt, first)
#endif

std::string Format (const char* format, ...) CG_PRINTF_LIKE (1, 2);

std::string Format (const char* format, ...)
{
	char    buffer[1024];
	va_list args;
	va_start (args, format);
	// vsnprintf termine toujours la chaine et rend la longueur VOULUE : la
	// troncature est silencieuse et sans debordement, ce qui est le
	// comportement recherche pour un message de diagnostic.
	std::vsnprintf (buffer, sizeof (buffer), format, args);
	va_end (args);
	return std::string (buffer);
}

// Les enonces sont nommes une fois : les deux branches d'une meme assertion --
// mesure et abandon -- doivent porter le MEME texte, sinon une sortie devient
// impossible a mettre en regard d'une autre.
const char* const kClaimP1 = "glfw ouvre une fenetre et un contexte sans hint de version";
const char* const kClaimP2 = "le contexte obtenu sans hint est de profil COMPATIBILITE";
const char* const kClaimP3 =
    "hints explicites 3.3 + GLFW_OPENGL_COMPAT_PROFILE : contexte de compatibilite";
const char* const kClaimP4 =
    "gladLoaderLoadGL >= 1.5 et les entrees du chemin fixe de cgre sont resolues";
const char* const kClaimP5    = "un glBegin / glVertex3f / glEnd passe sans erreur GL";
const char* const kClaimP6    = "cgre dessine : un cube engendre en memoire, lu au pixel";
const char* const kClaimP7    = "cgre::SurfaceProgram() construit le programme GLSL 1.20";
const char* const kClaimP8    = "glfw livre les positions du curseur HORS fenetre, bouton enfonce";

// ---------------------------------------------------------------------------
//  Erreurs glfw et journal de cgre
// ---------------------------------------------------------------------------

std::vector<std::string> g_cgreLog;

// Vrai si P6 a rendu son verdict par le REPLI sur le contraste, donc si la
// prediction au pixel est INFIRMEE. Le code de sortie ne distingue pas les deux
// cas -- les deux branches passent par Assert : ce drapeau le dit dans le
// resume, a l'intention d'un lecteur qui n'a que la sortie.
bool g_p6FellBackToContrast = false;

void OnGlfwError (int code, const char* description)
{
	std::fprintf (stderr, "glfw %d : %s\n", code, description ? description : "(sans texte)");
}

// Dernier code d'erreur glfw, file VIDEE : glfwGetError ne rend que la premiere
// erreur en attente et la consomme. Sans purge prealable, une sonde lirait
// l'erreur d'une etape precedente.
void ClearGlfwError ()
{
	glfwGetError (nullptr);
}

int LastGlfwError (std::string& description)
{
	const char* text = nullptr;
	const int   code = glfwGetError (&text);
	description      = text ? text : "(sans texte)";
	return code;
}

// LE PUITS DE JOURNALISATION DE cgre EST UNE RESSOURCE, donc il est tenu par un
// objet. Il capture g_cgreLog, de portee espace de noms de cette unite : un
// puits laisse en place apres la fin de main ecrirait dans un vecteur dont la
// duree de vie n'est pas ordonnee par rapport a celle du puits lui-meme, qui vit
// dans une AUTRE unite de traduction.
//
// Il est pose AVANT TOUT DESSIN : cgre y envoie la cause des echecs de
// compilation GLSL, et pose apres, il ne verrait rien de ce que P6 et P7
// declenchent.
class CgreLogSinkGuard
{
public:
	CgreLogSinkGuard ()
	{
		cgre::SetLogSink ([] (const std::string& message) { g_cgreLog.push_back (message); });
	}

	~CgreLogSinkGuard ()
	{
		cgre::SetLogSink (nullptr);
	}

	CgreLogSinkGuard (const CgreLogSinkGuard&)            = delete;
	CgreLogSinkGuard& operator= (const CgreLogSinkGuard&) = delete;
};

// ---------------------------------------------------------------------------
//  Entrees GL : on teste le pointeur, on n'appelle jamais a l'aveugle
// ---------------------------------------------------------------------------
//
// gladLoadGLUserPtr abandonne AVANT tout chargeur par version si glGetString est
// nul (src/cgre/glad/gl.c:2371-2373). Quand le chargement echoue, TOUS les
// pointeurs restent donc nuls -- pas seulement ceux des versions recentes.
// Appeler l'un d'eux est un dereferencement nul : un plantage, c'est-a-dire
// l'absence de tout verdict, la ou la sonde doit precisement en rendre un.
//
// Chaque mesure qui touche GL commence par tester un ECHANTILLON REPRESENTATIF
// de ses propres entrees. Le verdict d'une entree absente est un FAIL et non un
// SKIP : une entree GL non resolue EST un defaut de la plateforme.

bool HasFixedPipelineDrawEntries ()
{
	return glad_glViewport != nullptr && glad_glClear != nullptr &&
	       glad_glDisable != nullptr && glad_glLightfv != nullptr &&
	       glad_glReadPixels != nullptr;
}

std::string DescribeFixedPipelineDrawEntries ()
{
	return Format ("glViewport=%s glClear=%s glDisable=%s glLightfv=%s glReadPixels=%s",
	               glad_glViewport ? "ok" : "NUL", glad_glClear ? "ok" : "NUL",
	               glad_glDisable ? "ok" : "NUL", glad_glLightfv ? "ok" : "NUL",
	               glad_glReadPixels ? "ok" : "NUL");
}

// ---------------------------------------------------------------------------
//  Mesure du profil d'un contexte courant
// ---------------------------------------------------------------------------

struct ProfileMeasure
{
	std::string version     = "(GL_VERSION illisible)";
	int         major       = 0;
	int         minor       = 0;
	bool        compatible  = false;
	std::string how         = "aucun instrument applicable";
};

bool HasExtensionIndexed (const char* name)
{
	// glGetStringi appartient a GL 3.0. Sur un contexte anterieur le pointeur
	// est nul, et l'appeler serait un dereferencement nul, pas une erreur GL.
	if (glad_glGetStringi == nullptr)
		return false;

	GLint count = 0;
	glGetIntegerv (GL_NUM_EXTENSIONS, &count);
	for (GLint i = 0; i < count; ++i)
	{
		const GLubyte* extension = glGetStringi (GL_EXTENSIONS, (GLuint)i);
		if (extension && std::strcmp ((const char*)extension, name) == 0)
			return true;
	}
	return false;
}

ProfileMeasure MeasureProfile ()
{
	ProfileMeasure m;

	// Meme regle que partout ailleurs : glGetString est lui-meme un pointeur
	// charge par glad, et c'est celui dont l'absence fait abandonner le
	// chargeur entier.
	if (glad_glGetString == nullptr)
	{
		m.how = "glGetString n'est pas resolu : les entrees GL sont absentes";
		return m;
	}

	const GLubyte* version = glGetString (GL_VERSION);
	if (version == nullptr)
	{
		m.how = "glGetString(GL_VERSION) a rendu nullptr";
		return m;
	}
	m.version = (const char*)version;

	// La chaine commence par "<major>.<minor>" pour toute version de la
	// specification ; le reste est libre (numero de pilote, mention du profil).
	if (std::sscanf (m.version.c_str (), "%d.%d", &m.major, &m.minor) != 2)
	{
		m.major = 0;
		m.minor = 0;
		m.how   = "GL_VERSION non analysable";
		return m;
	}

	if (m.major > 3 || (m.major == 3 && m.minor >= 2))
	{
		// GL_CONTEXT_PROFILE_MASK n'existe qu'a partir de 3.2 : c'est la
		// version qui a introduit la notion de profil.
		GLint mask = 0;
		glGetIntegerv (GL_CONTEXT_PROFILE_MASK, &mask);
		m.compatible = (mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0;
		m.how        = Format ("GL_CONTEXT_PROFILE_MASK = 0x%X (%s)", (unsigned)mask,
		                       (mask & GL_CONTEXT_CORE_PROFILE_BIT)
		                           ? "CORE -- incompatible avec cgre"
		                           : "COMPATIBILITY");
	}
	else if (m.major == 3 && m.minor == 1)
	{
		// GL 3.1 a retire le pipeline fixe SAUF si GL_ARB_compatibility est
		// annonce. C'est donc l'extension, et elle seule, qui repond ici.
		m.compatible = HasExtensionIndexed ("GL_ARB_compatibility");
		m.how        = Format ("GL 3.1 : GL_ARB_compatibility %s",
		                       m.compatible ? "present" : "ABSENT");
	}
	else
	{
		// Avant 3.2, le profil core n'existe pas et le pipeline fixe est dans
		// le noyau : la question ne se pose pas.
		m.compatible = true;
		m.how        = "version < 3.1 : le profil core n'existe pas, le pipeline fixe est au noyau";
	}

	return m;
}

// ---------------------------------------------------------------------------
//  P6 -- le cube, engendre EN MEMOIRE
// ---------------------------------------------------------------------------
//
// Aucun fichier n'est lu : la sonde ne doit dependre ni d'un chemin de donnees,
// ni d'un importateur, ni d'un format. Douze triangles, arete unitaire, centre
// sur l'origine.
//
// La face +Z est celle que la camera regarde a plat (cf. BuildFrontalCamera).
void BuildUnitCube (Mesh& cube)
{
	const float h = 0.5f;

	cube.Init (8, 12);

	// Indices : bit 0 = X, bit 1 = Y, bit 2 = Z.
	for (unsigned int i = 0; i < 8; ++i)
		cube.SetVertex (i, (i & 1u) ? h : -h, (i & 2u) ? h : -h, (i & 4u) ? h : -h);

	// Enroulement DIRECT vu de l'exterieur : la normale de Newell de chaque face
	// pointe vers l'exterieur, donc celle de la face +Z vaut exactement (0,0,1).
	// C'est ce qui rend la couleur du pixel central predictible.
	const unsigned int faces[12][3] = {
		{ 4, 5, 7 }, { 4, 7, 6 },   // +Z
		{ 1, 0, 2 }, { 1, 2, 3 },   // -Z
		{ 1, 5, 4 }, { 1, 4, 0 },   // -Y
		{ 2, 6, 7 }, { 2, 7, 3 },   // +Y
		{ 0, 4, 6 }, { 0, 6, 2 },   // -X
		{ 5, 1, 3 }, { 5, 3, 7 }    // +X
	};
	for (unsigned int f = 0; f < 12; ++f)
		cube.SetFace (f, faces[f][0], faces[f][1], faces[f][2]);

	cube.ComputeNormals ();
}

// Camera FRONTALE et deterministe.
//
// La rotation identite regarde la scene depuis +Z, donc la face +Z du cube est
// vue a plat et sa normale en espace oeil vaut (0,0,1). C'est la seule pose
// pour laquelle la couleur du pixel central se calcule a la main.
//
// Distance 3 avec un champ vertical de 45 deg : la face occupe environ 48 % de
// la demi-hauteur d'image. Le pixel central tombe donc largement dedans et le
// pixel de coin largement dehors, quelle que soit l'imprecision du cadrage.
void BuildFrontalCamera (Ctrackball& trackball, int width, int height)
{
	static const float kIdentity[16] = { 1.f, 0.f, 0.f, 0.f,
	                                     0.f, 1.f, 0.f, 0.f,
	                                     0.f, 0.f, 1.f, 0.f,
	                                     0.f, 0.f, 0.f, 1.f };

	trackball.set_dimensions (width, height);
	trackball.set_rotation (kIdentity);
	trackball.set_pivot (0.f, 0.f, 0.f);
	trackball.set_zoom (3.f);
	trackball.set_camera ();
}

// Eclairage FIXE, pose explicitement.
//
// Une lampe DIRECTIONNELLE unique, orientee (0,0,1,0), donc colineaire a l'axe
// de vue : le produit N.L de la face avant vaut exactement 1. Elle est posee
// avec la modelview a l'identite, car glLightfv(GL_POSITION) transforme sa
// valeur par la matrice courante.
//
// Ambiante globale, ambiante et speculaire de lampe toutes NOIRES, seconde
// lampe eteinte et remise a zero : le programme de surface de cgre somme
// inconditionnellement les contributions des lampes 0 et 1, et lit l'ambiante
// du modele. Tout terme laisse a sa valeur par defaut entrerait dans le
// resultat sans figurer dans la prediction.
//
// La couleur du pixel central vaut alors, sur les DEUX chemins -- programme de
// surface et pipeline fixe -- la seule diffuse du materiau par defaut de cgre.
void SetupFixedLighting ()
{
	static const GLfloat kBlack[4]    = { 0.f, 0.f, 0.f, 1.f };
	static const GLfloat kWhite[4]    = { 1.f, 1.f, 1.f, 1.f };
	static const GLfloat kLightDir[4] = { 0.f, 0.f, 1.f, 0.f };

	glMatrixMode (GL_MODELVIEW);
	glLoadIdentity ();

	glLightModelfv (GL_LIGHT_MODEL_AMBIENT, kBlack);
	glLightModeli (GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

	glLightfv (GL_LIGHT0, GL_POSITION, kLightDir);
	glLightfv (GL_LIGHT0, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT0, GL_DIFFUSE, kWhite);
	glLightfv (GL_LIGHT0, GL_SPECULAR, kBlack);
	glEnable (GL_LIGHT0);

	glLightfv (GL_LIGHT1, GL_AMBIENT, kBlack);
	glLightfv (GL_LIGHT1, GL_DIFFUSE, kBlack);
	glLightfv (GL_LIGHT1, GL_SPECULAR, kBlack);
	glDisable (GL_LIGHT1);

	glEnable (GL_LIGHTING);
	glDisable (GL_COLOR_MATERIAL);
	glDisable (GL_TEXTURE_2D);
}

// ---------------------------------------------------------------------------
//  P8 -- evenements souris hors fenetre, bouton enfonce
// ---------------------------------------------------------------------------

struct DragProbe
{
	bool   buttonDown   = false;
	// Un appui a-t-il seulement eu lieu ? C'est ce qui separe « la plateforme
	// ne route pas les evenements hors cadre » de « l'operateur n'a rien fait ».
	bool   sawPress     = false;
	bool   released     = false;
	bool   sawOutside   = false;
	int    moveEvents   = 0;
	double outsideX     = 0.;
	double outsideY     = 0.;
};

void OnMouseButton (GLFWwindow* window, int button, int action, int /*mods*/)
{
	DragProbe* probe = static_cast<DragProbe*> (glfwGetWindowUserPointer (window));
	if (probe == nullptr || button != GLFW_MOUSE_BUTTON_LEFT)
		return;

	if (action == GLFW_PRESS)
	{
		probe->buttonDown = true;
		probe->sawPress   = true;
	}
	else if (action == GLFW_RELEASE && probe->buttonDown)
	{
		probe->buttonDown = false;
		probe->released   = true;
	}
}

void OnCursorPos (GLFWwindow* window, double x, double y)
{
	DragProbe* probe = static_cast<DragProbe*> (glfwGetWindowUserPointer (window));
	if (probe == nullptr || !probe->buttonDown)
		return;

	++probe->moveEvents;

	int width  = 0;
	int height = 0;
	glfwGetWindowSize (window, &width, &height);

	// Le curseur est HORS CADRE des qu'il sort du rectangle client. glfw ne
	// livre ces positions que si le systeme continue de router les evenements
	// vers la fenetre pendant l'appui -- c'est exactement la propriete mesuree.
	if (x < 0. || y < 0. || x >= (double)width || y >= (double)height)
	{
		probe->sawOutside = true;
		probe->outsideX   = x;
		probe->outsideY   = y;
	}
}

// ---------------------------------------------------------------------------
//  P1 -- une fenetre et un contexte, SANS AUCUN HINT DE VERSION
// ---------------------------------------------------------------------------

GLFWwindow* MeasureP1 ()
{
	glfwDefaultWindowHints ();
	// Aucun GLFW_CONTEXT_VERSION_*, aucun GLFW_OPENGL_PROFILE : c'est l'objet
	// meme de l'assertion. Le pilote rend son contexte par defaut, et P2 dit
	// lequel.
	//
	// Les trois hints ci-dessous ne touchent ni la version ni le profil :
	//  - le tampon de profondeur, sans lequel P6 echouerait pour une raison
	//    etrangere au profil (faces arriere peintes par-dessus la face avant) ;
	//  - zero echantillon : l'anticrenelage rendrait le pixel lu dependant de la
	//    position sous-pixel des aretes, donc non deterministe ;
	//  - double tampon, pour lire le tampon ARRIERE avant tout echange.
	glfwWindowHint (GLFW_DEPTH_BITS, 24);
	glfwWindowHint (GLFW_SAMPLES, 0);
	glfwWindowHint (GLFW_DOUBLEBUFFER, GLFW_TRUE);

	ClearGlfwError ();
	GLFWwindow* window = glfwCreateWindow (512, 512, "peles_probe", nullptr, nullptr);

	std::string description;
	const int   code = LastGlfwError (description);
	Assert ("P1", kClaimP1, window != nullptr && code == GLFW_NO_ERROR,
	        window != nullptr
	            ? Format ("fenetre 512x512, glfwGetError = %d", code)
	            : Format ("glfwCreateWindow = nullptr, glfwGetError = %d : %s", code,
	                      description.c_str ()));

	return window;
}

// ---------------------------------------------------------------------------
//  P4 -- le chargeur, et les quatre entrees du chemin fixe de cgre
// ---------------------------------------------------------------------------
//
// Mesure AVANT P2 : les deux instruments de P2 -- glGetIntegerv et glGetStringi
// -- sont eux-memes des pointeurs charges ici.
//
// TEST DE POINTEUR, JAMAIS D'APPEL. Appeler une entree absente est un
// dereferencement nul, donc un plantage et non un diagnostic.
void MeasureP4 ()
{
	const int  gladVersion = gladLoaderLoadGL ();
	const int  gladMajor   = GLAD_VERSION_MAJOR (gladVersion);
	const int  gladMinor   = GLAD_VERSION_MINOR (gladVersion);
	const bool hasLevel    = gladVersion != 0 &&
	                         (gladMajor > 1 || (gladMajor == 1 && gladMinor >= 5));
	const bool hasEntries = glad_glBegin != nullptr && glad_glPushAttrib != nullptr &&
	                        glad_glVertexPointer != nullptr &&
	                        glad_glEnableClientState != nullptr;
	Assert ("P4", kClaimP4, hasLevel && hasEntries,
	        Format ("glad %d.%d ; glBegin=%s glPushAttrib=%s glVertexPointer=%s "
	                "glEnableClientState=%s",
	                gladMajor, gladMinor,
	                glad_glBegin ? "ok" : "NUL",
	                glad_glPushAttrib ? "ok" : "NUL",
	                glad_glVertexPointer ? "ok" : "NUL",
	                glad_glEnableClientState ? "ok" : "NUL"));
}

// ---------------------------------------------------------------------------
//  P2 -- ce contexte est-il de COMPATIBILITE ?
// ---------------------------------------------------------------------------

ProfileMeasure MeasureP2 ()
{
	const ProfileMeasure profile = MeasureProfile ();
	Assert ("P2", kClaimP2, profile.compatible,
	        Format ("GL_VERSION = \"%s\" ; %s", profile.version.c_str (),
	                profile.how.c_str ()));
	return profile;
}

// ---------------------------------------------------------------------------
//  P5 -- le mode immediat passe
// ---------------------------------------------------------------------------

// Purge BORNEE de la file d'erreurs GL, et la borne n'est pas une precaution de
// style. Sous KHR_robustness, glGetError rend GL_CONTEXT_LOST de facon REPETEE
// tant que le contexte n'est pas restaure : une boucle « jusqu'a GL_NO_ERROR »
// n'en sortirait jamais, et la sonde se figerait sans rendre de verdict. Rend le
// nombre d'erreurs purgees.
const int kMaxDrainedGlErrors = 64;

int DrainGlErrors ()
{
	int drained = 0;
	while (drained < kMaxDrainedGlErrors && glGetError () != GL_NO_ERROR)
		++drained;
	return drained;
}

void MeasureP5 ()
{
	if (glad_glBegin == nullptr || glad_glEnd == nullptr || glad_glVertex3f == nullptr ||
	    glad_glGetError == nullptr)
	{
		Assert ("P5", kClaimP5, false,
		        "entrees du mode immediat non resolues : l'appel n'a pas ete tente");
		return;
	}

	// La file d'erreurs GL est VIDEE d'abord : glGetError ne rend qu'une
	// erreur a la fois, et une erreur heritee serait imputee a glBegin.
	if (DrainGlErrors () >= kMaxDrainedGlErrors)
	{
		Assert ("P5", kClaimP5, false,
		        Format ("la file d'erreurs GL ne se vide pas apres %d lectures : contexte "
		                "perdu ou pilote en faute, l'appel n'a pas ete tente",
		                kMaxDrainedGlErrors));
		return;
	}

	glBegin (GL_TRIANGLES);
	glVertex3f (0.f, 0.f, 0.f);
	glVertex3f (1.f, 0.f, 0.f);
	glVertex3f (0.f, 1.f, 0.f);
	glEnd ();

	const GLenum error = glGetError ();
	Assert ("P5", kClaimP5, error == GL_NO_ERROR,
	        Format ("glGetError apres glEnd = 0x%X (%s)", (unsigned)error,
	                error == GL_NO_ERROR
	                    ? "GL_NO_ERROR"
	                    : (error == GL_INVALID_OPERATION ? "GL_INVALID_OPERATION"
	                                                     : "autre")));
}

// ---------------------------------------------------------------------------
//  P6 -- cgre dessine
// ---------------------------------------------------------------------------
//
// Fonction a part, et non un bloc dans main : la mesure a deux PREALABLES --
// des entrees GL resolues et une surface de dessin exploitable -- dont l'absence
// doit interrompre l'etape sans interrompre la sonde. Un `return` l'exprime ; un
// saut au milieu d'un bloc ne le pourrait pas, le langage interdisant d'enjamber
// une initialisation.
void MeasureP6 (GLFWwindow* window, int glVersionMajor)
{
	// PREMIER PREALABLE : les entrees GL. Absentes, c'est un FAIL -- un defaut
	// de la plateforme, pas une mesure manquante.
	if (!HasFixedPipelineDrawEntries ())
	{
		Assert ("P6", kClaimP6, false,
		        "entrees GL non resolues : la mesure n'a pas ete tentee (" +
		            DescribeFixedPipelineDrawEntries () + ")");
		return;
	}

	int framebufferWidth  = 0;
	int framebufferHeight = 0;
	glfwPollEvents ();
	glfwGetFramebufferSize (window, &framebufferWidth, &framebufferHeight);

	// SECOND PREALABLE : UNE SURFACE DE DESSIN EXPLOITABLE, et ce n'est pas une
	// assertion. Une fenetre reduite, une session distante deconnectee ou un
	// compositeur qui rend une taille nulle sont ETRANGERS au profil GL, qui est
	// l'objet de la sonde. Les glReadPixels sortiraient du tampon et rendraient
	// du noir, ce qui se lirait comme « cgre ne dessine pas » -- et §4 de
	// peles_implementation.md fait de « P6 echoue, P7 passe » un signal d'arret
	// de lot. D'ou un SKIP : la mesure n'a pas eu lieu, elle n'est pas infirmee.
	if (framebufferWidth < 8 || framebufferHeight < 8)
	{
		Skip ("P6", kClaimP6,
		      Format ("tampon de dessin inexploitable : %dx%d -- fenetre reduite ou "
		              "surface absente, la mesure n'a pas eu lieu",
		              framebufferWidth, framebufferHeight));
		return;
	}

	Mesh cube;
	BuildUnitCube (cube);

	glViewport (0, 0, framebufferWidth, framebufferHeight);
	glDisable (GL_CULL_FACE);
	glEnable (GL_DEPTH_TEST);
	glDisable (GL_MULTISAMPLE);
	// GL_DITHER EST ACTIF PAR DEFAUT, et la specification autorise
	// l'implementation a modifier le bit de poids faible de la couleur ecrite
	// EN FONCTION DE LA POSITION DU PIXEL. Un oracle qui porte sur UN pixel a
	// +/-2/255 pres ne peut pas laisser allume un etat qui joue sur ce bit.
	glDisable (GL_DITHER);
	// L'encodage sRGB du tampon de sortie re-encoderait la couleur ecrite :
	// la prediction porte sur des valeurs LINEAIRES. L'enumerant appartient
	// a GL 3.0, d'ou la garde.
	if (glVersionMajor >= 3)
		glDisable (GL_FRAMEBUFFER_SRGB);

	SetupFixedLighting ();

	Ctrackball trackball;
	BuildFrontalCamera (trackball, framebufferWidth, framebufferHeight);

	glClearColor (0.f, 0.f, 0.f, 1.f);
	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	rendering_properties_s properties;
	rendering_properties_init (properties);
	// OMBRAGE PLAT : les coins d'un cube portent sinon une normale moyennee
	// sur trois faces, et la face avant cesserait d'avoir la normale (0,0,1)
	// sur laquelle repose la prediction.
	properties.smooth = 0;

	MeshRenderer* renderer = MeshRenderer::getInstance ();
	const int     meshId   = renderer->GetMeshId (&cube, CG_RENDERING_VBO);
	renderer->SetProperties (meshId, properties);
	renderer->Draw (meshId);

	glFinish ();

	// LE TAMPON ARRIERE, AVANT TOUT ECHANGE. Le tampon avant est soumis au
	// test d'appartenance des pixels : une fenetre partiellement recouverte
	// y rendrait des valeurs indefinies.
	glReadBuffer (GL_BACK);
	glPixelStorei (GL_PACK_ALIGNMENT, 1);

	unsigned char center[3] = { 0, 0, 0 };
	unsigned char corner[3] = { 0, 0, 0 };
	glReadPixels (framebufferWidth / 2, framebufferHeight / 2, 1, 1, GL_RGB,
	              GL_UNSIGNED_BYTE, center);
	glReadPixels (2, 2, 1, 1, GL_RGB, GL_UNSIGNED_BYTE, corner);

	glfwSwapBuffers (window);

	// Le maillage vit sur la pile : il doit sortir du registre du moteur
	// avant de mourir, sinon le singleton garde un Mesh* pendant.
	renderer->RemoveMesh (&cube);

	// PREDICTION. Le materiau par defaut de cgre a une diffuse (0.1, 0.5, 0.8),
	// une ambiante, une speculaire et une emission nulles
	// (MaterialRenderer::ActivateDefaultMaterial). Avec l'eclairage pose par
	// SetupFixedLighting et N.L = 1, la couleur vaut la diffuse seule, sur le
	// chemin du programme de surface comme sur le pipeline fixe.
	const unsigned char predicted[3] = { 26, 128, 204 };   // round(255 * {0.1, 0.5, 0.8})
	// ⚠ TOLERANCE ESTIMEE, NON CALIBREE. Elle couvre l'arrondi vers l'entier
	// de 25,5 et de 127,5 -- pas une dispersion mesuree sur un parc de
	// pilotes. Les valeurs brutes partent dans le rapport A CHAQUE
	// execution : c'est par elles que la tolerance definitive se fixera.
	const int kTolerance = 2;

	int worstDeviation = 0;
	for (int c = 0; c < 3; ++c)
	{
		const int deviation = std::abs ((int)center[c] - (int)predicted[c]);
		if (deviation > worstDeviation)
			worstDeviation = deviation;
	}

	int worstContrast = 0;
	for (int c = 0; c < 3; ++c)
	{
		const int contrast = std::abs ((int)center[c] - (int)corner[c]);
		if (contrast > worstContrast)
			worstContrast = contrast;
	}

	const std::string raw =
	    Format ("centre (%d,%d) = (%u,%u,%u) ; coin (2,2) = (%u,%u,%u) ; "
	            "prediction = (%u,%u,%u) ; ecart max = %d ; contraste max = %d",
	            framebufferWidth / 2, framebufferHeight / 2,
	            (unsigned)center[0], (unsigned)center[1], (unsigned)center[2],
	            (unsigned)corner[0], (unsigned)corner[1], (unsigned)corner[2],
	            (unsigned)predicted[0], (unsigned)predicted[1], (unsigned)predicted[2],
	            worstDeviation, worstContrast);

	if (worstDeviation <= kTolerance)
	{
		Assert ("P6", "cgre dessine : le pixel central est la couleur predite a +/-2/255 "
		              "(tolerance ESTIMEE)",
		        true, raw);
	}
	else
	{
		// REPLI, assertion plus faible mais toujours falsifiable : quelque
		// chose a ete dessine. Les valeurs brutes sont imprimees dans les
		// deux cas, pour que la tolerance definitive se calibre sur des
		// nombres reels.
		const int kMinimumContrast = 32;
		g_p6FellBackToContrast     = true;
		Assert ("P6", "cgre dessine : REPLI -- le pixel central differe du coin d'au moins "
		              "32/255 sur un canal",
		        worstContrast >= kMinimumContrast,
		        raw + Format (" ; la prediction a +/-%d est INFIRMEE, repli sur le contraste",
		                      kTolerance));
	}
}

// ---------------------------------------------------------------------------
//  P7 -- le programme de surface se construit
// ---------------------------------------------------------------------------

void MeasureP7 ()
{
	// cgre::SurfaceProgram() appelle glCreateProgram et glCreateShader
	// (src/cgre/gl_program.cpp:104, :119). Meme regle que P6 : sans entrees
	// resolues, l'appel serait un dereferencement nul et non un diagnostic.
	if (glad_glCreateProgram == nullptr || glad_glCreateShader == nullptr)
	{
		Assert ("P7", kClaimP7, false,
		        Format ("entrees GL non resolues : la mesure n'a pas ete tentee "
		                "(glCreateProgram=%s glCreateShader=%s)",
		                glad_glCreateProgram ? "ok" : "NUL",
		                glad_glCreateShader ? "ok" : "NUL"));
		return;
	}

	const cgre::GlProgram* program = cgre::SurfaceProgram ();
	std::string            detail  = program != nullptr
	                                     ? std::string ("cgre::SurfaceProgram() rend un pointeur non nul")
	                                     : std::string ("cgre::SurfaceProgram() rend nullptr : le GLSL "
	                                                    "#version 120 ne compile pas");
	if (!g_cgreLog.empty ())
	{
		detail += Format (" ; journal cgre (%d message(s)) :", (int)g_cgreLog.size ());
		for (const std::string& message : g_cgreLog)
			detail += "\n            | " + message;
	}
	else
		detail += " ; journal cgre vide";

	Assert ("P7", kClaimP7, program != nullptr, detail);
}

// ---------------------------------------------------------------------------
//  P3 -- memes mesures avec des hints EXPLICITES 3.3 + COMPAT
// ---------------------------------------------------------------------------

void MeasureP3 (GLFWwindow* primary)
{
	glfwDefaultWindowHints ();
	glfwWindowHint (GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint (GLFW_CONTEXT_VERSION_MINOR, 3);
	// GLFW_OPENGL_COMPAT_PROFILE, et c'est tout le propos : le meme code
	// avec GLFW_OPENGL_CORE_PROFILE donnerait un contexte ou cgre ne peut
	// rien dessiner, en silence.
	glfwWindowHint (GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
	glfwWindowHint (GLFW_DEPTH_BITS, 24);
	glfwWindowHint (GLFW_VISIBLE, GLFW_FALSE);

	ClearGlfwError ();
	GLFWwindow* second = glfwCreateWindow (64, 64, "peles_probe (P3)", nullptr, nullptr);
	std::string description;
	const int   code = LastGlfwError (description);

	if (second == nullptr)
	{
		Assert ("P3", kClaimP3, false,
		        Format ("glfwCreateWindow = nullptr, glfwGetError = %d : %s", code,
		                description.c_str ()));
		return;
	}

	glfwMakeContextCurrent (second);
	// Les pointeurs de glad appartiennent au contexte qui les a resolus : ils
	// sont recharges pour celui-ci, puis rendus au premier ci-dessous. LE
	// RETOUR EST CONTROLE : a zero, le chargeur a abandonne et TOUS les
	// pointeurs sont nuls -- MeasureProfile appellerait alors glGetString sur
	// un pointeur nul.
	if (gladLoaderLoadGL () == 0)
	{
		Assert ("P3", kClaimP3, false,
		        "gladLoaderLoadGL a rendu 0 sur le second contexte : aucune entree GL "
		        "resolue, le profil n'a pas ete mesure");
	}
	else
	{
		const ProfileMeasure explicitProfile = MeasureProfile ();
		Assert ("P3", kClaimP3, explicitProfile.compatible,
		        Format ("GL_VERSION = \"%s\" ; %s", explicitProfile.version.c_str (),
		                explicitProfile.how.c_str ()));
	}

	glfwDestroyWindow (second);
	glfwMakeContextCurrent (primary);

	// RESTAURATION des pointeurs pour le contexte principal. Ce n'est pas une
	// assertion : aucune mesure GL ne suit ce point, donc un echec ici ne
	// fausse aucun verdict. Il est signale parce qu'il rendrait dangereuse
	// toute mesure GL ajoutee apres lui dans cette sonde.
	//
	// ⚠ `--state-audit` N'EST PAS ICI, ET NE DOIT PAS Y VENIR : le mode est
	// porte par `peles`, pas par `peles_probe`, parce que la sonde ne lie pas
	// RmlUi -- elle ne peut donc pas auditer les deux phases sous cloture.
	if (gladLoaderLoadGL () == 0)
		std::fprintf (stderr,
		              "gladLoaderLoadGL a echoue au retour sur le contexte principal : "
		              "les entrees GL ne sont plus resolues.\n");
}

// ---------------------------------------------------------------------------
//  P8 -- evenements souris hors fenetre, bouton enfonce (--probe-drag)
// ---------------------------------------------------------------------------

void MeasureP8 (GLFWwindow* window)
{
	DragProbe probe;
	glfwSetWindowUserPointer (window, &probe);
	glfwSetMouseButtonCallback (window, &OnMouseButton);
	glfwSetCursorPosCallback (window, &OnCursorPos);

	// Consigne en ASCII pur : la console Windows n'est pas en UTF-8 par
	// defaut, et un verdict illisible ne vaut pas mieux qu'aucun verdict.
	std::printf ("\nP8 -- GESTE REQUIS. Dans la fenetre \"peles_probe\" :\n"
	             "  1. appuyez sur le bouton GAUCHE a l'interieur du cadre ;\n"
	             "  2. sans relacher, glissez le curseur HORS du cadre ;\n"
	             "  3. relachez.\n"
	             "Fermer la fenetre abandonne la mesure. Delai : 60 s.\n\n");
	std::fflush (stdout);

	const double deadline = glfwGetTime () + 60.;
	while (!probe.released && !glfwWindowShouldClose (window) && glfwGetTime () < deadline)
		glfwWaitEventsTimeout (0.1);

	glfwSetCursorPosCallback (window, nullptr);
	glfwSetMouseButtonCallback (window, nullptr);
	glfwSetWindowUserPointer (window, nullptr);

	if (!probe.sawPress)
		Skip ("P8", kClaimP8,
		      "aucun appui recu : le geste n'a pas eu lieu, l'instrument n'a pas mesure");
	else
		Assert ("P8", kClaimP8, probe.sawOutside,
		        probe.sawOutside
		            ? Format ("%d evenement(s) cursor_pos pendant l'appui ; dernier hors "
		                      "cadre = (%.1f, %.1f)",
		                      probe.moveEvents, probe.outsideX, probe.outsideY)
		            : Format ("%d evenement(s) cursor_pos pendant l'appui, AUCUN hors cadre",
		                      probe.moveEvents));
}

// ---------------------------------------------------------------------------
//  Ligne de commande et resume
// ---------------------------------------------------------------------------

enum class ArgOutcome
{
	RunDefault,
	RunDrag,
	HelpShown,
	Unknown
};

ArgOutcome ParseArguments (int argc, char** argv)
{
	ArgOutcome outcome = ArgOutcome::RunDefault;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp (argv[i], "--probe-drag") == 0)
			outcome = ArgOutcome::RunDrag;
		else if (std::strcmp (argv[i], "--help") == 0 || std::strcmp (argv[i], "-h") == 0)
		{
			std::printf ("peles_probe [--probe-drag]\n"
			             "  (sans argument)  P1..P7, automatiques, aucun operateur requis\n"
			             "  --probe-drag     ajoute P8, qui exige un geste humain\n");
			return ArgOutcome::HelpShown;
		}
		else
		{
			std::fprintf (stderr, "argument inconnu : %s\n", argv[i]);
			return ArgOutcome::Unknown;
		}
	}
	return outcome;
}

void ReportSummary (bool dragMode)
{
	std::printf ("\nRESULTAT : %d/%d assertion(s) passent.\n", g_total - g_failures, g_total);
	// Le code de sortie ne distingue pas la prediction verifiee du repli : les
	// deux branches de P6 passent par Assert. La mention est donc le seul signal
	// pour un lecteur -- ou une CI -- qui ne lit pas la ligne P6 elle-meme.
	if (g_p6FellBackToContrast)
		std::printf ("P6 : prediction au pixel INFIRMEE -- verdict rendu par le REPLI sur "
		             "le contraste.\n");
	if (!dragMode)
		std::printf ("P8 non mesure : relancer avec --probe-drag (un operateur est requis).\n");
}

} // namespace

int main (int argc, char** argv)
{
	const ArgOutcome outcome = ParseArguments (argc, argv);
	if (outcome == ArgOutcome::HelpShown)
		return 0;
	if (outcome == ArgOutcome::Unknown)
		return 3;
	const bool dragMode = (outcome == ArgOutcome::RunDrag);

	const CgreLogSinkGuard logSink;

	glfwSetErrorCallback (&OnGlfwError);
	if (!glfwInit ())
	{
		std::fprintf (stderr, "glfwInit a echoue : aucune assertion n'est mesurable.\n");
		return 2;
	}

	std::printf ("peles_probe -- sonde de contexte OpenGL pour peles\n");
	std::printf ("glfw %s\n\n", glfwGetVersionString ());

	GLFWwindow* window = MeasureP1 ();
	if (window == nullptr)
	{
		std::printf ("\nP1 a echoue : les assertions suivantes n'ont pas de contexte ou se tenir.\n");
		glfwTerminate ();
		return 1;
	}

	glfwMakeContextCurrent (window);

	MeasureP4 ();
	const ProfileMeasure defaultProfile = MeasureP2 ();
	MeasureP5 ();
	MeasureP6 (window, defaultProfile.major);
	MeasureP7 ();
	MeasureP3 (window);
	if (dragMode)
		MeasureP8 (window);

	glfwDestroyWindow (window);
	glfwTerminate ();

	ReportSummary (dragMode);

	return g_failures == 0 ? 0 : 1;
}
