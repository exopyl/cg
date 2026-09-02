#include "value_types.h"

#include "../../cgimg/image.h"
#include "../../cgmath/font.h"
#include "../../cgmesh/extrude_contours.h"
#include "../../cgmesh/profile2d.h"
#include "../../cgmesh/mesh.h"

namespace cggraph_nodes
{

namespace
{

std::shared_ptr<void> CloneMesh (const void *value)
{
	// Copie PROFONDE et complete : Mesh est un type valeur par la regle de zero,
	// aucun membre a enumerer donc aucun a oublier.
	return std::make_shared<Mesh> (*static_cast<const Mesh *> (value));
}

std::size_t SizeOfMesh (const void *value)
{
	// MINORANT assume, mais un minorant du bon ORDRE : positions, normales par
	// sommet et par face, couleurs par sommet, UV, un coin par triangle et le
	// materiau par face. Les tenseurs, les lignes, les points et la marge de
	// l'allocateur n'y sont pas.
	//
	// ⚠ Les normales par face, les couleurs et les materiaux ne sont PAS
	// facultatifs a l'echelle du budget : sur un maillage de 2 M de triangles
	// ils pesent 42 Mio contre 46 pour le reste. Les omettre, comme c'etait le
	// cas, divisait le chiffre par pres de deux -- et le cache evinçait donc sur
	// un budget deux fois plus permissif que celui affiche.
	//
	// m_faceMaterial porte le nombre de faces (Mesh::GetNFaces en derive), donc
	// GetNFaces() * sizeof(unsigned) est sa taille EXACTE, pas une estimation.
	const Mesh *mesh = static_cast<const Mesh *> (value);
	return sizeof (Mesh) + mesh->GetVertices ().size () * sizeof (float)
	       + mesh->GetVertexNormals ().size () * sizeof (float)
	       + mesh->GetFaceNormals ().size () * sizeof (float)
	       + mesh->GetVertexColors ().size () * sizeof (float)
	       + mesh->GetTextureCoordinates ().size () * sizeof (float)
	       + static_cast<std::size_t> (mesh->GetNFaces ()) * 3u * sizeof (unsigned int)
	       + static_cast<std::size_t> (mesh->GetNFaces ()) * sizeof (unsigned int);
}

std::shared_ptr<void> CloneImage (const void *value)
{
	// Copie PROFONDE : Img tient la regle des 3, son operateur de copie duplique
	// le tampon de pixels et la palette (image.h, « regle des 3/5 »).
	return std::make_shared<Img> (*static_cast<const Img *> (value));
}

std::size_t SizeOfImage (const void *value)
{
	// EXACT pour le tampon de pixels, qui est tout le poids : RGBA8 entrelace,
	// width*height*4 octets, contrat documente par Img::data(). La palette
	// eventuelle -- au plus 256 entrees -- n'est pas comptee ; a l'echelle d'une
	// image elle est du bruit, et Palette ne publie pas sa taille en octets.
	const Img *img = static_cast<const Img *> (value);
	return sizeof (Img)
	       + static_cast<std::size_t> (img->width ()) * img->height () * 4u;
}

bool PreviewImage (const void *value, int maxSide, cggraph::Thumbnail &out)
{
	const Img *img = static_cast<const Img *> (value);
	if (img == nullptr || img->width () == 0 || img->height () == 0 || maxSide <= 0)
		return false;

	// Reduction proportionnelle, jamais d'agrandissement : une vignette plus
	// grande que sa source n'apprend rien et coute une interpolation.
	const unsigned int w = img->width (), h = img->height ();
	const unsigned int largest = w > h ? w : h;
	unsigned int tw = w, th = h;
	if (largest > (unsigned int)maxSide)
	{
		const double k = (double)maxSide / (double)largest;
		tw = (unsigned int)(w * k); if (tw == 0) tw = 1;
		th = (unsigned int)(h * k); if (th == 0) th = 1;
	}

	// COPIE : resize modifie l'image, et celle-ci est la valeur d'un lien,
	// partagee par tous ses consommateurs.
	Img scaled (*img);
	if (tw != w || th != h)
		scaled.resize (tw, th, /*mode=*/1);   // bilineaire
	if (scaled.width () == 0 || scaled.height () == 0 || scaled.data () == nullptr)
		return false;

	out.width = (int)scaled.width ();
	out.height = (int)scaled.height ();
	const std::size_t n = (std::size_t)out.width * out.height * 4u;
	out.rgba.assign (scaled.data (), scaled.data () + n);
	return true;
}

std::size_t SizeOfPath (const void *value)
{
	const std::string *path = static_cast<const std::string *> (value);
	return sizeof (std::string) + path->size ();
}

std::size_t SizeOfMeshArray (const void *value)
{
	// Somme des minorants de ses elements. Les elements sont PARTAGES -- deux
	// suites qui contiennent le meme maillage le comptent chacune, et le cache
	// surestime donc. Surestimer fait evincer trop tot ; sous-estimer ferait
	// depasser le budget sans que rien ne le dise. Le biais est choisi.
	const MeshArray *array = static_cast<const MeshArray *> (value);
	std::size_t total = sizeof (MeshArray) + array->items.size () * sizeof (std::shared_ptr<Mesh>);
	for (const std::shared_ptr<const Mesh> &item : array->items)
		if (item != nullptr)
			total += SizeOfMesh (item.get ());
	return total;
}

std::size_t SizeOfSelection (const void *value)
{
	const Selection *selection = static_cast<const Selection *> (value);
	return sizeof (Selection) + selection->vertices.size () * sizeof (unsigned int);
}

std::size_t SizeOfScalarField (const void *value)
{
	const ScalarField *field = static_cast<const ScalarField *> (value);
	return sizeof (ScalarField) + field->values.size () * sizeof (float)
	       + field->defined.size () * sizeof (char);
}

std::size_t SizeOfGlyphContours (const void *value)
{
	const std::vector<GlyphContour> *contours =
		static_cast<const std::vector<GlyphContour> *> (value);
	std::size_t total = sizeof (*contours);
	for (const GlyphContour &contour : *contours)
		total += sizeof (contour) + contour.segments.size () * sizeof (GlyphSegment);
	return total;
}

std::size_t SizeOfExtrudeContours (const void *value)
{
	const std::vector<ExtrudeContour> *contours =
		static_cast<const std::vector<ExtrudeContour> *> (value);
	std::size_t total = sizeof (*contours);
	for (const ExtrudeContour &contour : *contours)
		total += sizeof (contour) + contour.pts.size () * sizeof (Vector2f);
	return total;
}

std::size_t SizeOfProfile (const void *value)
{
	const Profile2D *profile = static_cast<const Profile2D *> (value);
	return sizeof (Profile2D) + profile->points.size () * sizeof (Vector2d);
}

DomainTypes BuildTypes (cggraph::TypeRegistry &registry)
{
	DomainTypes types;

	cggraph::TypeDesc mesh;
	mesh.name = "cgmesh.Mesh";
	mesh.clone = &CloneMesh;
	mesh.sizeHint = &SizeOfMesh;
	// Forkable : un noeud aval a une raison legitime de vouloir ecrire dans le
	// maillage qu'il recoit. Il copie aujourd'hui, franchement, et c'est ce
	// clone qui portera plus tard la copie a la demande.
	mesh.mutability = cggraph::TypeDesc::Forkable;
	types.mesh = registry.Register (mesh);

	cggraph::TypeDesc image;
	image.name = "cgimg.Img";
	image.clone = &CloneImage;
	image.sizeHint = &SizeOfImage;
	// Forkable, comme le maillage et pour le meme motif : un noeud aval a une
	// raison legitime de vouloir ecrire dans l'image qu'il recoit. Aucun ne le
	// fait aujourd'hui -- les adaptateurs de relief et de blocs copient
	// franchement, parce que la vectorisation PALETTISE son entree -- et ce clone
	// portera la copie a la demande le jour ou elle existera.
	image.mutability = cggraph::TypeDesc::Forkable;
	// SEUL type du catalogue a savoir se montrer : ses octets SONT deja une
	// image. Un maillage demanderait un rendu hors ecran, une police une
	// rasterisation de contours -- ni l'un ni l'autre n'est un crochet de
	// quelques lignes.
	image.preview = &PreviewImage;
	types.image = registry.Register (image);

	cggraph::TypeDesc path;
	path.name = "nodes.Path";
	path.sizeHint = &SizeOfPath;
	path.mutability = cggraph::TypeDesc::Immutable;
	types.path = registry.Register (path);

	cggraph::TypeDesc meshArray;
	meshArray.name = "cgmesh.MeshArray";
	meshArray.sizeHint = &SizeOfMeshArray;
	// Immutable, alors que son element est Forkable : la SUITE se construit une
	// fois et se lit ensuite. Un noeud qui veut la modifier en construit une
	// neuve, dont il partage les elements inchanges -- c'est le partage des
	// elements qui rend l'operation bon marche, et il serait perdu par un clone
	// de la suite.
	meshArray.mutability = cggraph::TypeDesc::Immutable;
	types.meshArray = registry.Register (meshArray);

	cggraph::TypeDesc font;
	font.name = "cgmath.Font";
	// Sans clone, et ce n'est pas un trou : Font POSSEDE son buffer d'octets,
	// dont stb_truetype ne garde qu'un pointeur, si bien que sa copie est
	// supprimee par declaration. Un descripteur Forkable ne compilerait pas.
	// sizeHint nul aussi : la classe ne publie pas la taille de son buffer.
	font.mutability = cggraph::TypeDesc::Immutable;
	types.font = registry.Register (font);

	cggraph::TypeDesc selection;
	selection.name = "nodes.Selection";
	selection.sizeHint = &SizeOfSelection;
	selection.mutability = cggraph::TypeDesc::Immutable;
	types.selection = registry.Register (selection);

	cggraph::TypeDesc scalarField;
	scalarField.name = "nodes.ScalarField";
	scalarField.sizeHint = &SizeOfScalarField;
	// Immutable : aucun noeud n'ecrit dans un champ recu. Ceux qui en derivent
	// un autre en construisent un neuf, dont ils sont seuls proprietaires
	// jusqu'a la cession.
	scalarField.mutability = cggraph::TypeDesc::Immutable;
	types.scalarField = registry.Register (scalarField);

	cggraph::TypeDesc glyphContours;
	glyphContours.name = "cgmath.GlyphContours";
	glyphContours.sizeHint = &SizeOfGlyphContours;
	glyphContours.mutability = cggraph::TypeDesc::Immutable;
	types.glyphContours = registry.Register (glyphContours);

	cggraph::TypeDesc extrudeContours;
	extrudeContours.name = "cgmesh.ExtrudeContours";
	extrudeContours.sizeHint = &SizeOfExtrudeContours;
	extrudeContours.mutability = cggraph::TypeDesc::Immutable;
	types.extrudeContours = registry.Register (extrudeContours);

	// Meme charge utile, deux identites. Le nom est SERIALISE, donc les deux se
	// distinguent aussi dans un document.
	cggraph::TypeDesc splayProfile;
	splayProfile.name = "cgmesh.Profile2D.splay";
	splayProfile.sizeHint = &SizeOfProfile;
	splayProfile.mutability = cggraph::TypeDesc::Immutable;
	types.splayProfile = registry.Register (splayProfile);

	cggraph::TypeDesc barProfile;
	barProfile.name = "cgmesh.Profile2D.bar";
	barProfile.sizeHint = &SizeOfProfile;
	barProfile.mutability = cggraph::TypeDesc::Immutable;
	types.barProfile = registry.Register (barProfile);

	return types;
}

} // namespace

const DomainTypes &Types ()
{
	static cggraph::TypeRegistry registry;
	static const DomainTypes types = BuildTypes (registry);
	return types;
}

} // namespace cggraph_nodes
