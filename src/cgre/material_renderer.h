#pragma once

#include "gl_wrapper.h"

#include "../cgmesh/cgmesh.h"
// Decision D2 : MaterialRenderer consomme directement le MaterialPbr, sans
// passer par un troisieme modele de materiau. Les facteurs et les emplacements
// de cartes sont donc dans son interface, pas seulement dans son implementation.
#include "../cgmesh/material_pbr.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class MaterialRenderer
{
private:
	MaterialRenderer();
	~MaterialRenderer ();
public:
	static MaterialRenderer* getInstance (void) { return m_pInstance; };

	int AddMaterial (Material *pMaterial);

	// RETRAIT D'UN MATERIAU : libere ses objets de texture et rend son emplacement.
	//
	// Sans lui, le registre ne faisait que croitre : les entrees n'etaient jamais
	// retirees, donc (a) les textures fuyaient pour toute la duree du processus,
	// (b) le plafond de 256 finissait par etre atteint et tous les maillages
	// ouverts ensuite passaient en bleu par defaut sans un mot, et (c) les
	// Material* conserves devenaient pendants, si bien qu'un materiau neuf ne a
	// l'adresse recyclee d'un ancien se voyait rendre l'ANCIEN indice, donc
	// l'ancienne texture.
	//
	// Appele par MeshRenderer::RemoveMesh pour chaque materiau du maillage
	// retire. L'APPEL n'a pas besoin d'etre compte : un Material appartient a UN
	// maillage et a un seul -- Mesh::m_materials est un vector<unique_ptr>, il
	// n'y a pas de partage entre maillages.
	//
	// LES OBJETS DE TEXTURE, EUX, SONT COMPTES, et cette distinction est tout ce
	// qu'il faut retenir ici. Plusieurs materiaux nes d'une meme lecture glTF
	// partagent leurs pixels, donc leurs textures : retirer le premier n'en
	// detruit aucun, seul le dernier les rend. Cf. TextureKey, ReleaseTexture,
	// et le chiffrage a Entry::mapTex.
	void RemoveMaterial (Material *pMaterial);

	static void SetMaterial (MaterialColorExt::MaterialColorExtType eType);

	// LE MATERIAU NEUTRE, et il n'y en a qu'un dans tout le moteur.
	//
	// Il sert a DEUX endroits qu'il serait faux de laisser diverger : le mode
	// d'ombrage « Neutre », et le maillage qui ne porte AUCUN materiau. Ce
	// second cas ne liait rien du tout jusqu'ici -- l'etat GL courant, herite du
	// maillage precedent, s'appliquait -- donc l'apparence « par defaut » etait
	// un residu et non un choix. Elle en est un maintenant.
	static void ActivateNeutralMaterial (void);

	// LE MATERIAU PAR DEFAUT : celui d'un maillage qui ne porte AUCUN materiau.
	//
	// Il vivait en dur dans le reglage du contexte GL de l'hote, pose une seule
	// fois au demarrage. Cela tenait tant que rien ne le remplacait -- mais le
	// premier materiau active, texture ou non, l'ecrasait pour de bon, et la
	// couleur d'un maillage sans materiau dependait alors de ce qui avait ete
	// dessine avant lui.
	//
	// Il est ici pour n'exister qu'a UN endroit, et il est repose a chaque
	// maillage qui en a besoin plutot qu'une fois pour toutes.
	static void ActivateDefaultMaterial (void);
	void ActivateMaterial (unsigned int id);

	// CE QUE LE SHADER DOIT SAVOIR d'un materiau, et qu'il ne peut plus deduire
	// de l'etat GL.
	//
	// Sous programme lie, glEnable(GL_TEXTURE_2D) n'a plus d'effet et aucune
	// fonction GLSL ne dit si une unite de texture porte quelque chose : le
	// fragment doit se le faire dire par un uniforme. ActivateMaterial lie
	// toujours les objets de texture aux unites 0 et 1 -- c'est seulement la
	// DECISION d'echantillonner qui remonte ici.
	struct MaterialGlInfo
	{
		bool  hasTexture    = false;   // unite 0
		bool  hasReflection = false;   // unite 1
		float reflAmount    = 0.f;
	};
	MaterialGlInfo GetGlInfo (unsigned int id) const;

	// Ce qu'il faut pour echantillonner un materiau PBR : les objets de texture,
	// leur presence, et les facteurs qui les multiplient.
	//
	// `isPbr` faux -- materiau non PBR, emplacement libere, ou identifiant
	// inconnu -- veut dire « ce materiau n'a pas de chemin PBR », et l'appelant
	// doit alors emprunter la projection de Phong. C'est le SEUL test : un
	// appelant qui regarderait `mapTex` sans lui prendrait les zeros d'un
	// emplacement libre pour des textures par defaut.
	struct MaterialPbrInfo
	{
		bool   isPbr = false;
		GLuint mapTex[static_cast<std::size_t> (cgpbr::MapSlot::count)] = {};
		bool   hasMap[static_cast<std::size_t> (cgpbr::MapSlot::count)] = {};
		cgpbr::Factors factors {};

		// Jeu d'UV DEMANDE par chaque carte (cf. cgpbr::TextureRef::uvSet). Le
		// chemin PBR l'ignore -- dette connue, tout est echantillonne au jeu 0 ;
		// le repli de Phong enrichi, lui, refuse une carte qui ne vise pas le
		// jeu 0 plutot que de l'appliquer avec la mauvaise parametrisation.
		std::uint8_t uvSet[static_cast<std::size_t> (cgpbr::MapSlot::count)] = {};

		// Mode d'alpha du materiau source. Le chemin PBR ne l'exploite pas ; le
		// repli de Phong enrichi en tire la decoupe (`mask`).
		cgpbr::AlphaMode alphaMode = cgpbr::AlphaMode::opaque;
	};
	MaterialPbrInfo GetPbrInfo (unsigned int id) const;

	// COMPTE DES OBJETS DE TEXTURE VIVANTS, et ce qu'ils pesent.
	//
	// Rien ne comptait ces objets jusqu'ici : leur nombre se DEDUISAIT du
	// nombre de materiaux et de cartes, ce qui suppose precisement ce que la
	// mutualisation change. Le compte est desormais releve.
	//
	// `uses` est la somme des references, donc le nombre d'objets qu'il aurait
	// fallu sans partage : l'ecart entre les deux EST le gain.
	//
	// `bytes` est une VRAM THEORIQUE : quatre octets par texel, plus le tiers
	// des mipmaps quand elles ont ete generees. Le pilote pade et compresse a
	// sa guise -- ce chiffre borne la demande, il ne mesure pas l'occupation.
	struct TextureStats
	{
		std::size_t   objects = 0;
		std::size_t   uses    = 0;
		std::uint64_t bytes   = 0;
	};
	TextureStats GetTextureStats () const;

private:
	static MaterialRenderer *m_pInstance;

	// IDENTITE D'UN OBJET DE TEXTURE PARTAGEABLE.
	//
	// Deux usages ne partagent un objet que si les TROIS composantes coincident,
	// parce que les trois sont un etat de l'objet et non de l'unite :
	//
	//   - `image` : les pixels, designes par l'adresse. La cle ne possede rien,
	//     mais SharedTexture RETIENT l'image par shared_ptr, et c'est ce qui
	//     rend l'adresse non recyclable PAR CONSTRUCTION plutot que par
	//     convention. Sans cette prise, un RemoveMaterial manque -- cas que
	//     AddMaterial admet et journalise -- laisserait une cle vivante dont
	//     l'Img est detruite ; une Img reallouee a la meme adresse, meme format,
	//     meme habillage, obtiendrait l'objet de texture de l'ancien modele, et
	//     l'image serait fausse en silence. Avec la prise, la sanction d'un
	//     RemoveMaterial manque redevient ce qu'elle etait avant le partage :
	//     une fuite, pas une confusion.
	//
	//   - `internalFormat` : GL_SRGB8_ALPHA8 fait lineariser par le MATERIEL,
	//     avant le filtrage, ce qu'aucune correction a l'echantillonnage ne
	//     saurait reproduire. La couleur de base existe donc en deux exemplaires
	//     inconciliables -- sRGB pour le chemin PBR, composantes heritees pour le
	//     repli de Phong -- et le format est dans la cle pour cette raison. Le
	//     cout de leur confusion est chiffre a Entry::mapTex.
	//
	//   - `wrap` : GL_REPEAT pour une carte de surface, GL_CLAMP_TO_EDGE pour
	//     une sphere map, dont un repliement coudrait la silhouette.
	//
	//     AUCUN CHEMIN ACTUEL NE PRODUIT CETTE COLLISION, et le dire est plus
	//     honnete que de la presenter comme observee : LoadTextureImage alloue
	//     une Img neuve a chaque appel, donc un MTL declarant `map_Kd t.png` et
	//     `refl t.png` rend deux adresses distinctes. La composante est une
	//     garde PAR CONSTRUCTION, de cout nul, et elle deviendra necessaire le
	//     jour ou le `sampler` glTF sera lu -- une meme image y porte
	//     legitimement deux modes d'habillage.
	struct TextureKey
	{
		const Img* image          = nullptr;
		GLint      internalFormat = 0;
		GLint      wrap           = 0;

		bool operator== (const TextureKey& o) const
		{
			return image == o.image && internalFormat == o.internalFormat && wrap == o.wrap;
		}
		// Une cle sans image ne designe rien : c'est l'etat d'un emplacement
		// qui n'a jamais acquis de texture, et rien n'est a rendre pour elle.
		bool Empty () const { return image == nullptr; }
	};

	struct TextureKeyHash
	{
		std::size_t operator() (const TextureKey& k) const
		{
			const std::size_t a = std::hash<const void*> () (k.image);
			const std::size_t b = (std::size_t) (unsigned) k.internalFormat;
			const std::size_t c = (std::size_t) (unsigned) k.wrap;
			return a ^ (b * 0x9E3779B97F4A7C15ull) ^ (c << 1);
		}
	};

	// UN objet de texture partage et ce qui permet de le rendre.
	struct SharedTexture
	{
		GLuint       id    = 0;
		unsigned int refs  = 0;

		// L'IMAGE EST RETENUE pour que l'adresse qui sert de cle ne puisse pas
		// etre reattribuee tant qu'un objet la designe (cf. TextureKey::image).
		// Elle ne coute rien de plus : le materiau la detient deja par le meme
		// shared_ptr, et pour la meme duree.
		std::shared_ptr<const Img> image;

		// Pour GetTextureStats : la taille REELLEMENT televersee. Elle differe
		// de celle de l'image au-dela de GL_MAX_TEXTURE_SIZE, ou
		// UploadTextureImage televerse une COPIE REDUITE sans toucher a
		// l'original -- un albedo 8192 carre sur un pilote borne a 4096
		// televerse 85,3 Mio et l'image en annoncerait 341.
		unsigned int width = 0, height = 0;
		bool         mipmapped = false;
	};

	// Prend l'objet de texture de (image, format, habillage), en le creant au
	// premier usage, et compte une reference de plus. Rend 0 si rien n'a pu etre
	// televerse -- auquel cas aucune reference n'est comptee et `outKey` reste
	// vide.
	//
	// PREND UN shared_ptr PAR VALEUR : l'appel exprime le transfert d'une part
	// de propriete, pas un emprunt. Cf. SharedTexture::image.
	//
	// NEUTRE EN ETAT GL : l'unite active et la liaison de l'unite 0 sont relevees
	// puis restituees, y compris sur les chemins d'echec.
	GLuint AcquireTexture (std::shared_ptr<const Img> image, GLint internalFormat, GLint wrap,
	                       TextureKey& outKey);

	// Rend une reference. Le DERNIER usager detruit l'objet ; les autres ne
	// font que decrementer. Sans ce compte, retirer un maillage detruirait les
	// textures que ses voisins partagent avec lui.
	void ReleaseTexture (TextureKey& key);

	std::unordered_map<TextureKey, SharedTexture, TextureKeyHash> m_sharedTextures;

	// UN materiau enregistre. Remplace cinq tableaux paralleles de 256 entrees,
	// dont le plafond faisait basculer en bleu par defaut, sans message, tous les
	// maillages ouverts au-dela du 256e materiau de la session.
	struct Entry
	{
		Material *pMaterial = nullptr;      // nullptr : emplacement libre

		// Identite capturee a l'insertion, JAMAIS relue par le pointeur.
		// Sert de garde-fou : si RemoveMaterial venait a etre oublie sur un
		// chemin, un materiau neuf ne a une adresse recyclee serait detecte ici
		// au lieu de se voir rendre l'ancienne texture en silence.
		MaterialType type = MATERIAL_NONE;
		std::string  name;

		// PROJECTION Phong d'un MATERIAL_PBR, calculee une fois a l'insertion.
		//
		// Le pipeline fixe d'OpenGL ne connait que le modele de Phong : un
		// materiau PBR n'a aucune branche dans ActivateMaterial, et la cascade
		// y est SANS REPLI -- l'objet heriterait donc de l'etat GL du materiau
		// dessine juste avant. La projection lui rend un type que la cascade
		// sait traiter.
		//
		// Detenue ici plutot que recalculee a chaque activation : la projection
		// alloue, et ActivateMaterial est appelee a chaque rendu de plage.
		// nullptr pour tout materiau qui n'est pas PBR.
		std::unique_ptr<Material> projection;

		// LES OBJETS DE TEXTURE SONT PARTAGES, l'entree ne les possede pas : elle
		// en detient une reference, et la cle qui permet de la rendre. Deux
		// materiaux nes de la meme lecture glTF partagent leurs pixels, donc
		// leurs textures.
		GLuint textureId     = 0;
		TextureKey textureKey {};
		// Carte de reflexion, 0 si aucune. Objet GL distinct : un materiau peut
		// porter les deux cartes, liees a deux unites de texture.
		GLuint reflTextureId = 0;
		TextureKey reflKey {};
		// Taux de reflexion, dans [0,1] : proportion du reflet dans la couleur
		// finale. Ni le 3DS ni le MTL ne fournissent de montant pour la map, on
		// le derive donc du niveau speculaire du materiau (cf. ActivateMaterial).
		float  reflAmount    = 0.f;

		// ------------------------------------------------------------------
		//  Cartes PBR : televersees, PAS ENCORE ECHANTILLONNEES
		// ------------------------------------------------------------------
		// Un objet de texture par emplacement de cgpbr::MapSlot, 0 quand la
		// carte est absente. Aucune de ces textures n'est liee au moment du
		// dessin : le rendu passe toujours par `projection`, et c'est ce qui
		// rend cette etape verifiable -- un televersement qui changerait
		// l'image aurait un defaut.
		//
		// FORMAT INTERNE tire de TextureRef::colorSpace, jamais de l'indice
		// d'emplacement : GL_SRGB8_ALPHA8 pour une carte porteuse de couleur,
		// GL_RGBA8 pour une carte porteuse de donnees. Le decodage sRGB
		// materiel s'applique AVANT le filtrage, ce qu'une correction a
		// l'echantillonnage ne saurait faire.
		//
		// ⚠ CHIFFRAGE FAISANT AUTORITE pour ce module ; les autres commentaires
		// y renvoient plutot que de le repeter.
		//
		// LA COULEUR DE BASE EST TELEVERSEE DEUX FOIS -- ici, et comme
		// `textureId` par la projection Phong -- dans DEUX FORMATS INTERNES
		// INCONCILIABLES. Mesure par mutation : faire echantillonner la carte
		// sRGB au repli de Phong fait tomber sa luminance moyenne de 0,238 a
		// 0,103 sur Lantern.glb, le materiel linearisant des octets que le
		// repli ne re-encode jamais. Les fusionner n'est donc pas un nettoyage :
		// c'est choisir LEQUEL DES DEUX CHEMINS absorbe la conversion sRGB, et
		// aujourd'hui la reponse est le chemin PBR, parce que lui seul encode
		// sa sortie.
		//
		// CE QUE COUTE LE MODELE, releve par GetTextureStats sur Lantern.glb --
		// un materiau du fichier servi par trois primitives, quatre cartes de
		// 2048 carres avec leurs mipmaps a 21,3 Mio piece, plus l'albedo du
		// repli dans son format herite :
		//
		//     5 objets de texture pour 15 usages, 106,7 Mio de VRAM theorique.
		//
		// Sans partage, ce serait 15 objets et 320 Mio.
		std::array<GLuint, static_cast<std::size_t> (cgpbr::MapSlot::count)> mapTex {};

		// Cle de chaque mapTex : ce qu'il faut rendre a la table. Une cle vide
		// marque un emplacement qui n'a jamais acquis de texture.
		std::array<TextureKey, static_cast<std::size_t> (cgpbr::MapSlot::count)> mapKey {};

		// Bit i leve quand mapTex[i] porte un objet de texture COMPLET, c'est-a-dire
		// dont le niveau 0 a ete effectivement televerse. Un nom nul ou un
		// televersement echoue ne s'y trouvent pas : un echantillonneur teste une
		// presence, il ne devine pas un identifiant.
		std::uint8_t mapMask = 0;

		// Jeu d'UV demande par chaque carte. 0 ou 1, et ce n'est PAS une garantie
		// que le maillage porte ce jeu (cf. cgpbr::TextureRef).
		std::array<std::uint8_t, static_cast<std::size_t> (cgpbr::MapSlot::count)> mapUvSet {};

		// Copies du materiau source. Detenues ici pour la meme raison que
		// `projection` : le materiau d'origine peut disparaitre, et son pointeur
		// n'est jamais relu. `toPhong` jette les trois, et le chemin
		// d'echantillonnage les reclamera.
		cgpbr::Factors   pbrFactors {};
		cgpbr::AlphaMode alphaMode   = cgpbr::AlphaMode::opaque;
		bool             doubleSided = false;
	};

	// Libere les objets GL d'une entree et la remet a l'etat libre.
	void ReleaseEntry (Entry& entry);

	// Prend un objet de texture par carte portee par `src` -- cree au premier
	// usage, partage ensuite. N'echantillonne rien et ne laisse aucune liaison
	// derriere elle.
	void UploadPbrMaps (Entry& entry, const MaterialPbr& src);

	std::vector<Entry> m_materials;

	// Deduplication en O(1). Les emplacements liberes sont reutilises : c'est sur
	// parce que le seul detenteur d'indices est rendering_element_s::materialCache,
	// recalcule a chaque changement de revision du maillage, et vide pour un
	// maillage retire (GetMaterialRendererIds rend un vecteur vide des que
	// pMesh est nul).
	std::unordered_map<const Material*, int> m_indexByMaterial;
	std::vector<int> m_freeSlots;
};

// LoadTexture(char*) a ete retiree avec le decodeur TGAImg qu'elle etait seule a
// utiliser : jamais appelee, et compilee sous #ifndef WIN32 alors que cgre n'est
// construit qu'avec ENABLE_SINAIA, donc sous Windows. Pour charger une texture,
// passer par Img::load(), qui dispatche sur l'extension (TGA compris).
