#pragma once

#include <string>
#include <vector>

namespace peles
{
namespace app
{

// Charge les entrees GL par le glad de cgre et verifie celles dont le chemin
// fixe depend. Rend false et renseigne `description` si l'une manque.
//
// ⚠ TEST DE POINTEUR, JAMAIS D'APPEL. gladLoadGLUserPtr abandonne avant tout
// chargeur par version si glGetString est nul (src/cgre/glad/gl.c:2371-2373) :
// un chargement echoue laisse TOUS les pointeurs nuls, et le premier appel est
// alors un dereferencement nul, donc un plantage et non un diagnostic.
bool LoadGlEntries (std::string& description);

// Un etat nomme du releve : son nom, sa valeur imprimable, et -- quand une
// comparaison est demandee -- s'il coincide avec celui de l'autre releve.
struct GlStateEntry
{
	std::string name;
	std::string value;
	bool        identical = true;
};

// Le releve du contrat d'etat (peles_implementation.md §1.2).
//
// ⚠ LE NOMBRE D'ETATS N'EST ECRIT NULLE PART. Describe(), StateCount() et
// DifferingStates() derivent tous les trois de Entries() : un etat ajoute se
// compte tout seul, et aucun message ne peut annoncer un chiffre faux.
//
// Aucun type OpenGL dans cette declaration : l'en-tete doit rester incluable
// depuis une unite qui ne touche pas gl_wrapper.h.
struct GlStateSnapshot
{
	// ⚠ BORNE DELIBEREE, ET CE N'EST PAS UNE PRUDENCE. GL_MAX_TEXTURE_IMAGE_UNITS
	// compte les echantillonneurs du fragment shader (16 et plus), alors que
	// glActiveTexture en pipeline fixe est borne par GL_MAX_TEXTURE_UNITS.
	// Huit couvre les unites 0 et 1 du pipeline fixe ET les unites 2 a 6 ou le
	// chemin PBR de cgre lie ses cartes (src/cgre/vertex_buffer_manager.cpp,
	// kPbrUnitOffset).
	static constexpr int kMaxTextureUnits = 8;

	int currentProgram = 0;

	// ⚠ LES MATRICES NE SONT PAS DANS LA PILE D'ATTRIBUTS : GL_TRANSFORM_BIT
	// sauve le MODE de matrice courant, pas les matrices elles-memes. Elles
	// sont donc relevees, sans quoi le releve serait aveugle a la seule partie
	// du contrat que glPushAttrib ne tient pas.
	int   matrixMode           = 0;
	float projectionMatrix[16] = {};
	float textureMatrix[16]    = {};
	float modelviewMatrix[16]  = {};

	// ⚠ RELEVE AVEC LES LIAISONS, ET PAS SEULEMENT RESTAURE PAR L'INSTRUMENT :
	// GL_TEXTURE_MATRIX est celle de l'unite ACTIVE. Sans cet etat au releve,
	// une matrice de texture lue sur une autre unite que la precedente se
	// presenterait comme une divergence de matrice, sans sa cause.
	int activeTextureUnit = 0;

	int textureUnitCount                   = 0;
	int textureBinding2D[kMaxTextureUnits] = {};

	bool         blend            = false;
	bool         depthTest        = false;
	bool         cullFace         = false;
	bool         normalize        = false;
	bool         lighting         = false;
	bool         scissorTest      = false;
	bool         stencilTest      = false;
	unsigned int stencilWriteMask = 0;
	int          viewport[4]      = { 0, 0, 0, 0 };
	bool         framebufferSrgb  = false;
	int          framebufferBinding = 0;

	// ⚠ Exige un contexte courant. Le releve restaure l'unite de texture active
	// qu'il a du changer pour lire les liaisons des autres unites : il ne
	// modifie donc rien de ce qu'il mesure.
	static GlStateSnapshot Capture ();

	// Le releve nomme, dans l'ordre d'impression. `other` non nul : chaque
	// entree porte en plus le resultat de la comparaison. ⚠ Les matrices se
	// comparent BIT A BIT -- deux valeurs qui s'impriment pareil peuvent
	// differer.
	std::vector<GlStateEntry> Entries (const GlStateSnapshot* other = nullptr) const;

	std::string Describe () const;

	// Nombre d'etats nommes. ⚠ DERIVE du releve, jamais recopie : il depend du
	// nombre d'unites de texture du contexte.
	int StateCount () const;

	// Noms des etats qui different. Vide si les deux releves sont identiques.
	// ⚠ Un ecart doit NOMMER l'etat fautif : un booleen « ils different » ne
	// permettrait pas de conclure sur la cause.
	std::vector<std::string> DifferingStates (const GlStateSnapshot& other) const;
};

// Les deux releves encadrant UNE phase sous cloture.
//
// ⚠ `measured` DISTINGUE « conforme » DE « pas mesure ». Une phase sautee --
// rien a dessiner -- ne doit pas compter comme une phase dont le contrat a ete
// verifie : deux releves vierges sont identiques, et c'est le seul verdict
// franchement faux que cet instrument puisse rendre.
struct GlStatePhaseAudit
{
	const char*     phase    = "";
	GlStateSnapshot before;
	GlStateSnapshot after;
	bool            measured = false;
};

// Clôture RAII autour d'un moteur de rendu tiers.
//
// Ni cgre ni RmlUi ne sont reputes propres : l'hote empile l'etat avant et le
// depile apres, quel qu'en soit l'auteur.
//
// ⚠ glPushAttrib NE COUVRE NI LE PROGRAMME COURANT, NI LES VAO, NI LES LIAISONS
// DE TAMPON. glUseProgram(0) est donc pose explicitement AUX DEUX BORDS : un
// appelant qui laisserait un programme lie verrait le pipeline fixe du voisin
// dessiner a travers son fragment shader, SANS ERREUR GL.
//
// ⚠ GL_FRAMEBUFFER_SRGB n'est pas de son ressort. Il est un INVARIANT GLOBAL,
// desactive a l'initialisation et jamais retouche : src/cgre/surface_program.h:
// 72-75 encode la sortie par pow(1/2,2) DANS LE FRAGMENT precisement parce que
// GL_FRAMEBUFFER_SRGB re-encoderait aussi les surcouches dessinees en
// fixe-fonction dans le meme tampon.
class GlStateFence
{
public:
	// `audit` non nul : la cloture releve l'etat elle-meme, aux deux bords.
	//
	// ⚠ C'EST LA CLOTURE QUI RELEVE, ET NON L'APPELANT, POUR UN SEUL ETAT :
	// GL_CURRENT_PROGRAM. La cloture pose glUseProgram(0) aux deux bords, donc
	// deux releves pris de part et d'autre par l'appelant vaudraient 0 DES DEUX
	// COTES quoi qu'ait fait le moteur encadre -- l'etat figurerait au releve
	// sans pouvoir jamais diverger. Le releve de sortie lit donc le programme
	// AVANT le glUseProgram(0) du destructeur ; tout le reste est lu APRES le
	// depilement, puisque c'est le depilement que le contrat mesure.
	explicit GlStateFence (GlStatePhaseAudit* audit = nullptr);
	~GlStateFence ();

	GlStateFence (const GlStateFence&)            = delete;
	GlStateFence& operator= (const GlStateFence&) = delete;

private:
	GlStatePhaseAudit* m_audit;
};

// Pose les invariants globaux de l'image, dans cet ordre : GL_FRAMEBUFFER_SRGB
// desactive, aucun FBO lie, aucun VAO lie, aucun programme lie. A appeler une
// fois, le contexte courant.
void ApplyGlobalInvariants ();

} // namespace app
} // namespace peles
