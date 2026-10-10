// ORDRE D'INCLUSION CONTRAINT : gl_wrapper.h est le SEUL en-tete GL de cette
// unite, et il vient en premier. Aucun en-tete de RmlUi n'entre ici.
#include "gl_wrapper.h"

#include "gl_state_fence.h"

#include "diagnostics.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace peles
{
namespace app
{

namespace
{

// Version du noyau GL, lue une fois. 0 tant qu'elle n'a pas ete lue : les
// enumerants GL_CONTEXT_PROFILE_MASK (3.2) et GL_FRAMEBUFFER_SRGB (3.0) n'ont
// pas de sens en deca.
int g_glVersionMajor = 0;
int g_glVersionMinor = 0;

// Nombre d'unites de texture relevees, DERIVE DU CONTEXTE et borne a
// GlStateSnapshot::kMaxTextureUnits. 1 tant qu'il n'a pas ete lu : l'unite 0
// existe dans toutes les versions.
int g_textureUnitCount = 1;

void ReadGlVersion ()
{
	if (glad_glGetString == nullptr)
		return;
	const GLubyte* version = glGetString (GL_VERSION);
	if (version == nullptr)
		return;
	std::sscanf ((const char*)version, "%d.%d", &g_glVersionMajor, &g_glVersionMinor);
}

bool HasProfileMask ()
{
	return g_glVersionMajor > 3 || (g_glVersionMajor == 3 && g_glVersionMinor >= 2);
}

bool IsEnabled (GLenum capability)
{
	return glIsEnabled (capability) == GL_TRUE;
}

int GetInteger (GLenum name)
{
	GLint value = 0;
	glGetIntegerv (name, &value);
	return (int)value;
}

void ReadTextureUnitCount ()
{
	if (glad_glActiveTexture == nullptr)
	{
		g_textureUnitCount = 1;
		return;
	}
	const int reported = GetInteger (GL_MAX_TEXTURE_IMAGE_UNITS);
	g_textureUnitCount = std::min (std::max (reported, 1), GlStateSnapshot::kMaxTextureUnits);
}

// LE PROFIL EST VERIFIE UNE FOIS, PAS A CHAQUE IMAGE.
//
// Sur un contexte core, glPushAttrib rend GL_INVALID_OPERATION et la cloture ne
// protege plus rien : elle deviendrait un mecanisme de verification neutralise,
// ce qui est pire que son absence.
void VerifyCompatibilityProfileOnce ()
{
	static bool verified = false;
	if (verified)
		return;
	verified = true;

	if (!HasProfileMask ())
		return;

	const int mask = GetInteger (GL_CONTEXT_PROFILE_MASK);
	if ((mask & GL_CONTEXT_COMPATIBILITY_PROFILE_BIT) != 0)
		return;

	cgre::Logf ("GlStateFence : le contexte courant n'est PAS de profil compatibilite "
	            "(GL_CONTEXT_PROFILE_MASK = 0x%X). glPushAttrib y rend GL_INVALID_OPERATION "
	            "et la cloture d'etat ne protege rien.",
	            (unsigned)mask);
}

std::string FormatFlag (bool value)
{
	return value ? "1" : "0";
}

std::string FormatInteger (int value)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "%d", value);
	return buffer;
}

std::string FormatMask (unsigned int value)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "0x%08X", value);
	return buffer;
}

std::string FormatViewport (const int value[4])
{
	char buffer[96];
	std::snprintf (buffer, sizeof (buffer), "(%d, %d, %d, %d)", value[0], value[1], value[2],
	               value[3]);
	return buffer;
}

std::string FormatMatrix (const float value[16])
{
	std::string text;
	for (int i = 0; i < 16; ++i)
	{
		char buffer[32];
		std::snprintf (buffer, sizeof (buffer), "%.6g", (double)value[i]);
		if (i != 0)
			text += ' ';
		text += buffer;
	}
	return text;
}

// ⚠ COMPARAISON BIT A BIT, et pas `==` sur les flottants. Deux matrices qui
// s'impriment pareil peuvent differer du dernier bit -- c'est exactement ce
// qu'un depilement fautif produit -- et `==` rendrait par ailleurs faux deux
// NaN identiques.
bool SameBits (const float left[16], const float right[16])
{
	return std::memcmp (left, right, 16 * sizeof (float)) == 0;
}

// Le releve de GL_MATRIX_MODE nomme le mode plutot que son enumerant : un
// « 0x1701 » dans une sortie de diagnostic se relit au dictionnaire.
std::string FormatMatrixMode (int mode)
{
	switch (mode)
	{
	case GL_MODELVIEW:
		return "GL_MODELVIEW";
	case GL_PROJECTION:
		return "GL_PROJECTION";
	case GL_TEXTURE:
		return "GL_TEXTURE";
	default:
		return FormatMask ((unsigned int)mode);
	}
}

} // namespace

bool LoadGlEntries (std::string& description)
{
	if (gladLoaderLoadGL () == 0)
	{
		description = "gladLoaderLoadGL a rendu 0 : aucune entree GL n'est resolue.";
		return false;
	}

	ReadGlVersion ();
	ReadTextureUnitCount ();

	// Les quatre entrees du chemin fixe de cgre, plus les deux de la cloture.
	struct Entry
	{
		const char* name;
		const void* pointer;
	};
	const Entry entries[] = {
		{ "glBegin", (const void*)glad_glBegin },
		{ "glPushAttrib", (const void*)glad_glPushAttrib },
		{ "glPopAttrib", (const void*)glad_glPopAttrib },
		{ "glPushClientAttrib", (const void*)glad_glPushClientAttrib },
		{ "glPopClientAttrib", (const void*)glad_glPopClientAttrib },
		{ "glVertexPointer", (const void*)glad_glVertexPointer },
		{ "glEnableClientState", (const void*)glad_glEnableClientState },
	};

	std::string missing;
	for (const Entry& entry : entries)
	{
		if (entry.pointer == nullptr)
		{
			if (!missing.empty ())
				missing += ", ";
			missing += entry.name;
		}
	}

	if (!missing.empty ())
	{
		description = "entrees GL absentes : " + missing +
		              " -- le chemin fixe de cgre est inatteignable dans ce contexte.";
		return false;
	}

	// glGetString est resolu -- il est la premiere entree que le chargeur exige
	// -- mais son retour est controle : une chaine nulle construirait un
	// std::string a partir d'un pointeur nul, donc un comportement indefini a
	// l'endroit meme ou l'on cherche a diagnostiquer.
	const GLubyte* version = glGetString (GL_VERSION);
	description = version != nullptr ? (const char*)version : "GL_VERSION indisponible";
	return true;
}

void ApplyGlobalInvariants ()
{
	// GL_FRAMEBUFFER_SRGB : INVARIANT, pas un etat a restaurer. L'enumerant
	// appartient a GL 3.0, d'ou la garde.
	if (g_glVersionMajor >= 3)
		glDisable (GL_FRAMEBUFFER_SRGB);

	// Aucun FBO lie : tout le monde dessine dans le tampon de la fenetre.
	if (glad_glBindFramebuffer != nullptr)
		glBindFramebuffer (GL_FRAMEBUFFER, 0);

	// Aucun VAO lie. En profil de compatibilite, le VAO 0 est un objet valide
	// et c'est lui que le pipeline fixe et les tableaux clients de cgre
	// utilisent ; un VAO laisse lie par un tiers ferait lire les attributs
	// AILLEURS, sans erreur GL. L'entree n'existe qu'a partir de GL 3.0.
	if (glad_glBindVertexArray != nullptr)
		glBindVertexArray (0);

	// Aucun programme lie -- le pendant a l'initialisation de ce que la cloture
	// pose a chaque bord.
	if (glad_glUseProgram != nullptr)
		glUseProgram (0);
}

GlStateSnapshot GlStateSnapshot::Capture ()
{
	GlStateSnapshot snapshot;

	if (glad_glUseProgram != nullptr)
		snapshot.currentProgram = GetInteger (GL_CURRENT_PROGRAM);

	if (glad_glActiveTexture != nullptr)
		snapshot.activeTextureUnit = GetInteger (GL_ACTIVE_TEXTURE) - GL_TEXTURE0;

	snapshot.matrixMode = GetInteger (GL_MATRIX_MODE);
	// glGetFloatv sur les trois matrices : aucune n'exige de changer le mode
	// courant, donc le releve ne touche pas ce qu'il mesure. GL_TEXTURE_MATRIX
	// est lue AVANT le parcours des unites, donc sur l'unite active relevee
	// ci-dessus.
	glGetFloatv (GL_PROJECTION_MATRIX, snapshot.projectionMatrix);
	glGetFloatv (GL_TEXTURE_MATRIX, snapshot.textureMatrix);
	glGetFloatv (GL_MODELVIEW_MATRIX, snapshot.modelviewMatrix);

	snapshot.textureUnitCount = g_textureUnitCount;
	if (glad_glActiveTexture != nullptr)
	{
		for (int unit = 0; unit < snapshot.textureUnitCount; ++unit)
		{
			glActiveTexture ((GLenum)(GL_TEXTURE0 + unit));
			snapshot.textureBinding2D[unit] = GetInteger (GL_TEXTURE_BINDING_2D);
		}
		// L'instrument remet ce qu'il a deplace : sans cette ligne, le releve
		// modifierait l'etat qu'il pretend mesurer, et le second releve
		// differerait du premier POUR CETTE RAISON.
		glActiveTexture ((GLenum)(GL_TEXTURE0 + snapshot.activeTextureUnit));
	}
	else
	{
		snapshot.textureUnitCount    = 1;
		snapshot.textureBinding2D[0] = GetInteger (GL_TEXTURE_BINDING_2D);
	}

	snapshot.blend            = IsEnabled (GL_BLEND);
	snapshot.depthTest        = IsEnabled (GL_DEPTH_TEST);
	snapshot.cullFace         = IsEnabled (GL_CULL_FACE);
	snapshot.normalize        = IsEnabled (GL_NORMALIZE);
	snapshot.lighting         = IsEnabled (GL_LIGHTING);
	snapshot.scissorTest      = IsEnabled (GL_SCISSOR_TEST);
	snapshot.stencilTest      = IsEnabled (GL_STENCIL_TEST);
	snapshot.stencilWriteMask = (unsigned int)GetInteger (GL_STENCIL_WRITEMASK);

	GLint viewport[4] = { 0, 0, 0, 0 };
	glGetIntegerv (GL_VIEWPORT, viewport);
	for (int i = 0; i < 4; ++i)
		snapshot.viewport[i] = (int)viewport[i];

	if (g_glVersionMajor >= 3)
	{
		snapshot.framebufferSrgb    = IsEnabled (GL_FRAMEBUFFER_SRGB);
		snapshot.framebufferBinding = GetInteger (GL_FRAMEBUFFER_BINDING);
	}

	return snapshot;
}

std::vector<GlStateEntry> GlStateSnapshot::Entries (const GlStateSnapshot* other) const
{
	std::vector<GlStateEntry> entries;
	entries.reserve ((size_t)(16 + textureUnitCount));

	const auto add = [&entries] (std::string name, std::string value, bool identical) {
		GlStateEntry entry;
		entry.name      = std::move (name);
		entry.value     = std::move (value);
		entry.identical = identical;
		entries.push_back (std::move (entry));
	};

	add ("GL_CURRENT_PROGRAM", FormatInteger (currentProgram),
	     other == nullptr || currentProgram == other->currentProgram);

	add ("GL_MATRIX_MODE", FormatMatrixMode (matrixMode),
	     other == nullptr || matrixMode == other->matrixMode);
	add ("GL_PROJECTION_MATRIX", FormatMatrix (projectionMatrix),
	     other == nullptr || SameBits (projectionMatrix, other->projectionMatrix));
	add ("GL_TEXTURE_MATRIX", FormatMatrix (textureMatrix),
	     other == nullptr || SameBits (textureMatrix, other->textureMatrix));
	add ("GL_MODELVIEW_MATRIX", FormatMatrix (modelviewMatrix),
	     other == nullptr || SameBits (modelviewMatrix, other->modelviewMatrix));

	add ("GL_ACTIVE_TEXTURE", FormatInteger (activeTextureUnit),
	     other == nullptr || activeTextureUnit == other->activeTextureUnit);

	// Les deux releves viennent du meme contexte, donc du meme nombre d'unites.
	// Le minimum est pris malgre tout : comparer au-dela de ce qui a ete lu
	// rendrait une divergence fabriquee par l'instrument.
	const int units =
	    other == nullptr ? textureUnitCount : std::min (textureUnitCount, other->textureUnitCount);
	for (int unit = 0; unit < units; ++unit)
	{
		add ("GL_TEXTURE_BINDING_2D[" + std::to_string (unit) + "]",
		     FormatInteger (textureBinding2D[unit]),
		     other == nullptr || textureBinding2D[unit] == other->textureBinding2D[unit]);
	}

	add ("GL_BLEND", FormatFlag (blend), other == nullptr || blend == other->blend);
	add ("GL_DEPTH_TEST", FormatFlag (depthTest),
	     other == nullptr || depthTest == other->depthTest);
	add ("GL_CULL_FACE", FormatFlag (cullFace), other == nullptr || cullFace == other->cullFace);
	add ("GL_NORMALIZE", FormatFlag (normalize),
	     other == nullptr || normalize == other->normalize);
	add ("GL_LIGHTING", FormatFlag (lighting), other == nullptr || lighting == other->lighting);
	add ("GL_SCISSOR_TEST", FormatFlag (scissorTest),
	     other == nullptr || scissorTest == other->scissorTest);
	add ("GL_STENCIL_TEST", FormatFlag (stencilTest),
	     other == nullptr || stencilTest == other->stencilTest);
	add ("GL_STENCIL_WRITEMASK", FormatMask (stencilWriteMask),
	     other == nullptr || stencilWriteMask == other->stencilWriteMask);

	bool sameViewport = true;
	for (int i = 0; other != nullptr && i < 4; ++i)
		sameViewport = sameViewport && viewport[i] == other->viewport[i];
	add ("GL_VIEWPORT", FormatViewport (viewport), sameViewport);

	add ("GL_FRAMEBUFFER_SRGB", FormatFlag (framebufferSrgb),
	     other == nullptr || framebufferSrgb == other->framebufferSrgb);
	add ("GL_FRAMEBUFFER_BINDING", FormatInteger (framebufferBinding),
	     other == nullptr || framebufferBinding == other->framebufferBinding);

	return entries;
}

std::string GlStateSnapshot::Describe () const
{
	std::string text;
	for (const GlStateEntry& entry : Entries ())
	{
		text += "  ";
		text += entry.name;
		text += " = ";
		text += entry.value;
		text += "\n";
	}
	return text;
}

int GlStateSnapshot::StateCount () const
{
	return (int)Entries ().size ();
}

std::vector<std::string> GlStateSnapshot::DifferingStates (const GlStateSnapshot& other) const
{
	std::vector<std::string> names;
	for (const GlStateEntry& entry : Entries (&other))
	{
		if (!entry.identical)
			names.push_back (entry.name);
	}
	return names;
}

GlStateFence::GlStateFence (GlStatePhaseAudit* audit) : m_audit (audit)
{
	VerifyCompatibilityProfileOnce ();

	if (m_audit != nullptr)
	{
		// Releve pris AVANT tout ce que la cloture pose, glUseProgram(0)
		// compris : sans quoi le programme courant vaudrait 0 des deux cotes.
		m_audit->before   = GlStateSnapshot::Capture ();
		m_audit->measured = false;
	}

	glPushAttrib (GL_ALL_ATTRIB_BITS);
	glPushClientAttrib (GL_CLIENT_ALL_ATTRIB_BITS);

	// LES MATRICES NE SONT PAS DANS LA PILE D'ATTRIBUTS. GL_TRANSFORM_BIT sauve
	// le MODE de matrice courant, pas les matrices elles-memes.
	//
	// La pile de texture est empilee comme les deux autres : RmlUi_Renderer_GL2
	// ecrit la matrice de texture a chaque image (BeginFrame : glMatrixMode
	// (GL_TEXTURE) ; glLoadIdentity ()).
	glMatrixMode (GL_PROJECTION);
	glPushMatrix ();
	glMatrixMode (GL_TEXTURE);
	glPushMatrix ();
	glMatrixMode (GL_MODELVIEW);
	glPushMatrix ();

	if (glad_glUseProgram != nullptr)
		glUseProgram (0);
}

GlStateFence::~GlStateFence ()
{
	// ⚠ LU AVANT LE glUseProgram(0) CI-DESSOUS, ET C'EST TOUTE LA RAISON D'ETRE
	// DE CE POINT DE MESURE : apres, la valeur vaut 0 par construction et l'etat
	// ne peut plus diverger de son releve d'entree, quoi qu'ait fait le moteur.
	const int programLeftBound =
	    glad_glUseProgram != nullptr ? GetInteger (GL_CURRENT_PROGRAM) : 0;

	if (glad_glUseProgram != nullptr)
		glUseProgram (0);

	// Depilement dans l'ordre INVERSE de l'empilement.
	glMatrixMode (GL_MODELVIEW);
	glPopMatrix ();
	glMatrixMode (GL_TEXTURE);
	glPopMatrix ();
	glMatrixMode (GL_PROJECTION);
	glPopMatrix ();

	glPopClientAttrib ();
	glPopAttrib ();

	if (m_audit != nullptr)
	{
		// Tout le reste est lu APRES le depilement : c'est le depilement que le
		// contrat mesure.
		m_audit->after                = GlStateSnapshot::Capture ();
		m_audit->after.currentProgram = programLeftBound;
		m_audit->measured             = true;
	}
}

} // namespace app
} // namespace peles
