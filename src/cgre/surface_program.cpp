#include "surface_program.h"

#include "diagnostics.h"
#include "gl_program.h"

#include <map>

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
uniform float uReflAmount;

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

    vec3 V = normalize (-vPosEye);   // l'oeil est a l'origine en espace oeil

    // D'OU VIENT LA COULEUR DIFFUSE. GL_COLOR_MATERIAL n'existe plus sous
    // programme lie : le choix que le pipeline fixe faisait par un glEnable se
    // fait ici, et seulement ici.
    vec4 base = gl_FrontMaterial.diffuse;
    if (uUseVertexColors)
        base = vColor;
    if (uUseTexture)
        base *= texture2D (uAlbedo, gl_TexCoord[0].st);

    vec3 color = vec3 (gl_LightModel.ambient) * vec3 (gl_FrontMaterial.ambient)
               + vec3 (gl_FrontMaterial.emission);
    color += lightContribution (0, N, V, base.rgb);
    color += lightContribution (1, N, V, base.rgb);

    // CARTE DE REFLEXION : un MELANGE, pas une addition. Une carte
    // d'environnement est claire par nature ; l'ajouter a pleine intensite
    // saturait la surface et donnait des pieces chromees uniformement blanchies.
    // Le melange conserve l'energie -- plus le materiau reflechit, moins on voit
    // sa diffuse -- ce qui est exactement le sens d'un « montant de reflexion ».
    // Les 20 appels d'etat de BindReflectionUnit (GL_COMBINE / GL_INTERPOLATE /
    // GL_SPHERE_MAP) tiennent ici en trois lignes.
    if (uUseReflection)
    {
        vec3 refl = texture2D (uReflection, sphereMapCoord (N, vPosEye)).rgb;
        color = mix (color, refl, clamp (uReflAmount, 0.0, 1.0));
    }

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
