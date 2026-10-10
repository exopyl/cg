#include "viewport_rect.h"

#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Box.h>
#include <RmlUi/Core/Types.h>

#include <cmath>

namespace peles
{
namespace ui
{

namespace
{

// ARRONDI PAR LES BORDS, pas par l'origine et la taille. Arrondir separement
// x et width fait varier le bord droit de deux pixels la ou les bords eux-memes
// n'en bougent que d'un : c'est ce qui laisse une couture entre deux zones
// adjacentes.
int RoundEdge (float v)
{
	return (int)std::lround ((double)v);
}

} // namespace

GlViewport ToGlViewport (const ViewportRect& rect, int framebufferHeight)
{
	GlViewport gl;
	gl.x      = rect.x;
	gl.width  = rect.width;
	gl.height = rect.height;
	// RmlUi compte depuis le haut, GL depuis le bas. L'origine GL est donc le
	// bord INFERIEUR du rectangle, soit hauteur du cadre moins son bord bas.
	gl.y = framebufferHeight - (rect.y + rect.height);
	return gl;
}

void WindowToView (const ViewportRect& rect, float windowX, float windowY, float& viewX,
                   float& viewY)
{
	viewX = windowX - (float)rect.x;
	viewY = windowY - (float)rect.y;
}

ViewportRectStatus ReadViewportRect (Rml::Element* root, const char* id, ViewportRect& rect)
{
	// Un document absent est traite comme un element absent : dans les deux cas
	// il n'existe aucune geometrie a lire, et la cause est la configuration.
	if (root == nullptr || id == nullptr)
		return ViewportRectStatus::ElementMissing;

	Rml::Element* element = root->GetElementById (id);
	if (element == nullptr)
		return ViewportRectStatus::ElementMissing;

	// BoxArea::Border et non Content : c'est la surface que l'element occupe
	// reellement a l'ecran, bordures comprises. Content exclurait les bordures
	// et la vue 3D se retrouverait decalee de leur epaisseur.
	const Rml::Vector2f offset = element->GetAbsoluteOffset (Rml::BoxArea::Border);
	const Rml::Vector2f size   = element->GetBox ().GetSize (Rml::BoxArea::Border);

	const int left   = RoundEdge (offset.x);
	const int top    = RoundEdge (offset.y);
	const int right  = RoundEdge (offset.x + size.x);
	const int bottom = RoundEdge (offset.y + size.y);

	if (right <= left || bottom <= top)
		return ViewportRectStatus::DegenerateSize;

	rect.x      = left;
	rect.y      = top;
	rect.width  = right - left;
	rect.height = bottom - top;
	return ViewportRectStatus::Ok;
}

} // namespace ui
} // namespace peles
