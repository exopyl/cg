#include "signature.h"

namespace cggraph
{

namespace
{

// FNV-1a 64 bits. Le choix n'engage rien : la signature n'est ni un
// identifiant persistant ni une protection, seulement une cle de cache.
const Hash kFnvOffset = 14695981039346656037ull;
const Hash kFnvPrime = 1099511628211ull;

// Distingue une entree non alimentee d'une entree alimentee par un amont dont
// la signature vaudrait la meme chose.
const Hash kUnconnected = 0x9e3779b97f4a7c15ull;

Hash HashScalar (Hash value, Hash seed)
{
	return HashBytes (&value, sizeof (value), seed);
}

} // namespace

Hash HashBytes (const void *data, std::size_t size, Hash seed)
{
	const unsigned char *bytes = static_cast<const unsigned char *> (data);
	Hash h = seed;
	for (std::size_t i = 0; i < size; ++i)
	{
		h ^= static_cast<Hash> (bytes[i]);
		h *= kFnvPrime;
	}
	return h;
}

Hash HashString (const std::string &text, Hash seed)
{
	// La longueur entre dans le hachage : sans elle, deux entrees consecutives
	// concatenees seraient indiscernables d'une seule.
	const Hash size = static_cast<Hash> (text.size ());
	Hash h = HashBytes (&size, sizeof (size), seed);
	return HashBytes (text.data (), text.size (), h);
}

Hash HashCombine (Hash left, Hash right)
{
	return HashBytes (&right, sizeof (right), left);
}

Hash HashParamSet (const ParamSet &params)
{
	Hash h = kFnvOffset;
	for (const ParamEntry &entry : params.GetEntries ())
	{
		if (entry.role != ParamRole::Semantic)
			continue;

		// Le nom entre dans le hachage : renommer un parametre change le calcul
		// qu'il decrit.
		h = HashString (entry.name, h);
		h = HashScalar (static_cast<Hash> (entry.value.kind), h);
		h = HashScalar (static_cast<Hash> (entry.value.type), h);

		if (entry.value.kind == ParamKind::Driven)
		{
			h = HashString (entry.value.expression, h);
			continue;
		}

		switch (entry.value.type)
		{
		case ParamType::Int:
			h = HashBytes (&entry.value.intValue, sizeof (entry.value.intValue), h);
			break;
		case ParamType::Float:
			h = HashBytes (&entry.value.floatValue, sizeof (entry.value.floatValue), h);
			break;
		case ParamType::Bool:
			h = HashBytes (&entry.value.boolValue, sizeof (entry.value.boolValue), h);
			break;
		case ParamType::String:
			h = HashString (entry.value.stringValue, h);
			break;
		}
	}
	return h;
}

Hash Signature (const Graph &graph, NodeId id, SignatureMemo &memo)
{
	SignatureMemo::const_iterator known = memo.find (id);
	if (known != memo.end ())
		return known->second;

	const Node *node = graph.FindNode (id);
	if (node == nullptr)
		return kNoSignature;

	const NodeDesc &desc = node->GetDesc ();
	Hash h = HashString (desc.typeName, kFnvOffset);
	h = HashCombine (h, HashParamSet (node->GetParams ()));

	// La reference de sous-graphe designe le CALCUL, au meme titre que le type
	// du noeud : deux delegations vers deux documents differents n'ont aucune
	// raison de partager une entree de cache. Elle est hachee des maintenant,
	// bien qu'aucun noeud ne l'interprete encore -- une signature completee
	// apres coup est exactement le defaut que ce module existe pour empecher.
	h = HashString (graph.GetNodeSubgraph (id), h);
	// Le document EMBARQUE entre dans la signature comme la reference, et il y
	// entre MIEUX : il est son propre contenu, donc l'editer change la
	// signature sans qu'aucun releve exterieur soit necessaire. Un chemin, lui,
	// ne hache qu'un nom -- d'ou le parametre d'identite que les noeuds de flux
	// tiennent a jour pour les fichiers.
	h = HashString (graph.GetNodeSubgraphDocument (id), h);

	// La recursion termine sans garde : le graphe refuse les cycles au moment
	// de la connexion, il n'en existe donc aucun a parcourir.
	for (std::size_t i = 0; i < desc.inputs.size (); ++i)
	{
		const Link *link = graph.FindInputLink (id, static_cast<PortIdx> (i));
		if (link == nullptr)
		{
			h = HashCombine (h, kUnconnected);
			continue;
		}
		h = HashCombine (h, Signature (graph, link->from, memo));
		// Le port lu entre dans la signature : deux sorties d'un meme amont
		// portent deux valeurs differentes.
		h = HashBytes (&link->fromPort, sizeof (link->fromPort), h);
	}

	if (h == kNoSignature)
		h = 1;
	memo[id] = h;
	return h;
}

Hash Signature (const Graph &graph, NodeId id)
{
	SignatureMemo memo;
	return Signature (graph, id, memo);
}

} // namespace cggraph
