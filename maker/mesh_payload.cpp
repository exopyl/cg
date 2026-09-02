#include "mesh_payload.h"

#ifdef __EMSCRIPTEN__

#include <cstddef>

#include "../src/cgimg/image.h"
#include "material.h"
#include "mesh.h"

namespace maker
{

void MeshPayloadBuffers::Clear ()
{
	positions.clear ();
	normals.clear ();
	uvs.clear ();
	colors.clear ();
	indices.clear ();
}

namespace
{

// Un materiau du maillage -> l'objet JS qui le decrit.
emscripten::val DescribeMaterial (Material *mat)
{
	using namespace emscripten;
	val m = val::object ();

	if (MaterialTexture *tex = dynamic_cast<MaterialTexture *> (mat)) {
		Img *img = tex->GetImage ();
		if (img != nullptr && img->width () > 0 && img->height () > 0) {
			const unsigned int w = img->width (), h = img->height ();
			// RGBA a plat. get_pixel plutot qu'une lecture directe de data() :
			// la conversion vaut alors quel que soit le format de l'entree --
			// palettisee, en niveaux de gris ou RGB.
			std::vector<unsigned char> rgba ((std::size_t)w * h * 4, 0);
			for (unsigned int y = 0; y < h; ++y)
				for (unsigned int x = 0; x < w; ++x) {
					unsigned char r = 0, g = 0, b = 0, a = 255;
					img->get_pixel (x, y, &r, &g, &b, &a);
					const std::size_t k = ((std::size_t)y * w + x) * 4;
					rgba[k + 0] = r; rgba[k + 1] = g; rgba[k + 2] = b; rgba[k + 3] = a;
				}
			m.set ("kind", std::string ("texture"));
			m.set ("width", (int)w);
			m.set ("height", (int)h);
			// COPIE (slice) et non une vue : une texture survit a l'appel qui l'a
			// produite, alors que la vue pointe un tampon local deja detruit.
			m.set ("rgba", val (typed_memory_view (rgba.size (), rgba.data ()))
			                   .call<val> ("slice"));
			return m;
		}
		m.set ("kind", std::string ("none"));
		return m;
	}

	if (MaterialColor *col = dynamic_cast<MaterialColor *> (mat)) {
		m.set ("kind", std::string ("color"));
		m.set ("r", col->GetFloatRed ());
		m.set ("g", col->GetFloatGreen ());
		m.set ("b", col->GetFloatBlue ());
		return m;
	}

	m.set ("kind", std::string ("none"));
	return m;
}

} // namespace

emscripten::val BuildMeshPayload (const Mesh *mesh, MeshPayloadBuffers &bufs)
{
	using namespace emscripten;

	bufs.Clear ();
	val groups = val::array ();
	val materials = val::array ();

	if (mesh != nullptr) {
		Mesh::PolygonRenderData rd = mesh->BuildPolygonRenderData (false);
		bufs.positions.swap (rd.positions);
		bufs.normals.swap (rd.normals);
		bufs.uvs.swap (rd.texCoords);
		bufs.indices.swap (rd.indices);

		// COULEURS PAR SOMMET, mais seulement les VRAIES. Mesh::InitVertices
		// remplit m_vertexColors de gris 0,5 (mesh.cpp:174) : tout maillage en
		// porte donc, y compris ceux qui n'en ont jamais recu. Les verser telles
		// quelles ferait basculer le JS en mode « le maillage a ses couleurs » sur
		// un simple texte extrude, ce qui neutraliserait son selecteur de couleur
		// et le figerait sur ce gris. On ne les transmet que si l'une d'elles
		// s'ecarte du defaut -- ce qui est le cas apres mesh.color.map, seul noeud
		// du catalogue qui les peint.
		bool painted = false;
		for (std::size_t i = 0; i < rd.colors.size () && !painted; ++i)
			if (rd.colors[i] != 0.5f) painted = true;
		if (painted) bufs.colors.swap (rd.colors);

		// Une entree de `materials` par PLAGE, et non par materiau du maillage :
		// c'est l'indice dans CE tableau que le groupe designe. Les faire coincider
		// eviterait une indirection, mais un materiau ne portant aucune face
		// laisserait alors un trou.
		unsigned int slot = 0;
		for (const Mesh::MaterialRange &r : rd.materialRanges) {
			val g = val::object ();
			g.set ("start", (unsigned int)r.offset);
			g.set ("count", (unsigned int)r.count);
			g.set ("material", slot);
			groups.call<void> ("push", g);

			Material *mat = (r.materialId < mesh->GetNMaterials ())
				? const_cast<Mesh *> (mesh)->GetMaterial (r.materialId)
				: nullptr;
			materials.call<void> ("push", DescribeMaterial (mat));
			++slot;
		}
	}

	val out = val::object ();
	out.set ("positions", val (typed_memory_view (bufs.positions.size (), bufs.positions.data ())));
	out.set ("normals",   val (typed_memory_view (bufs.normals.size (),   bufs.normals.data ())));
	out.set ("uvs",       val (typed_memory_view (bufs.uvs.size (),       bufs.uvs.data ())));
	out.set ("colors",    val (typed_memory_view (bufs.colors.size (),    bufs.colors.data ())));
	out.set ("indices",   val (typed_memory_view (bufs.indices.size (),   bufs.indices.data ())));
	out.set ("groups",    groups);
	out.set ("materials", materials);
	out.set ("nv", (int)(bufs.positions.size () / 3));
	out.set ("nf", (int)(bufs.indices.size () / 3));
	return out;
}

} // namespace maker

#endif // __EMSCRIPTEN__
