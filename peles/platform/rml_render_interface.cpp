#include "rml_render_interface.h"

// ⚠ SEULE UNITE DE TRADUCTION DU PROJET QUI ATTEINT LE RENDERER GL2. Son .cpp
// tire l'en-tete GL du systeme ; src/cgre/gl_wrapper.h n'entre jamais ici, et
// le repertoire Backends/ de RmlUi est un chemin d'inclusion PRIVE de
// peles_platform.
#include "RmlUi_Renderer_GL2.h"

namespace peles
{
namespace platform
{

RenderInterfaceGL2::RenderInterfaceGL2 () : m_impl (new RenderInterface_GL2) {}

// Definie hors ligne parce que RenderInterface_GL2 est INCOMPLET dans
// l'en-tete : le destructeur de unique_ptr doit voir le type complet.
RenderInterfaceGL2::~RenderInterfaceGL2 () = default;

Rml::RenderInterface* RenderInterfaceGL2::Interface () const
{
	return m_impl.get ();
}

void RenderInterfaceGL2::SetViewport (int width, int height)
{
	m_impl->SetViewport (width, height);
}

void RenderInterfaceGL2::BeginFrame ()
{
	m_impl->BeginFrame ();
}

void RenderInterfaceGL2::EndFrame ()
{
	m_impl->EndFrame ();
}

} // namespace platform
} // namespace peles
