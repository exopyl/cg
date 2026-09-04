#include "mounts.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <vector>

#include "../../../cgmesh/bounding_box.h"
#include "../../../cgmesh/contour_ops.h"
#include "../../../cgmesh/countersink.h"
#include "../../../cgmesh/extrude_contours.h"
#include "../../../cgmesh/mesh.h"
#include "../../../cgmesh/mesh_slab.h"
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
		d.typeName = "mesh.mounts";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.outputs.push_back ({ "maillage", Types ().mesh, false });
		return d;
	}();
	return desc;
}

ExtrudeContour asContour (std::vector<Vector2f> pts)
{
	ExtrudeContour c;
	c.pts = std::move (pts);
	return c;
}

} // namespace

MountsNode::MountsNode ()
{
	// Hauteur de la ligne de fixation, en FRACTION de la piece : 0 au bas, 1 au
	// sommet, 0,5 a mi-hauteur. Une fraction et non des millimetres, pour qu'un
	// changement de taille du texte ne deplace pas la fixation.
	GetParams ().SetFloat ("lineHeight", 0.5f);
	// Hauteur du bandeau lui-meme.
	GetParams ().SetFloat ("bandWidth", 6.0f);
	GetParams ().SetFloat ("earDiameter", 12.0f);
	// 4,5 mm : le passage courant d'une vis de 4. Le jeu est DANS ce chiffre --
	// un trou imprime sort sous-cote, l'extrusion mordant vers l'interieur.
	GetParams ().SetFloat ("holeDiameter", 4.5f);
	// De combien le bord EXTERIEUR de l'oreille depasse l'emprise.
	GetParams ().SetFloat ("overhang", 6.0f);
	GetParams ().SetFloat ("thickness", 2.0f);
	// FRAISURE : diametre de la BOUCHE, sur la face avant. Zero = pas de
	// fraisure, et c'est le defaut -- une tete cylindrique se pose a plat, une
	// tete fraisee ne s'affleure que si on l'a prevu.
	GetParams ().SetFloat ("countersinkDiameter", 0.0f);
	// Angle au SOMMET du cone, en degres. 90 est la norme metrique ; 82 est la
	// norme pouce, et une tete de 82 dans un cone de 90 ne porte que sur son
	// arete.
	GetParams ().SetFloat ("countersinkAngle", 90.0f);
}

const cggraph::NodeDesc &MountsNode::GetDesc () const
{
	return Desc ();
}

void MountsNode::PublishStats (std::vector<cggraph::NodeStat> &out) const
{
	out.push_back ({ "holeSpacing", (double)m_holeSpacing.load () });
}

bool MountsNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                          cggraph::ValueList &out)
{
	(void)ctx;

	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	const float bandWidth = GetFloat (GetParams (), "bandWidth", 6.0f);
	const float earR = 0.5f * GetFloat (GetParams (), "earDiameter", 12.0f);
	const float holeR = 0.5f * GetFloat (GetParams (), "holeDiameter", 4.5f);
	const float overhang = GetFloat (GetParams (), "overhang", 6.0f);
	const float thickness = GetFloat (GetParams (), "thickness", 2.0f);
	// Une oreille plus petite que son trou n'est pas une oreille, et un bandeau
	// sans hauteur ne relie rien : on REFUSE plutot que de rendre une piece
	// trouee de part en part sans le dire.
	if (bandWidth <= 0.f || earR <= 0.f || thickness <= 0.f || holeR <= 0.f)
		return false;
	if (holeR >= earR)
		return false;

	// FRAISURE. Zero -- le defaut -- vaut « aucune », et la piece est alors
	// exactement celle d'avant : plaque percee au diametre du trou, sans bague.
	const float mouthR = 0.5f * GetFloat (GetParams (), "countersinkDiameter", 0.0f);
	const float csAngle = GetFloat (GetParams (), "countersinkAngle", 90.0f);
	const bool countersunk = (mouthR > 0.f);
	float coneDepth = 0.f;
	if (countersunk)
	{
		// Une bouche plus etroite que le trou n'est pas une fraisure, et une
		// bouche plus large que l'oreille mange l'oreille : dans les deux cas on
		// REFUSE, plutot que de rendre une piece dont le defaut ne se verrait
		// qu'une fois la vis en main.
		if (mouthR <= holeR) return false;
		if (mouthR >= earR) return false;
		if (csAngle < 30.f || csAngle > 170.f) return false;

		// Profondeur DEDUITE du diametre et de l'angle, jamais reglee a part :
		// les trois ne sont pas independants, et laisser regler les trois serait
		// offrir de decrire un cone qui n'existe pas.
		const float PI = 3.14159265358979323846f;
		const float halfAngle = 0.5f * csAngle * PI / 180.f;
		const float tangent = std::tan (halfAngle);
		if (tangent <= 0.f) return false;
		coneDepth = (mouthR - holeR) / tangent;
		// Traversante, elle ne laisse aucune portee cylindrique pour guider la
		// vis -- et au-dela, elle ouvre un trou plus large que prevu par-dessous.
		if (coneDepth >= thickness) return false;
	}

	// La boite est un CACHE sans peremption (cf. mesh.h) : sur un maillage qui
	// n'y est jamais passe, elle est vide. On la recalcule donc, sur la copie que
	// l'on possede.
	std::shared_ptr<Mesh> result = std::make_shared<Mesh> (*input);
	result->computebbox ();
	const BoundingBox &box = result->bbox ();
	if (box.IsEmpty ())
		return false;

	float lineFraction = GetFloat (GetParams (), "lineHeight", 0.5f);
	if (lineFraction < 0.f) lineFraction = 0.f;
	if (lineFraction > 1.f) lineFraction = 1.f;

	const float y0 = box.GetMinY (), y1 = box.GetMaxY ();
	const float lineY = y0 + lineFraction * (y1 - y0);

	// ⚠ L'ETENDUE EST MESUREE DANS LA TRANCHE DU BANDEAU, pas sur la boite
	// englobante -- et la difference n'est pas cosmetique. Un L va jusqu'a la
	// pointe de son pied en bas, et pas plus loin que son montant a mi-hauteur :
	// une fixation posee a mi-hauteur d'apres l'emprise TOTALE deborderait de
	// plusieurs millimetres dans le vide, oreille comprise.
	//
	// La tranche est celle que le bandeau occupe, ni plus ni moins. C'est ce qui
	// donne la propriete que l'emprise totale ne donnait pas : les deux bouts du
	// bandeau tombent sur de la MATIERE, puisqu'ils sont pris sur elle.
	float x0 = 0.f, x1 = 0.f;
	if (!meshSlabExtentX (*result, lineY - 0.5f * bandWidth, lineY + 0.5f * bandWidth,
	                      x0, x1))
		return false;   // rien a cette hauteur : le bandeau flotterait

	// Les centres d'oreille sont sur la MEME ligne -- c'est toute l'affaire -- et
	// le bandeau court de l'un a l'autre. En le faisant aller jusqu'aux centres,
	// la soudure oreille/bandeau est acquise par construction, quel que soit le
	// debord demande.
	const float leftCx = x0 - overhang + earR;
	const float rightCx = x1 + overhang - earR;
	if (rightCx <= leftCx)
		return false;   // debord tel que les deux oreilles se croisent

	m_holeSpacing.store (rightCx - leftCx);

	std::vector<ExtrudeContour> solid;
	solid.push_back (asContour (roundedRectContour (leftCx, lineY - 0.5f * bandWidth,
	                                                rightCx, lineY + 0.5f * bandWidth, 0.f)));
	solid.push_back (asContour (circleContour (leftCx, lineY, earR)));
	solid.push_back (asContour (circleContour (rightCx, lineY, earR)));

	// UNION d'abord, PERCAGE ensuite, et l'ordre n'est pas indifferent : le
	// bandeau va jusqu'aux centres, donc il recouvre les trous. Les poser comme
	// contours inverses dans la meme region les reboucherait a moitie -- la ou le
	// bandeau passe, le compte NonZero redeviendrait non nul.
	//
	// ⚠ AVEC FRAISURE, la plaque est percee au diametre de la BOUCHE, de part en
	// part -- donc trop large. Ce sont les bagues, plus bas, qui remettent la
	// matiere qu'il ne fallait pas retirer. On ne creuse pas : le depot n'a aucun
	// booleen 3D, et la piece fraisee se construit a l'envers (cf.
	// cgmesh/countersink.h).
	const float pierceR = countersunk ? mouthR : holeR;
	std::vector<ExtrudeContour> holes;
	holes.push_back (asContour (circleContour (leftCx, lineY, pierceR)));
	holes.push_back (asContour (circleContour (rightCx, lineY, pierceR)));

	const std::vector<ExtrudeContour> fixture =
		differenceContours (unionContours (solid, {}), holes);
	if (fixture.empty ())
		return false;

	// Du BAS de la piece vers le haut : c'est la que le socle et les lettres
	// commencent tous les deux, donc la que le bandeau mord.
	ExtrudeAppendOptions options;
	options.zBottom = box.GetMinZ ();
	options.zTop = box.GetMinZ () + thickness;
	options.winding = ExtrudeWinding::NonZero;
	options.normalizeOrientation = false;

	ExtrudedMeshBuilder builder;
	builder.Append (fixture, options);
	if (builder.Empty ())
		return false;

	std::unique_ptr<Mesh> mounts (builder.Build ());
	if (mounts == nullptr)
		return false;

	// CONCATENATION, comme le socle : deux coques fermees qui s'interpenetrent,
	// que le slicer unifie. Aucun booleen 3D n'entre ici.
	result->Append (mounts.get ());

	if (countersunk)
	{
		CountersinkCollarOptions collar;
		collar.holeRadius = holeR;
		collar.mouthRadius = mouthR;
		collar.zBottom = options.zBottom;
		collar.zTop = options.zTop;
		collar.coneDepth = coneDepth;
		collar.cy = lineY;
		for (const float cx : { leftCx, rightCx })
		{
			collar.cx = cx;
			std::unique_ptr<Mesh> ring (countersinkCollar (collar));
			if (ring == nullptr)
				return false;
			result->Append (ring.get ());
		}
	}

	result->IncrementRevision ();

	out[0] = cggraph::Value::Make (Types ().mesh, result);
	return true;
}

} // namespace cggraph_nodes
