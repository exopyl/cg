#include "save_mesh.h"

#include <memory>

#include "../../../cgmesh/mesh.h"
#include "../file_identity.h"
#include "../node_support.h"
#include "../value_types.h"

namespace cggraph_nodes
{

namespace
{

// Empreinte du contenu : positions puis triangles. Deux maillages qui
// s'ecriraient dans le meme fichier sont deux maillages identiques, et deux
// executions du meme calcul rendent la meme empreinte -- c'est ce qui la rend
// utilisable comme nom.
std::uint64_t HashMeshContent (const Mesh &mesh)
{
	const std::vector<float> &vertices = mesh.GetVertices ();
	const std::vector<unsigned int> triangles = mesh.GetTriangles ();
	const std::uint64_t positions =
		HashBuffer (vertices.data (), vertices.size () * sizeof (float));
	const std::uint64_t topology =
		HashBuffer (triangles.data (), triangles.size () * sizeof (unsigned int));
	std::uint64_t both[2] = { positions, topology };
	return HashBuffer (both, sizeof (both));
}

std::string ExpandPath (const std::string &path, const Mesh &mesh)
{
	const std::string token = "{hash}";
	if (path.find (token) == std::string::npos)
		return path;

	const std::string digits = HexDigits (HashMeshContent (mesh));
	std::string expanded = path;
	for (std::string::size_type at = expanded.find (token); at != std::string::npos;
	     at = expanded.find (token, at + digits.size ()))
		expanded.replace (at, token.size (), digits);
	return expanded;
}

const cggraph::NodeDesc &Desc ()
{
	static const cggraph::NodeDesc desc = [] {
		cggraph::NodeDesc d;
		d.typeName = "mesh.io.save";
		d.inputs.push_back ({ "maillage", Types ().mesh, false });
		d.sideEffect = true;
		return d;
	}();
	return desc;
}

} // namespace

SaveMeshNode::SaveMeshNode (const std::string &path)
{
	GetParams ().SetString ("path", path);
}

const cggraph::NodeDesc &SaveMeshNode::GetDesc () const
{
	return Desc ();
}

bool SaveMeshNode::Compute (cggraph::EvalContext &ctx, const cggraph::ValueList &in,
                            cggraph::ValueList &out)
{
	(void)ctx;
	(void)out;

	const std::shared_ptr<const Mesh> input = in[0].Share<Mesh> (Types ().mesh);
	if (input == nullptr)
		return false;

	const std::string path = GetString (GetParams (), "path", std::string ());
	if (path.empty ())
		return false;

	const std::string target = ExpandPath (path, *input);

	// save est const : le puits ecrit un fichier, jamais le maillage recu.
	++m_writes;
	if (input->save (target.c_str ()) != 0)
		return false;

	{
		const std::lock_guard<std::mutex> held (m_writtenMutex);
		m_written.push_back (target);
	}
	return true;
}

std::vector<std::string> SaveMeshNode::GetWrittenPaths () const
{
	const std::lock_guard<std::mutex> held (m_writtenMutex);
	return m_written;
}

} // namespace cggraph_nodes
