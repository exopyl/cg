#include "extrude_profiled.h"

#include <memory>
#include <vector>

#include "../../../cgmesh/extrude_contours.h"
#include "../../../cgmesh/extrude_profiled.h"
#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/profile2d.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "shape.extrude.profiled";
		d.inputs.push_back ({ "contours", Types ().extrudeContours, false });
		// EBRASEMENT et non section de barre : brancher l'autre est refuse a la
		// connexion plutot que silencieusement mal lu (cf. shapes/profile.h).
		d.inputs.push_back ({ "profil", Types ().splayProfile, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

} // namespace

ExtrudeProfiledNode::ExtrudeProfiledNode ()
{
	GetParams ().SetFloat ("depth", 0.2f);
	// Meme convention que shape.extrude : `depth` est une EPAISSEUR, le solide
	// occupe [zBottom, zBottom + depth].
	GetParams ().SetFloat ("zBottom", 0.0f);
	// 0 = decalage interieur (l'emprise nominale est preservee), 1 = dilatation
	// (l'emprise croit, la face du dessus reste nominale). Le defaut est la
	// decision D1 du dossier de faisabilite.
	GetParams ().SetInt ("direction", 0);
}

void ExtrudeProfiledNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "vanishedPieces", (double)m_vanished.load () });
	out.push_back ({ "steinerPoints", (double)m_steiner.load () });
}

const cggraph::NodeDesc &ExtrudeProfiledNode::GetDesc () const
{
	return Desc ();
}

bool ExtrudeProfiledNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                   cggraph::ValueList &out)
{
	(void)ctx;

	const std::vector<ExtrudeContour> *contours =
		in[0].Get<std::vector<ExtrudeContour>> (Types ().extrudeContours);
	if (contours == nullptr || contours->empty ())
		return false;

	const Profile2D *profile = in[1].Get<Profile2D> (Types ().splayProfile);
	if (profile == nullptr)
		return false;

	ProfiledExtrudeOptions options;
	options.zBottom = GetFloat (GetParams (), "zBottom", 0.0f);
	options.zTop = options.zBottom + GetFloat (GetParams (), "depth", 0.2f);

	int direction = GetInt (GetParams (), "direction", 0);
	if (direction < 0) direction = 0;
	if (direction > 1) direction = 1;
	options.direction = (direction == 1) ? ProfiledExtrudeOptions::Direction::Outward
	                                     : ProfiledExtrudeOptions::Direction::Inward;

	std::shared_ptr<Mesh> mesh = std::make_shared<Mesh> ();
	ProfiledExtrudeStats stats;
	// Un profil plus large que la moitie de la piece, ou plus profond qu'elle :
	// la primitive REFUSE plutot que de rendre un solide sans face superieure.
	// On relaie le refus tel quel -- il a une cause, et la masquer par une
	// extrusion droite ferait mentir le reglage.
	if (!extrudeProfiledContours (*contours, *profile, options, *mesh, &stats))
	{
		m_vanished.store (0);
		m_steiner.store (0);
		return false;
	}

	m_vanished.store ((unsigned int)stats.vanishedPieces);
	m_steiner.store ((unsigned int)stats.steinerPoints);

	out[0] = cggraph::Value::Make (Types ().mesh, mesh);
	return true;
}

} // namespace cggraph_nodes
