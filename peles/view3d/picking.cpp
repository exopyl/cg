#include "picking.h"

// PAS DE PREFIXE view3d/ ICI : la racine de peles n'est PAS sur le chemin
// d'inclusion de cette cible. La forme non qualifiee resout vers view3d/camera.h
// par la regle « repertoire du fichier incluant d'abord », avant le camera.h de
// cgmesh.
#include "camera.h"
#include "document.h"

#include "vmodels.h"

#include <algorithm>
#include <cmath>
#include <memory>

namespace peles
{
namespace view3d
{

namespace
{

// REPERE VUE -> NDC, ECRIT UNE SEULE FOIS.
//
// Origine en HAUT A GAUCHE et y vers le BAS -- la convention des evenements
// souris, et non celle d'OpenGL, d'ou l'inversion sur y.
//
// Les bornes sont les BORDS du rectangle : le pixel 0 vaut -1 en NDC, le pixel
// `width` vaut +1. Un decalage d'un demi-pixel casserait l'aller-retour avec la
// projection.
void NdcFromPixel (float pixelX, float pixelY, int width, int height, float* ndcX, float* ndcY)
{
	*ndcX = 2.f * pixelX / (float)width - 1.f;
	*ndcY = 1.f - 2.f * pixelY / (float)height;
}

// Intersection rayon (o + t.d) / boite alignee sur les axes, par la methode des
// tranches. Rend true et la distance d'ENTREE (0 si l'origine est dans la
// boite) quand l'intersection est devant l'origine.
bool RayAabb (const float o[3], const float d[3], const float mn[3], const float mx[3],
              float& tHit)
{
	float t0 = -1e30f;
	float t1 = 1e30f;
	for (int a = 0; a < 3; ++a)
	{
		if (std::fabs (d[a]) < 1e-12f)
		{
			// Rayon parallele a cette paire de plans : hors de la tranche, il ne
			// peut plus rien toucher.
			if (o[a] < mn[a] || o[a] > mx[a])
				return false;
			continue;
		}

		const float inverse = 1.f / d[a];
		float       ta      = (mn[a] - o[a]) * inverse;
		float       tb      = (mx[a] - o[a]) * inverse;
		if (ta > tb)
			std::swap (ta, tb);
		if (ta > t0)
			t0 = ta;
		if (tb < t1)
			t1 = tb;
		if (t0 > t1)
			return false;
	}

	if (t1 < 0.f)
		return false; // boite entierement derriere l'oeil

	tHit = t0 >= 0.f ? t0 : 0.f;
	return true;
}

} // namespace

bool ScreenToRay (const Camera& camera, float viewX, float viewY, float origin[3],
                  float direction[3])
{
	const int width  = camera.ViewWidth ();
	const int height = camera.ViewHeight ();
	if (width <= 0 || height <= 0)
		return false;

	float ndcX = 0.f;
	float ndcY = 0.f;
	NdcFromPixel (viewX, viewY, width, height, &ndcX, &ndcY);
	camera.GetPickRay (ndcX, ndcY, origin, direction);
	return true;
}

PickHit PickModel (const core::Document& document, const Camera& camera, float viewX, float viewY)
{
	PickHit hit;

	float origin[3]    = { 0.f, 0.f, 0.f };
	float direction[3] = { 0.f, 0.f, 0.f };
	if (!ScreenToRay (camera, viewX, viewY, origin, direction))
		return hit;

	::Model* best  = nullptr;
	float    bestT = 1e30f;

	for (const std::unique_ptr<::Model>& model : document.Models ())
	{
		if (!model || !model->m_visible || model->m_bbox.IsEmpty ())
			continue;

		// 1) Filtre par la boite englobante : rejet immediat si le rayon la
		//    manque. Sans lui, chaque deplacement de souris descendrait dans le
		//    BVH de tous les modeles de la scene.
		float mn[3]  = { 0.f, 0.f, 0.f };
		float mx[3]  = { 0.f, 0.f, 0.f };
		float tBox   = 0.f;
		model->m_bbox.GetMinMax (mn, mx);
		if (!RayAabb (origin, direction, mn, mx, tBox))
			continue;

		// 2) Departage par la SURFACE quand le modele en a une ; sinon -- un
		//    nuage de points n'a pas de face -- repli sur la distance d'entree
		//    dans la boite. Une boite touchee sans aucune face n'est PAS un
		//    survol : elle designerait le vide autour d'un objet mince.
		float t = tBox;
		if (model->HasSurface ())
		{
			t = model->RayNearestSurface (origin, direction);
			if (t < 0.f)
				continue;
		}

		if (t < bestT)
		{
			bestT = t;
			best  = model.get ();
		}
	}

	if (best == nullptr)
		return hit;

	hit.model = best;
	hit.t     = bestT;
	for (int i = 0; i < 3; ++i)
		hit.point[i] = origin[i] + bestT * direction[i];
	return hit;
}

PivotPick SetPivotFromPixel (Camera& camera, const core::Document& document, float viewX,
                             float viewY)
{
	PivotPick out;

	const int width  = camera.ViewWidth ();
	const int height = camera.ViewHeight ();
	if (width <= 0 || height <= 0)
	{
		out.status = PivotPickStatus::NoView;
		return out;
	}

	// Bornes INCLUSIVES : NdcFromPixel envoie 0 sur -1 et `width` sur +1, donc
	// le bord droit du rectangle est le bord du frustum et reste un pixel
	// legitime.
	if (viewX < 0.f || viewX > (float)width || viewY < 0.f || viewY > (float)height)
	{
		out.status = PivotPickStatus::OutOfViewport;
		return out; // PIVOT INCHANGE
	}

	const PickHit hit = PickModel (document, camera, viewX, viewY);
	if (hit.model == nullptr)
	{
		out.status = PivotPickStatus::NoHit;
		return out; // PIVOT INCHANGE
	}

	camera.SetPivot (hit.point);

	out.status = PivotPickStatus::Ok;
	out.model  = hit.model;
	out.t      = hit.t;
	for (int i = 0; i < 3; ++i)
		out.point[i] = hit.point[i];
	return out;
}

} // namespace view3d
} // namespace peles
