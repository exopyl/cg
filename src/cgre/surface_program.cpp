#include "surface_program.h"

#include "diagnostics.h"
#include "gl_program.h"

#include <map>
#include <string>

namespace cgre
{

namespace
{
	// ACTIF PAR DEFAUT depuis la validation par captures de reference (18 paires,
	// 2 modeles x 3 points de vue x 3 modes d'ombrage ; voir
	// sinaia/tools/shader-captures.ps1). Ecart maximal 70/255, et surtout STABLE
	// d'un point de vue a l'autre -- un reflet mal place aurait varie avec la vue.
	//
	// La bascule reste disponible (`shader off`) : elle est le seul moyen de
	// comparer A/B dans une meme session, et cgre n'a pas de test de rendu.
	bool      g_enabled = true;
	bool      g_buildAttempted = false;
	GlProgram g_program;

	bool      g_pbrBuildAttempted = false;
	GlProgram g_pbrProgram;
	int       g_pbrTangentLoc = -1;

	bool      g_extrasBuildAttempted = false;
	GlProgram g_extrasProgram;

	// Canal d'inspection : `off` hors mesure, et c'est ce qui garantit que le
	// rendu nominal est celui d'avant. Aucune persistance, aucun reglage : une
	// session qui ne le pose pas ne le voit pas.
	PbrChannel g_pbrChannel = PbrChannel::off;

	// Environnement : eteint par defaut. Une session qui ne le pose pas rend
	// exactement l'image d'avant E7.
	PbrEnvironment g_pbrEnv;

	//
	// LITTERAUX BRUTS R"()", et ce n'est pas un detail de style.
	//
	// L'ancien GL_Shader du module ecrivait ses sources en concatenant des
	// litteraux adjacents SANS saut de ligne :
	//     "#version 330 core"  "layout(location = 0) in vec3 aPos;"
	// ce qui produit « #version 330 corelayout(...) » sur une seule ligne. La
	// directive #version doit terminer sa ligne : ce shader n'a jamais pu
	// compiler, et rien ne le signalait. Un litteral brut rend la faute
	// impossible.
	//

	const char* kVertexSource = R"GLSL(
#version 120

// Sorties vers le fragment. On passe en espace OEIL : c'est l'espace dans lequel
// gl_LightSource[] exprime ses positions, donc le seul ou l'eclairage se calcule
// sans conversion supplementaire.
varying vec3 vNormalEye;
varying vec3 vPosEye;
varying vec4 vColor;

void main ()
{
    vPosEye    = vec3 (gl_ModelViewMatrix * gl_Vertex);
    vNormalEye = gl_NormalMatrix * gl_Normal;
    vColor     = gl_Color;

    gl_TexCoord[0] = gl_MultiTexCoord0;

    // PLAN DE COUPE. glClipPlane est ignore des qu'un programme est lie, SAUF si
    // le vertex shader ecrit gl_ClipVertex -- auquel cas le materiel reprend
    // exactement le plan pose par glClipPlane, en espace oeil. C'est ce qui
    // permet de ne rien changer cote hote ni dans mesh_renderer : la coupe,
    // pilotee au clavier, continue de fonctionner a l'identique.
    //
    // L'alternative (gl_ClipDistance + glEnable(GL_CLIP_DISTANCE0)) exigerait
    // #version 130 et de modifier les appelants pour rien de plus.
    gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;

    // ftransform() plutot que gl_ModelViewProjectionMatrix * gl_Vertex : la
    // specification garantit qu'il reproduit EXACTEMENT la transformation du
    // pipeline fixe. Sans cela, le z du chemin shader et celui des surcouches --
    // qui restent en fixe-fonction -- pourraient differer d'un ulp et faire
    // clignoter le fil de fer contre la surface.
    gl_Position = ftransform ();
}
)GLSL";

	const char* kFragmentSource = R"GLSL(
#version 120

uniform sampler2D uAlbedo;       // unite 0
uniform sampler2D uReflection;   // unite 1

uniform bool  uUseTexture;
uniform bool  uUseReflection;
uniform bool  uUseVertexColors;
uniform bool  uLighting;
uniform float uReflAmount;

// VARIANTE « EXTRAS » : tout ce qui suit un #ifdef CG_PHONG_EXTRAS n'existe que
// dans le programme PhongExtrasProgram, construit a partir de CETTE source avec
// la definition injectee apres #version. Le programme de surface ordinaire ne la
// definit pas : le preprocesseur retire ces blocs AVANT l'analyse, donc la suite
// de lexemes qu'il compile est celle d'avant leur ajout -- c'est ce qui le garde
// identique pour tout materiau, et non un argument sur des uniformes a zero.
#ifdef CG_PHONG_EXTRAS
// Unites : celles du chemin PBR (cgre::PbrTextureUnit), posees par l'hote. Les
// cartes y sont deja televersees par MaterialRenderer::UploadPbrMaps, dans le
// format de leur emplacement : sRGB pour l'emissive, lineaire pour les autres.
uniform sampler2D uEmissiveMap;
uniform sampler2D uOcclusionMap;
uniform sampler2D uNormalMap;

uniform vec3  uEmissiveFactor;      // LINEAIRE, comme dans glTF
uniform bool  uUseEmissiveMap;
uniform bool  uUseOcclusionMap;
uniform float uOcclusionStrength;
uniform bool  uUseNormalMap;
uniform float uNormalScale;
uniform float uAlphaCutoff;         // negatif : pas de decoupe

// Occlusion du fragment, lue par lightContribution sur ses seuls termes
// AMBIANTS. Une globale plutot qu'un parametre : la signature de la fonction
// reste celle du programme ordinaire.
float gOcclusion = 1.0;
#endif

varying vec3 vNormalEye;
varying vec3 vPosEye;
varying vec4 vColor;

// Contribution d'une lumiere DIRECTIONNELLE. sinaia n'en pose que de ce type
// (wxOpenGLCanvas.cpp : deux gl_LightSource dont la position a w = 0), donc on
// lit la position comme une direction et l'on ignore l'attenuation.
vec3 lightContribution (int i, vec3 N, vec3 V, vec3 diffuseColor)
{
    vec3 L = normalize (vec3 (gl_LightSource[i].position));
    vec3 H = normalize (L + V);

    float nDotL = max (dot (N, L), 0.0);
    float nDotH = max (dot (N, H), 0.0);

    vec3 ambient = vec3 (gl_LightSource[i].ambient) * vec3 (gl_FrontMaterial.ambient);
#ifdef CG_PHONG_EXTRAS
    ambient *= gOcclusion;
#endif
    vec3 diffuse = vec3 (gl_LightSource[i].diffuse) * diffuseColor * nDotL;

    // Le speculaire ne s'ajoute que sur une face effectivement eclairee : sinon
    // un lisere brillant apparait sur la silhouette des faces detournees.
    vec3 specular = vec3 (0.0);
    if (nDotL > 0.0)
        specular = vec3 (gl_LightSource[i].specular) * vec3 (gl_FrontMaterial.specular)
                 * pow (nDotH, max (gl_FrontMaterial.shininess, 1.0));

    return ambient + diffuse + specular;
}

// COORDONNEES DE SPHERE MAP, formule exacte de la specification OpenGL pour
// GL_SPHERE_MAP. Reproduite telle quelle plutot qu'approchee par
// normalize(N).xy * 0.5 + 0.5 : l'approximation fait GLISSER le reflet sur la
// silhouette, ce qui se voit sur les pieces chromees du depot.
vec2 sphereMapCoord (vec3 N, vec3 posEye)
{
    vec3  u = normalize (posEye);
    vec3  r = reflect (u, N);
    float m = 2.0 * sqrt (r.x * r.x + r.y * r.y + (r.z + 1.0) * (r.z + 1.0));
    return vec2 (r.x / m + 0.5, r.y / m + 0.5);
}

#ifdef CG_PHONG_EXTRAS
// CARTE DE NORMALES SANS ATTRIBUT TANGENTE : la base est tiree des DERIVEES
// ECRAN de la position et des UV (« cotangent frame », Schueler ; meme forme que
// getTangentFrame de three.js). Le programme de surface n'a pas d'attribut
// generique, et en ajouter un rouvrirait le piege de la location 0, aliasee sur
// gl_Vertex en profil de compatibilite.
//
// `N` est la normale DEJA retournee pour les faces vues de dos, et `faceDir`
// vaut -1 pour elles : le retournement de N inverse aussi T et B, que faceDir
// remet dans le sens des UV.
//
// LA COMPOSANTE VERTE EST INVERSEE. Une base tiree des derivees suit v croissant,
// alors que la convention de glTF -- dont les UV ne sont pas retournees a
// l'import -- oriente la bitangente a l'oppose : c'est la correction que fait
// three.js (normalScale.y = -normalScale.y) quand il n'a pas de tangentes.
vec3 perturbNormal (vec3 N, float faceDir)
{
    vec3 q0  = dFdx (vPosEye);
    vec3 q1  = dFdy (vPosEye);
    vec2 st0 = dFdx (gl_TexCoord[0].st);
    vec2 st1 = dFdy (gl_TexCoord[0].st);

    vec3 q1perp = cross (q1, N);
    vec3 q0perp = cross (N, q0);
    vec3 T = q1perp * st0.x + q0perp * st1.x;
    vec3 B = q1perp * st0.y + q0perp * st1.y;

    // UV degenerees (derivees nulles) : la base s'effondre et la normale
    // geometrique est conservee, plutot qu'un normalize de zero -- NaN.
    float det = max (dot (T, T), dot (B, B));
    if (det <= 0.0)
        return N;
    float scale = faceDir * inversesqrt (det);

    vec3 t = texture2D (uNormalMap, gl_TexCoord[0].st).xyz * 2.0 - 1.0;
    t.xy *= uNormalScale;
    t.y   = -t.y;
    vec3 n = mat3 (T * scale, B * scale, N) * t;
    return (dot (n, n) > 0.0) ? normalize (n) : N;
}
#endif

void main ()
{
    vec3 N = normalize (vNormalEye);

    // DEUX FACES. sinaia pose glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE) et
    // desactive le facettage arriere, precisement parce que les OBJ importes ont
    // des enroulements incoherents (voir le commentaire de InitGL). Sans ce
    // retournement, toute face vue de dos sortirait noire -- c'est le cas nominal
    // de l'application, pas un cas limite.
    if (!gl_FrontFacing)
        N = -N;

#ifdef CG_PHONG_EXTRAS
    // APRES le retournement, comme au programme PBR -- mais la base, elle, est
    // recalculee ici a partir de N retourne, d'ou le signe de face transmis.
    if (uUseNormalMap)
        N = perturbNormal (N, gl_FrontFacing ? 1.0 : -1.0);
    if (uUseOcclusionMap)
        gOcclusion = mix (1.0, texture2D (uOcclusionMap, gl_TexCoord[0].st).r,
                          uOcclusionStrength);
#endif

    vec3 V = normalize (-vPosEye);   // l'oeil est a l'origine en espace oeil

    // D'OU VIENT LA COULEUR DIFFUSE. GL_COLOR_MATERIAL n'existe plus sous
    // programme lie : le choix que le pipeline fixe faisait par un glEnable se
    // fait ici, et seulement ici.
    vec4 base = gl_FrontMaterial.diffuse;
    if (uUseVertexColors)
        base = vColor;

    // LE TEXEL EST TENU A PART DE `base`, parce qu'il ne module pas la seule
    // diffuse : il module la couleur ECLAIREE ENTIERE. C'est la regle du
    // pipeline fixe, et cgre ne pose jamais GL_SEPARATE_SPECULAR_COLOR -- donc
    // GL_SINGLE_COLOR, le defaut, replie ambiante, diffuse, speculaire ET
    // emission dans la couleur primaire, que GL_MODULATE multiplie ensuite par
    // le texel. N'en moduler que la diffuse laisserait un speculaire blanc
    // non teinte, qui delave la texture de tout materiau a speculaire dominant.
    // Sans texture, `texel` vaut 1 et aucun terme n'est touche.
    vec4 texel = vec4 (1.0);
    if (uUseTexture)
        texel = texture2D (uAlbedo, gl_TexCoord[0].st);

    // ECLAIRAGE DESACTIVE. glDisable(GL_LIGHTING) ne decide plus rien sous
    // programme lie : l'etat doit arriver par uniforme, comme uUseVertexColors.
    // Le pipeline fixe rend alors la couleur courante modulee par la texture,
    // sans ambiante globale ni emission.
    //
    // `base` EST ECRETE ICI parce que rien ne le borne en amont. Le pendant
    // fixe de cette couleur est glColor, qui ecrete ses composantes a la
    // specification ; glMaterialfv, lui, ne les ecrete pas, et un Kd de MTL
    // traverse l'import sans borne. Un materiau a Kd = 2 rendrait donc
    // 2 x texel ici contre 1 x texel en fixe.
#ifdef CG_PHONG_EXTRAS
    // DECOUPE ALPHA (mode `mask`), sur l'alpha que ce programme ecrit. Seuil
    // negatif hors `mask` : rien n'est jamais ecarte.
    if (base.a * texel.a < uAlphaCutoff)
        discard;

    // EMISSION glTF : facteur x carte, en LINEAIRE -- la carte est televersee en
    // sRGB, le materiel la rend donc deja lineaire. Encodee comme la sortie du
    // programme PBR, puis AJOUTEE apres le texturage : une emission n'est pas
    // teinte par la couleur de base, contrairement a ce que ferait GL_MODULATE.
    vec3 emission = uEmissiveFactor;
    if (uUseEmissiveMap)
        emission *= texture2D (uEmissiveMap, gl_TexCoord[0].st).rgb;
    emission = pow (clamp (emission, 0.0, 1.0), vec3 (1.0 / 2.2));
#endif

    if (!uLighting)
    {
        gl_FragColor = clamp (base, 0.0, 1.0) * texel;
#ifdef CG_PHONG_EXTRAS
        gl_FragColor.rgb = min (gl_FragColor.rgb + emission, 1.0);
#endif
        return;
    }

#ifdef CG_PHONG_EXTRAS
    // gl_FrontMaterial.emission N'EST PAS LUE : pour un materiau PBR elle porte
    // la projection du MEME facteur emissif (cas non texture), qui compterait
    // alors deux fois. L'emission vient de l'uniforme, dans les deux cas.
    vec3 color = vec3 (gl_LightModel.ambient) * vec3 (gl_FrontMaterial.ambient) * gOcclusion;
#else
    vec3 color = vec3 (gl_LightModel.ambient) * vec3 (gl_FrontMaterial.ambient)
               + vec3 (gl_FrontMaterial.emission);
#endif
    color += lightContribution (0, N, V, base.rgb);
    color += lightContribution (1, N, V, base.rgb);

    // ECRETAGE AVANT TEXTURAGE, et non a l'ecriture du fragment. Le pipeline
    // fixe borne la couleur primaire a [0,1] a la sortie de l'equation
    // d'eclairage, donc AVANT que l'etage de texturage ne s'en saisisse : le
    // sur-eclairement est PERDU, il ne teinte pas le texel. Sans cette borne, un
    // sommet de lobe speculaire a 1,5 rendrait 1,5 x texel la ou le fixe rend le
    // texel seul -- un ecart qui croit avec le niveau speculaire du materiau.
    // Un materiau sans texture est concerne au meme titre : `texel` vaut alors
    // 1, mais l'ecretage, lui, s'applique.
    color = clamp (color, 0.0, 1.0) * texel.rgb;

    // CARTE DE REFLEXION : un MELANGE, pas une addition. Une carte
    // d'environnement est claire par nature ; l'ajouter a pleine intensite
    // saturait la surface et donnait des pieces chromees uniformement blanchies.
    // Le melange conserve l'energie -- plus le materiau reflechit, moins on voit
    // sa diffuse -- ce qui est exactement le sens d'un « montant de reflexion ».
    // Les 20 appels d'etat de BindReflectionUnit (GL_COMBINE / GL_INTERPOLATE /
    // GL_SPHERE_MAP) tiennent ici en trois lignes. Le melange vient APRES la
    // modulation par l'albedo : l'unite de reflexion est la seconde, elle
    // combine ce que la premiere a produit.
    if (uUseReflection)
    {
        vec3 refl = texture2D (uReflection, sphereMapCoord (N, vPosEye)).rgb;
        color = mix (color, refl, clamp (uReflAmount, 0.0, 1.0));
    }

#ifdef CG_PHONG_EXTRAS
    color = min (color + emission, 1.0);
#endif

    gl_FragColor = vec4 (color, base.a * texel.a);
}
)GLSL";

	// ------------------------------------------------------------------------
	//  PROGRAMME PBR -- metallic-roughness
	// ------------------------------------------------------------------------
	// Le vertex shader duplique celui de surface plutot que de le partager, et
	// la duplication a paye : il porte l'attribut generique de tangente, que le
	// programme de surface n'a pas et n'aura pas.
	const char* kPbrVertexSource = R"GLSL(
#version 120

// TANGENTE, attribut GENERIQUE : le pipeline fixe n'a pas d'equivalent integre.
// vec4 comme dans glTF -- xyz la tangente, w la POIGNEE, qui vaut +1 ou -1 selon
// l'orientation du repere UV. La bitangente s'en deduit et n'est pas televersee.
attribute vec4 aTangent;

varying vec3 vNormalEye;
varying vec3 vPosEye;
varying vec3 vTangentEye;
varying vec3 vBitangentEye;

void main ()
{
    vPosEye    = vec3 (gl_ModelViewMatrix * gl_Vertex);
    vNormalEye = gl_NormalMatrix * gl_Normal;

    // ⚠ gl_NormalMatrix N'EST PAS LA TRANSFORMATION JUSTE D'UNE TANGENTE. Une
    // normale est un COVECTEUR et se transporte par l'inverse transposee ; une
    // tangente est un VECTEUR et se transporte par le bloc 3x3 de la modelview.
    // Les deux coincident tant qu'il n'y a pas d'echelle NON UNIFORME, ce que
    // sinaia ne pose pas, et la re-orthogonalisation du fragment absorbe de
    // toute facon la composante hors-plan. A corriger le jour ou une echelle
    // non uniforme apparait -- pas avant, pour ne pas changer un rendu juste.
    //
    // La bitangente se recalcule APRES transformation : la transporter
    // elle-meme donnerait un repere non orthogonal des que la matrice n'est pas
    // une rotation pure.
    vTangentEye   = gl_NormalMatrix * aTangent.xyz;
    vBitangentEye = cross (vNormalEye, vTangentEye) * aTangent.w;

    gl_TexCoord[0] = gl_MultiTexCoord0;

    // Meme raison qu'au programme de surface : glClipPlane est ignore sous
    // programme lie tant que le vertex shader n'ecrit pas gl_ClipVertex.
    gl_ClipVertex = gl_ModelViewMatrix * gl_Vertex;
    gl_Position   = ftransform ();
}
)GLSL";

	const char* kPbrFragmentSource = R"GLSL(
#version 120

// L'unite de chaque echantillonneur est posee par l'hote, depuis
// cgre::PbrTextureUnit : ne pas la recopier ici, un nombre ecrit a deux endroits
// finit par en designer deux.
uniform sampler2D uBaseColorMap;
uniform sampler2D uNormalMap;
uniform sampler2D uMetallicRoughnessMap;
uniform sampler2D uEmissiveMap;

uniform bool  uHasBaseColorMap;
uniform bool  uHasNormalMap;
uniform bool  uHasMetallicRoughnessMap;
uniform bool  uHasEmissiveMap;
uniform float uNormalScale;

uniform vec4  uBaseColorFactor;
uniform vec3  uEmissiveFactor;
uniform float uMetallicFactor;
uniform float uRoughnessFactor;
uniform bool  uLighting;

// CANAL D'INSPECTION. 0 = rendu nominal, et c'est le defaut : toute autre valeur
// ecrit un CANAL BRUT a la place de la couleur eclairee.
//
// Ce n'est pas un mode d'affichage mais un INSTRUMENT DE MESURE. Un masque de
// segmentation tire du rendu final depend de l'eclairage, donc du cadrage, donc
// de ce qu'on veut justement mesurer ; tire d'ici, il ne depend que de la
// donnee. La segmentation par seuil de chroma qu'il remplace faisait basculer le
// SIGNE de l'ecart bois - ferrures selon le seuil choisi -- de -0,1204 a 0,10
// jusqu'a +0,0849 a 0,30 -- ce qui ne mesurait pas le modele mais le reglage.
//
// Les valeurs sont celles de cgre::PbrChannel, posees par l'hote : ne pas les
// recopier ici, un nombre ecrit a deux endroits finit par en designer deux.
uniform int uChannel;

// ENVIRONNEMENT ANALYTIQUE : un degrade lineaire ciel/sol, MONOCHROME, sans
// aucune texture ni LUT.
//
// IL EST EVALUE EN ESPACE OEIL, et cette justification est mesuree, non
// esthetique. Les deux lampes de sinaia sont posees dans InitGL avec la
// modelview a l'identite : leur `GL_POSITION` est donc deja exprime en espace
// oeil, elles SONT attachees a la camera. Un environnement en espace oeil est
// coherent avec elles. L'attacher au monde exigerait gl_ModelViewMatrixInverse
// -- une matrice de plus -- et rendrait l'environnement fixe pendant que les
// lampes tournent avec la vue, ce qui serait un eclairage contradictoire.
//
// `uEnvEnabled` faux n'est pas « un degrade nul » mais un SAUT DU BLOC : c'est
// ce qui garantit par construction, et non par un argument sur l'arithmetique
// flottante, que `env off` rend l'image d'avant E7 au bit pres.
uniform bool  uEnvEnabled;
uniform float uEnvSky;      // radiance au zenith
uniform float uEnvGround;   // radiance au nadir

varying vec3 vNormalEye;
varying vec3 vPosEye;
varying vec3 vTangentEye;
varying vec3 vBitangentEye;

const float kPi = 3.14159265358979;

// Distribution des microfacettes, Trowbridge-Reitz. `a` est la rugosite
// PERCEPTUELLE au carre : c'est la convention de glTF, et la confondre avec la
// rugosite elle-meme donne des reflets nettement trop larges aux valeurs basses.
float distributionGGX (float nDotH, float a)
{
    float a2 = a * a;
    float d  = nDotH * nDotH * (a2 - 1.0) + 1.0;
    return a2 / (kPi * d * d);
}

// Occultation geometrique, Schlick-GGX en formulation de VISIBILITE : le
// denominateur 4 nDotL nDotV de la BRDF est deja absorbe ici, il ne doit donc
// pas etre applique une seconde fois a l'appelant.
float visibilitySmith (float nDotV, float nDotL, float a)
{
    float k = a * 0.5;
    float gv = nDotV * (1.0 - k) + k;
    float gl = nDotL * (1.0 - k) + k;
    return 0.25 / max (gv * gl, 1e-4);
}

vec3 fresnelSchlick (vec3 f0, float vDotH)
{
    // BORNE AVANT LA PUISSANCE : le produit scalaire de deux vecteurs
    // normalises peut rendre 1 + epsilon en flottant, et pow d'une base
    // negative n'est pas defini en GLSL.
    return f0 + (1.0 - f0) * pow (clamp (1.0 - vDotH, 0.0, 1.0), 5.0);
}

// Contribution d'une lumiere DIRECTIONNELLE, comme au programme de surface :
// sinaia n'en pose que de ce type, position a w = 0.
vec3 lightContribution (int i, vec3 N, vec3 V, vec3 diffuseColor, vec3 f0, float a)
{
    vec3  L = normalize (vec3 (gl_LightSource[i].position));
    vec3  H = normalize (L + V);

    float nDotL = max (dot (N, L), 0.0);
    if (nDotL <= 0.0)
        return vec3 (0.0);

    float nDotV = max (dot (N, V), 1e-4);
    float nDotH = max (dot (N, H), 0.0);
    float vDotH = max (dot (V, H), 0.0);

    vec3  F    = fresnelSchlick (f0, vDotH);
    vec3  spec = F * (distributionGGX (nDotH, a) * visibilitySmith (nDotV, nDotL, a));

    // Ce que la reflexion speculaire n'a pas pris s'en va en diffus : c'est la
    // conservation d'energie qui fait qu'un metal n'a pas de diffuse, sans avoir
    // a l'ecrire comme un cas particulier.
    vec3  kD   = vec3 (1.0) - F;

    return (kD * diffuseColor / kPi + spec) * vec3 (gl_LightSource[i].diffuse) * nDotL;
}

// Radiance du degrade dans la direction `d`, en espace oeil. La forme
// a + b*d.y est celle qui rend la solution analytique ci-dessous exacte.
float envRadiance (vec3 d)
{
    float a = 0.5 * (uEnvSky + uEnvGround);
    float b = 0.5 * (uEnvSky - uEnvGround);
    return a + b * d.y;
}

// IRRADIANCE DIFFUSE, EXACTE POUR CE DEGRADE et non approchee.
//
// L'integrale ponderee par le cosinus d'une radiance a + b*d.y sur l'hemisphere
// de normale N vaut a + (2/3) b N.y : le terme constant passe tel quel, et
// l'integrale de d (N.d) sur l'hemisphere vaut (2 pi / 3) N. Il n'y a donc rien
// a prefiltrer pour le diffus, et aucun echantillonnage.
float envIrradiance (vec3 n)
{
    float a = 0.5 * (uEnvSky + uEnvGround);
    float b = 0.5 * (uEnvSky - uEnvGround);
    return a + (2.0 / 3.0) * b * n.y;
}

// TERME D'ENVIRONNEMENT DE LA BRDF, approximation analytique de Lazarov
// popularisee par Karis. Elle remplace la table 2D que le rendu differe
// preferre : aucune texture, aucune unite de plus, aucun etat GL.
//
// Rend (echelle, biais) a appliquer a F0 : F0 * x + y.
vec2 envBrdfApprox (float nDotV, float roughness)
{
    const vec4 c0 = vec4 (-1.0, -0.0275, -0.572,  0.022);
    const vec4 c1 = vec4 ( 1.0,  0.0425,  1.040, -0.040);
    vec4 r = roughness * c0 + c1;
    float a004 = min (r.x * r.x, exp2 (-9.28 * nDotV)) * r.x + r.y;
    return vec2 (-1.04, 1.04) * a004 + r.zw;
}

void main ()
{
    vec3 N = normalize (vNormalEye);

    // CARTE DE NORMALES. Le texel est un vecteur dans l'espace TANGENT, encode
    // de [-1,1] vers [0,1] : la conversion inverse est exacte, la carte etant
    // televersee en GL_RGBA8 -- porteuse de DONNEES et non de couleur, donc
    // jamais decodee en sRGB par le materiel.
    //
    // uNormalScale ne s'applique qu'aux composantes TANGENTIELLES : c'est la
    // definition de glTF, et mettre a l'echelle z aplatirait le relief au lieu
    // de l'accentuer.
    //
    // L'UNIFORME DE PRESENCE VAUT AUSSI POUR LA BASE TANGENTE : l'hote ne le
    // leve que si le maillage porte un tampon de tangentes. Sans lui, aTangent
    // vaudrait l'attribut generique par defaut et la base serait degeneree.
    if (uHasNormalMap)
    {
        vec3 t = texture2D (uNormalMap, gl_TexCoord[0].st).xyz * 2.0 - 1.0;
        t.xy *= uNormalScale;

        // Re-orthogonalisation de Gram-Schmidt : l'interpolation par sommet ne
        // preserve pas l'orthogonalite du repere, et un repere oblique inclinerait
        // la normale meme sur une carte parfaitement plate.
        //
        // LE RESIDU EST TESTE, AU MEME SEUIL QUE LE PRODUCTEUR -- tangents.cpp,
        // kLengthEpsilon2 = 1e-20 sur le carre de la longueur. Une tangente
        // nulle arrive jusqu'ici : l'import ecrit (0,0,0,w) quand il n'a pas pu
        // normaliser, SetVertexTangents ne controle que la taille du tableau, et
        // AreTangentsValid ne regarde pas le contenu. normalize (vec3 (0)) rend
        // alors NaN, que GGX propage et que clamp ne rattrape pas -- son
        // comportement sur NaN est implementation-definie en GLSL.
        //
        // Le meme seuil couvre le cas QUASI colineaire, qui n'est pas NaN mais
        // amplifie sans borne le bruit d'interpolation. Il est d'autant plus
        // atteignable que les normales sont recalculees par ComputeNormals
        // tandis que les tangentes viennent du fichier : les deux divergent sur
        // une arete vive.
        vec3 t0 = vTangentEye - N * dot (N, vTangentEye);
        vec3 b0 = vBitangentEye - N * dot (N, vBitangentEye);
        if (dot (t0, t0) >= 1e-20)
        {
            vec3 T = normalize (t0);
            b0 -= T * dot (T, b0);
            if (dot (b0, b0) >= 1e-20)
                N = normalize (mat3 (T, normalize (b0), N) * t);
        }
        // Base degeneree : la normale geometrique est conservee telle quelle.
    }

    // APRES la perturbation, et non avant : retourner d'abord inverserait aussi
    // le sens de la base tangente.
    if (!gl_FrontFacing)
        N = -N;
    vec3 V = normalize (-vPosEye);

    // LES CARTES DE COULEUR SONT EN sRGB DANS L'OBJET DE TEXTURE, donc le
    // materiel les rend deja LINEAIRES ici. Aucune correction a l'echantillon :
    // elle arriverait apres le filtrage, donc trop tard.
    vec4 base = uBaseColorFactor;
    if (uHasBaseColorMap)
        base *= texture2D (uBaseColorMap, gl_TexCoord[0].st);

    float metallic  = uMetallicFactor;
    float roughness = uRoughnessFactor;
    if (uHasMetallicRoughnessMap)
    {
        // Convention glTF : G porte la rugosite, B le metallique. Le canal R
        // n'a AUCUN sens dans le coeur du format et n'est pas lu -- sur Lantern
        // il vaut zero partout, et le prendre pour une occlusion eteindrait le
        // modele.
        vec4 mr = texture2D (uMetallicRoughnessMap, gl_TexCoord[0].st);
        roughness *= mr.g;
        metallic  *= mr.b;
    }

    vec3 emissive = uEmissiveFactor;
    if (uHasEmissiveMap)
        emissive *= texture2D (uEmissiveMap, gl_TexCoord[0].st).rgb;

    metallic  = clamp (metallic,  0.0, 1.0);
    roughness = clamp (roughness, 0.04, 1.0);
    float a   = roughness * roughness;

    // SORTIE BRUTE, ni eclairee ni encodee. L'encodage sRGB de sortie est une
    // presentation ; un masque doit lire la grandeur, pas sa presentation.
    //
    // Place ICI parce que les quatre canaux exigent que la carte de normales ait
    // deja perturbe N et que les facteurs aient deja multiplie les cartes : plus
    // haut, `normal` rendrait la normale geometrique, et `metallic` le facteur
    // sans sa carte.
    if (uChannel != 0)
    {
        vec3 raw = vec3 (0.0);
        if      (uChannel == 1) raw = vec3 (metallic);
        else if (uChannel == 2) raw = vec3 (roughness);
        else if (uChannel == 3) raw = N * 0.5 + 0.5;
        else if (uChannel == 4) raw = base.rgb;
        else if (uChannel == 5) raw = emissive;
        gl_FragColor = vec4 (raw, 1.0);
        return;
    }

    // 0,04 : la reflectance normale des dielectriques courants. Un metal prend
    // sa couleur de base comme reflectance et perd sa diffuse.
    vec3 f0           = mix (vec3 (0.04), base.rgb, metallic);
    vec3 diffuseColor = base.rgb * (1.0 - metallic);

    // ECLAIRAGE DESACTIVE : la couleur de base seule, emission comprise. Il n'y
    // a pas d'equation a evaluer, et le texel reste l'information utile.
    vec3 color;
    if (!uLighting)
    {
        color = base.rgb + emissive;
    }
    else
    {
        // IRRADIANCE AMBIANTE CONSTANTE, faute d'environnement preffiltre. Elle
        // alimente AUSSI la reflectance speculaire : un metal n'ayant pas de
        // diffuse, une ambiante purement diffuse laisserait les ferrures noires
        // partout hors du point speculaire.
        color = vec3 (gl_LightModel.ambient) * (diffuseColor + f0);

        // ENVIRONNEMENT, EN SUS DE L'AMBIANTE CONSTANTE et non a sa place.
        //
        // Le remplacer la ferait BAISSER la luminance moyenne : l'ambiante vaut
        // 0,12 quand la moyenne du degrade retenu vaut moins. Ce que ce terme
        // apporte de neuf n'est donc pas un niveau -- ce serait une exposition
        // deguisee -- mais une VARIATION SPATIALE : le diffus suit N, le
        // speculaire suit la direction reflechie, la ou l'ambiante ne suit rien.
        if (uEnvEnabled)
        {
            // Diffus : irradiance exacte du degrade, sans echantillonnage.
            color += diffuseColor * envIrradiance (N);

            // Speculaire : le degrade evalue dans la direction reflechie, ELARGI
            // PAR LA RUGOSITE -- interpolation vers la moyenne du degrade, qui
            // est la limite du lobe quand il couvre l'hemisphere. C'est ce qui
            // remplace le prefiltrage, et c'est pourquoi aucun LOD explicite
            // n'est necessaire : il n'y a pas de chaine de mipmaps a parcourir.
            vec3  R    = reflect (-V, N);
            float moy  = 0.5 * (uEnvSky + uEnvGround);
            float pref = mix (envRadiance (R), moy, roughness);

            float nDotV = max (dot (N, V), 0.0);
            vec2  ab    = envBrdfApprox (nDotV, roughness);
            color += pref * (f0 * ab.x + ab.y);
        }

        color += lightContribution (0, N, V, diffuseColor, f0, a);
        color += lightContribution (1, N, V, diffuseColor, f0, a);
        color += emissive;
    }

    // ENCODAGE DE SORTIE DANS LE FRAGMENT, et non par GL_FRAMEBUFFER_SRGB :
    // celui-ci re-encoderait aussi les surcouches, le repere et la grille, qui
    // sont dessines en fixe-fonction dans le meme tampon et sont deja en sRGB.
    color = pow (clamp (color, 0.0, 1.0), vec3 (1.0 / 2.2));

    gl_FragColor = vec4 (color, base.a);
}
)GLSL";
}

const GlProgram* SurfaceProgram ()
{
	if (g_buildAttempted)
		return g_program.IsValid () ? &g_program : nullptr;

	// Une seule tentative par processus : un shader qui ne compile pas ne
	// compilera pas davantage a l'image suivante, et reessayer inonderait le
	// journal a la cadence du rendu.
	g_buildAttempted = true;

	std::map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER]   = kVertexSource;
	sources[GL_FRAGMENT_SHADER] = kFragmentSource;

	if (!g_program.Build (sources, "surface"))
	{
		Log ("Programme de surface indisponible : on garde le pipeline fixe.");
		return nullptr;
	}

	Log ("Programme de surface compile et lie.");
	return &g_program;
}

const GlProgram* PbrProgram ()
{
	if (g_pbrBuildAttempted)
		return g_pbrProgram.IsValid () ? &g_pbrProgram : nullptr;

	// Meme regle que le programme de surface : une seule tentative par
	// processus. Un shader qui ne compile pas ne compilera pas davantage a
	// l'image suivante, et reessayer inonderait le journal a la cadence du rendu.
	g_pbrBuildAttempted = true;

	// LE NOMBRE D'UNITES EST VERIFIE, PAS SUPPOSE. Sur une implementation qui en
	// expose moins que ce chemin n'en demande, glActiveTexture au-dela du
	// maximum rend GL_INVALID_ENUM SANS CHANGER l'unite active : le
	// glBindTexture suivant lierait donc la carte sur l'unite precedente, et le
	// fragment lirait l'emissive comme rugosite. Modele faux, rien de visible
	// hors de la file d'erreurs.
	//
	// Refuser la construction est exactement ce pour quoi le repli de Phong est
	// conserve. Le minimum garanti par GL 2.0 est de 2 unites fragment.
	GLint maxUnits = 0;
	glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
	if (maxUnits < kPbrUnitEmissive + 1)
	{
		Logf ("Programme PBR indisponible : %d unites de texture fragment, %d requises. "
		      "On garde la projection de Phong.",
		      (int)maxUnits, (int)kPbrUnitEmissive + 1);
		return nullptr;
	}

	std::map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER]   = kPbrVertexSource;
	sources[GL_FRAGMENT_SHADER] = kPbrFragmentSource;

	if (!g_pbrProgram.Build (sources, "pbr"))
	{
		// L'appelant retombe sur la projection de Phong, qui reste en place
		// precisement pour cela : degrade, jamais noir.
		Log ("Programme PBR indisponible : on garde la projection de Phong.");
		return nullptr;
	}

	// LOCATION DE L'ATTRIBUT TANGENTE, resolue une fois et TRACEE. En profil de
	// compatibilite, l'attribut generique 0 est aliase sur gl_Vertex : une
	// tangente ecrite a cette location ecraserait les positions et detruirait la
	// geometrie, sans la moindre erreur GL. Le compilateur choisit librement, et
	// #version 120 n'offre pas de qualificateur de location : le seul recours
	// est de relever ce qu'il a decide et de refuser le cas dangereux.
	const GLint loc = glGetAttribLocation (g_pbrProgram.Id (), "aTangent");
	if (loc <= 0)
	{
		g_pbrTangentLoc = -1;
		Logf ("Programme PBR : attribut aTangent a la location %d -- inutilisable. "
		      "Les cartes de normales resteront inactives.", (int)loc);
	}
	else
	{
		g_pbrTangentLoc = (int)loc;
		Logf ("Programme PBR compile et lie. Attribut aTangent a la location %d.",
		      g_pbrTangentLoc);
		return &g_pbrProgram;
	}

	Log ("Programme PBR compile et lie.");
	return &g_pbrProgram;
}

int PbrTangentAttribLocation ()
{
	return g_pbrTangentLoc;
}

const GlProgram* PhongExtrasProgram ()
{
	if (g_extrasBuildAttempted)
		return g_extrasProgram.IsValid () ? &g_extrasProgram : nullptr;

	// Une seule tentative par processus, meme regle que les deux autres.
	g_extrasBuildAttempted = true;

	// Memes unites que le chemin PBR, donc meme verification : au-dela du
	// maximum, glActiveTexture echoue sans changer l'unite, et la carte suivante
	// serait liee a la place de la precedente.
	GLint maxUnits = 0;
	glGetIntegerv (GL_MAX_TEXTURE_IMAGE_UNITS, &maxUnits);
	if (maxUnits < kPbrUnitEmissive + 1)
	{
		Logf ("Programme de Phong enrichi indisponible : %d unites de texture fragment, "
		      "%d requises. On garde le programme de surface.",
		      (int)maxUnits, (int)kPbrUnitEmissive + 1);
		return nullptr;
	}

	// LA MEME SOURCE que le programme de surface, avec la definition injectee
	// JUSTE APRES la ligne #version -- qui doit rester la premiere directive.
	// Une copie de la source aurait diverge au premier correctif.
	std::string fragment = kFragmentSource;
	const std::string::size_type version = fragment.find ("#version");
	const std::string::size_type eol =
		(version == std::string::npos) ? std::string::npos : fragment.find ('\n', version);
	if (eol == std::string::npos)
	{
		Log ("Programme de Phong enrichi : directive #version introuvable.");
		return nullptr;
	}
	fragment.insert (eol + 1, "#define CG_PHONG_EXTRAS 1\n");

	std::map<GLenum, std::string> sources;
	sources[GL_VERTEX_SHADER]   = kVertexSource;
	sources[GL_FRAGMENT_SHADER] = fragment;

	if (!g_extrasProgram.Build (sources, "phong-extras"))
	{
		Log ("Programme de Phong enrichi indisponible : on garde le programme de surface.");
		return nullptr;
	}

	Log ("Programme de Phong enrichi compile et lie.");
	return &g_extrasProgram;
}

void SetPbrChannel (PbrChannel channel)
{
	if (channel != g_pbrChannel)
		Logf ("Programme PBR : canal d'inspection %d.", (int) channel);
	g_pbrChannel = channel;
}

PbrChannel GetPbrChannel ()
{
	return g_pbrChannel;
}

void SetPbrEnvironment (const PbrEnvironment& env)
{
	if (env.enabled != g_pbrEnv.enabled || env.sky != g_pbrEnv.sky || env.ground != g_pbrEnv.ground)
		Logf ("Programme PBR : environnement %s, ciel %.3f, sol %.3f.",
		      env.enabled ? "actif" : "eteint", env.sky, env.ground);
	g_pbrEnv = env;
}

PbrEnvironment GetPbrEnvironment ()
{
	return g_pbrEnv;
}

void SetSurfaceShaderEnabled (bool enabled)
{
	if (enabled != g_enabled)
		Log (enabled ? "Rendu de surface : SHADER."
		             : "Rendu de surface : pipeline fixe.");
	g_enabled = enabled;
}

bool IsSurfaceShaderEnabled ()
{
	return g_enabled;
}

} // namespace cgre
