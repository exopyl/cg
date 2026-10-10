#pragma once

#include <memory>

class RenderInterface_GL2;

namespace Rml
{
class RenderInterface;
}

namespace peles
{
namespace platform
{

// Detenteur du renderer GL2 de RmlUi.
//
// ⚠ POURQUOI UNE ENVELOPPE PLUTOT QU'UN USAGE DIRECT. RmlUi_Renderer_GL2.cpp
// inclut <gl/Gl.h> sous Windows et <GL/gl.h> ailleurs. Aucune unite de
// traduction ne doit inclure a la fois cet en-tete et src/cgre/gl_wrapper.h,
// qui redefinit chaque entree GL par macro. L'enveloppe tient la frontiere :
// le type concret est INCOMPLET dans cet en-tete, et le repertoire Backends/ de
// RmlUi est un chemin d'inclusion PRIVE de peles_platform. Une unite exterieure
// ne peut donc pas atteindre l'en-tete GL, meme par accident.
//
// GL2 et non GL3 : c'est un renderer de PIPELINE FIXE, il vit exactement dans le
// profil de compatibilite que cgre exige, et il n'a besoin que d'entrees GL 1.x
// -- exportees statiquement par opengl32.lib / libGL. Il n'amene donc pas un
// second chargeur GL complet dans le processus.
class RenderInterfaceGL2
{
public:
	RenderInterfaceGL2 ();
	~RenderInterfaceGL2 ();

	RenderInterfaceGL2 (const RenderInterfaceGL2&)            = delete;
	RenderInterfaceGL2& operator= (const RenderInterfaceGL2&) = delete;

	// Pointeur NON PROPRIETAIRE, a remettre a Rml::SetRenderInterface.
	Rml::RenderInterface* Interface () const;

	// Taille du TAMPON DE DESSIN, pas de la fenetre.
	void SetViewport (int width, int height);

	void BeginFrame ();
	void EndFrame ();

private:
	std::unique_ptr<RenderInterface_GL2> m_impl;
};

} // namespace platform
} // namespace peles
