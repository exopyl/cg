#include "profile.h"

#include <memory>

#include "../../../cgmesh/profile2d.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

cggraph::NodeDesc BuildDesc (const char *typeName, const cggraph::TypeDesc *type)
{
	cggraph::NodeDesc d;
	d.typeName = typeName;
	// Aucune entree : un profil est engendre par ses seuls parametres.
	d.outputs.push_back ({ "profil", type, false });
	return d;
}

const cggraph::NodeDesc &ChamferDesc ()
{
	static const cggraph::NodeDesc d = BuildDesc ("profile.chamfer", Types ().splayProfile);
	return d;
}

const cggraph::NodeDesc &CavettoDesc ()
{
	static const cggraph::NodeDesc d = BuildDesc ("profile.cavetto", Types ().splayProfile);
	return d;
}

const cggraph::NodeDesc &RollDesc ()
{
	static const cggraph::NodeDesc d = BuildDesc ("profile.bar.roll", Types ().barProfile);
	return d;
}

const cggraph::NodeDesc &KeelDesc ()
{
	static const cggraph::NodeDesc d = BuildDesc ("profile.bar.keel", Types ().barProfile);
	return d;
}

const cggraph::NodeDesc &OgeeDesc ()
{
	static const cggraph::NodeDesc d = BuildDesc ("profile.bar.ogee", Types ().barProfile);
	return d;
}

bool Publish (const Profile2D &profile, const cggraph::TypeDesc *type, cggraph::ValueList &out)
{
	// Deux points au moins : en dessous, ce n'est pas un profil, et le
	// consommateur le refuserait plus loin, sans pouvoir dire d'ou il vient.
	if (profile.points.size () < 2)
		return false;
	out[0] = cggraph::Value::Make (type, std::make_shared<Profile2D> (profile));
	return true;
}

} // namespace

// --- chanfrein --------------------------------------------------------------

ChamferProfileNode::ChamferProfileNode ()
{
	// 6 et 10 : ce que la baie gothique derivait de ses offsets PAR DEFAUT pour
	// sa position « Chamfer » -- chamW = min (0,6 x 10 ; 0,45 x 10 + 2) = 6, et
	// chamD = min (0,5 x 20 ; 3 x 6) = 10. Un profil neuf branche sur une baie
	// neuve rend donc ce que rendait le menu.
	GetParams ().SetFloat ("width", 6.0f);
	GetParams ().SetFloat ("depth", 10.0f);
}

const cggraph::NodeDesc &ChamferProfileNode::GetDesc () const { return ChamferDesc (); }

bool ChamferProfileNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                  cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;
	return Publish (chamferSplayProfile (GetFloat (GetParams (), "width", 6.0f),
	                                     GetFloat (GetParams (), "depth", 10.0f)),
	                Types ().splayProfile, out);
}

// --- cavet ------------------------------------------------------------------

CavettoProfileNode::CavettoProfileNode ()
{
	GetParams ().SetFloat ("width", 6.0f);
	GetParams ().SetFloat ("depth", 10.0f);
	// Nombre d'echantillons du quart de cercle. Semantique : il change la
	// geometrie produite, donc il invalide le calcul comme n'importe quel autre.
	GetParams ().SetInt ("segments", 6);
}

const cggraph::NodeDesc &CavettoProfileNode::GetDesc () const { return CavettoDesc (); }

bool CavettoProfileNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                  cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;
	int segments = GetInt (GetParams (), "segments", 6);
	if (segments < 1) segments = 1;
	if (segments > 64) segments = 64;
	return Publish (cavettoSplayProfile (GetFloat (GetParams (), "width", 6.0f),
	                                     GetFloat (GetParams (), "depth", 10.0f), segments),
	                Types ().splayProfile, out);
}

// --- barres -----------------------------------------------------------------

RollBarProfileNode::RollBarProfileNode ()
{
	// 5 : ce que la baie derivait de ses offsets par defaut, rb = min (0,5 x 10 ;
	// 0,45 x 10 + 1,5) = 5.
	GetParams ().SetFloat ("radius", 5.0f);
	GetParams ().SetInt ("segments", 12);
}

const cggraph::NodeDesc &RollBarProfileNode::GetDesc () const { return RollDesc (); }

bool RollBarProfileNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                  cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;
	int segments = GetInt (GetParams (), "segments", 12);
	if (segments < 2) segments = 2;
	if (segments > 64) segments = 64;
	return Publish (rollBarProfile (GetFloat (GetParams (), "radius", 5.0f), segments),
	                Types ().barProfile, out);
}

KeelBarProfileNode::KeelBarProfileNode ()
{
	GetParams ().SetFloat ("radius", 5.0f);
}

const cggraph::NodeDesc &KeelBarProfileNode::GetDesc () const { return KeelDesc (); }

bool KeelBarProfileNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                  cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;
	return Publish (keelBarProfile (GetFloat (GetParams (), "radius", 5.0f)),
	                Types ().barProfile, out);
}

OgeeBarProfileNode::OgeeBarProfileNode ()
{
	GetParams ().SetFloat ("radius", 5.0f);
	GetParams ().SetInt ("segments", 6);
}

const cggraph::NodeDesc &OgeeBarProfileNode::GetDesc () const { return OgeeDesc (); }

bool OgeeBarProfileNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                                  cggraph::ValueList &out)
{
	(void)ctx;
	(void)in;
	int segments = GetInt (GetParams (), "segments", 6);
	if (segments < 1) segments = 1;
	if (segments > 64) segments = 64;
	return Publish (ogeeBarProfile (GetFloat (GetParams (), "radius", 5.0f), segments),
	                Types ().barProfile, out);
}

} // namespace cggraph_nodes
