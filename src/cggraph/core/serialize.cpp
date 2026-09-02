#include "serialize.h"

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

namespace cggraph
{

namespace
{

using nlohmann::json;

const char *kFormatTag = "cggraph";

// Les enumerations se serialisent par un NOM et non par leur valeur entiere :
// un entier se decale silencieusement des qu'on insere une valeur dans
// l'enumeration, et le document deja ecrit se relit alors en disant autre chose.
const char *RoleName (ParamRole role)
{
	return role == ParamRole::NonSemantic ? "nonSemantic" : "semantic";
}

bool ReadRole (const std::string &text, ParamRole &role)
{
	if (text == "semantic")
		role = ParamRole::Semantic;
	else if (text == "nonSemantic")
		role = ParamRole::NonSemantic;
	else
		return false;
	return true;
}

const char *VisibilityName (ParamVisibility visibility)
{
	return visibility == ParamVisibility::Internal ? "internal" : "public";
}

bool ReadVisibility (const std::string &text, ParamVisibility &visibility)
{
	if (text == "public")
		visibility = ParamVisibility::Public;
	else if (text == "internal")
		visibility = ParamVisibility::Internal;
	else
		return false;
	return true;
}

const char *KindName (ParamKind kind)
{
	return kind == ParamKind::Driven ? "driven" : "literal";
}

bool ReadKind (const std::string &text, ParamKind &kind)
{
	if (text == "literal")
		kind = ParamKind::Literal;
	else if (text == "driven")
		kind = ParamKind::Driven;
	else
		return false;
	return true;
}

const char *TypeName (ParamType type)
{
	switch (type)
	{
	case ParamType::Int:
		return "int";
	case ParamType::Float:
		return "float";
	case ParamType::Bool:
		return "bool";
	case ParamType::String:
		return "string";
	}
	return "int";
}

bool ReadType (const std::string &text, ParamType &type)
{
	if (text == "int")
		type = ParamType::Int;
	else if (text == "float")
		type = ParamType::Float;
	else if (text == "bool")
		type = ParamType::Bool;
	else if (text == "string")
		type = ParamType::String;
	else
		return false;
	return true;
}

const char *ConnectStatusName (ConnectStatus status)
{
	switch (status)
	{
	case ConnectStatus::Ok:
		return "ok";
	case ConnectStatus::UnknownNode:
		return "unknownNode";
	case ConnectStatus::UnknownPort:
		return "unknownPort";
	case ConnectStatus::TypeMismatch:
		return "typeMismatch";
	case ConnectStatus::InputAlreadyConnected:
		return "inputAlreadyConnected";
	case ConnectStatus::Cycle:
		return "cycle";
	}
	return "unknown";
}

LoadResult Refuse (SerializeStatus status, const std::string &detail)
{
	LoadResult result;
	result.status = status;
	result.detail = detail;
	return result;
}

json SaveParam (const ParamEntry &entry)
{
	json out = json::object ();
	out["name"] = entry.name;
	out["role"] = RoleName (entry.role);

	// Ecrit seulement quand il s'ecarte du defaut, comme la reference de
	// sous-graphe : un document deja ecrit, ou tout est Public, se relit et se
	// re-sauve a l'octet pres au lieu de gagner une ligne par parametre.
	if (entry.visibility != ParamVisibility::Public)
		out["visibility"] = VisibilityName (entry.visibility);

	out["kind"] = KindName (entry.value.kind);
	out["type"] = TypeName (entry.value.type);

	if (entry.value.kind == ParamKind::Driven)
	{
		out["expression"] = entry.value.expression;
		return out;
	}

	switch (entry.value.type)
	{
	case ParamType::Int:
		out["value"] = entry.value.intValue;
		break;
	case ParamType::Float:
		out["value"] = entry.value.floatValue;
		break;
	case ParamType::Bool:
		out["value"] = entry.value.boolValue;
		break;
	case ParamType::String:
		out["value"] = entry.value.stringValue;
		break;
	}
	return out;
}

bool LoadParam (const json &entry, ParamSet &params, std::string &detail)
{
	if (!entry.is_object ())
	{
		detail = "param";
		return false;
	}

	const json::const_iterator name = entry.find ("name");
	const json::const_iterator role = entry.find ("role");
	const json::const_iterator kind = entry.find ("kind");
	const json::const_iterator type = entry.find ("type");
	if (name == entry.end () || !name->is_string () || role == entry.end () || !role->is_string ()
	    || kind == entry.end () || !kind->is_string () || type == entry.end () || !type->is_string ())
	{
		detail = "param";
		return false;
	}

	const std::string paramName = name->get<std::string> ();
	ParamRole paramRole = ParamRole::Semantic;
	ParamKind paramKind = ParamKind::Literal;
	ParamType paramType = ParamType::Int;
	if (paramName.empty () || !ReadRole (role->get<std::string> (), paramRole)
	    || !ReadKind (kind->get<std::string> (), paramKind)
	    || !ReadType (type->get<std::string> (), paramType))
	{
		detail = "param " + paramName;
		return false;
	}

	// Champ absent = Public : un document ecrit avant que cet axe existe se
	// relit sans migration. Present mais illisible = refus, comme les trois
	// autres enumerations -- deviner ferait entrer dans le graphe un etat que
	// personne n'a demande.
	ParamVisibility paramVisibility = ParamVisibility::Public;
	const json::const_iterator visibility = entry.find ("visibility");
	if (visibility != entry.end ()
	    && (!visibility->is_string () || !ReadVisibility (visibility->get<std::string> (), paramVisibility)))
	{
		detail = "param " + paramName;
		return false;
	}

	if (paramKind == ParamKind::Driven)
	{
		const json::const_iterator expression = entry.find ("expression");
		if (expression == entry.end () || !expression->is_string ())
		{
			detail = "param " + paramName;
			return false;
		}
		params.SetDriven (paramName, paramType, expression->get<std::string> (), paramRole,
		                  paramVisibility);
		return true;
	}

	const json::const_iterator value = entry.find ("value");
	if (value == entry.end ())
	{
		detail = "param " + paramName;
		return false;
	}

	switch (paramType)
	{
	case ParamType::Int:
		if (!value->is_number_integer ())
		{
			detail = "param " + paramName;
			return false;
		}
		params.SetInt (paramName, value->get<int> (), paramRole, paramVisibility);
		return true;
	case ParamType::Float:
		if (!value->is_number ())
		{
			detail = "param " + paramName;
			return false;
		}
		params.SetFloat (paramName, value->get<float> (), paramRole, paramVisibility);
		return true;
	case ParamType::Bool:
		if (!value->is_boolean ())
		{
			detail = "param " + paramName;
			return false;
		}
		params.SetBool (paramName, value->get<bool> (), paramRole, paramVisibility);
		return true;
	case ParamType::String:
		if (!value->is_string ())
		{
			detail = "param " + paramName;
			return false;
		}
		params.SetString (paramName, value->get<std::string> (), paramRole, paramVisibility);
		return true;
	}

	detail = "param " + paramName;
	return false;
}

} // namespace

const char *ToString (SerializeStatus status)
{
	switch (status)
	{
	case SerializeStatus::Ok:
		return "ok";
	case SerializeStatus::FileNotReadable:
		return "fichier illisible";
	case SerializeStatus::FileNotWritable:
		return "fichier non inscriptible";
	case SerializeStatus::ParseError:
		return "JSON invalide";
	case SerializeStatus::NotAGraph:
		return "ce n'est pas un document de graphe";
	case SerializeStatus::UnsupportedFormat:
		return "version de format non supportee";
	case SerializeStatus::GraphNotEmpty:
		return "le graphe d'accueil n'est pas vide";
	case SerializeStatus::BadNode:
		return "entree de noeud mal formee";
	case SerializeStatus::DuplicateNodeId:
		return "identifiant de noeud en double";
	case SerializeStatus::UnknownNodeType:
		return "type de noeud inconnu";
	case SerializeStatus::BadLink:
		return "lien refuse";
	}
	return "statut inconnu";
}

std::string SaveGraph (const Graph &graph)
{
	json document = json::object ();
	document["format"] = kFormatTag;
	document["formatVersion"] = kDocumentFormatVersion;

	json nodes = json::array ();
	for (NodeId id : graph.GetNodeIds ())
	{
		const Node *node = graph.FindNode (id);
		if (node == nullptr)
			continue;

		float x = 0.0f;
		float y = 0.0f;
		graph.GetNodePosition (id, x, y);

		json entry = json::object ();
		entry["id"] = id;
		entry["type"] = node->GetDesc ().typeName;

		// La version ECRITE est celle du document dont le noeud vient, non celle
		// du descripteur courant : re-sauver un document non migre ne doit pas
		// le declarer a jour.
		entry["version"] = node->GetDocumentVersion ();
		entry["x"] = x;
		entry["y"] = y;

		// Ecrit seulement quand il porte quelque chose : un champ vide sur
		// chaque noeud alourdit un document que des humains relisent.
		// Une seule des deux formes peut etre posee -- le graphe fait s'exclure
		// chemin et document embarque --, donc l'ordre du test ne tranche rien.
		const std::string &subgraph = graph.GetNodeSubgraph (id);
		const std::string &subgraphDocument = graph.GetNodeSubgraphDocument (id);
		if (!subgraph.empty ())
			entry["subgraph"] = subgraph;
		else if (!subgraphDocument.empty ())
		{
			// RE-ANALYSE plutot que recopie textuelle : le document est stocke en
			// texte, et l'inserer tel quel donnerait une chaine JSON echappee au
			// lieu d'un objet. Un texte devenu illisible entre-temps ne fait pas
			// echouer l'ecriture du parent -- il est alors ecrit tel quel, et sa
			// relecture le refusera en le nommant.
			json embedded = json::parse (subgraphDocument, nullptr, false);
			if (embedded.is_discarded ())
				entry["subgraph"] = subgraphDocument;
			else
				entry["subgraph"] = embedded;
		}

		json params = json::array ();
		for (const ParamEntry &param : node->GetParams ().GetEntries ())
			params.push_back (SaveParam (param));
		entry["params"] = params;

		nodes.push_back (entry);
	}
	document["nodes"] = nodes;

	json links = json::array ();
	for (const Link &link : graph.GetLinks ())
	{
		json entry = json::object ();
		entry["from"] = link.from;
		entry["fromPort"] = link.fromPort;
		entry["to"] = link.to;
		entry["toPort"] = link.toPort;
		links.push_back (entry);
	}
	document["links"] = links;

	return document.dump (1, '\t') + "\n";
}

LoadResult LoadGraph (const std::string &text, const NodeFactory &factory, Graph &graph)
{
	if (graph.GetNodeCount () != 0)
		return Refuse (SerializeStatus::GraphNotEmpty, std::string ());

	// Sans exception : un document mal forme est un cas nominal de ce module,
	// pas un incident.
	const json document = json::parse (text, nullptr, false);
	if (document.is_discarded () || !document.is_object ())
		return Refuse (SerializeStatus::ParseError, std::string ());

	const json::const_iterator format = document.find ("format");
	if (format == document.end () || !format->is_string () || format->get<std::string> () != kFormatTag)
		return Refuse (SerializeStatus::NotAGraph, std::string ());

	const json::const_iterator formatVersion = document.find ("formatVersion");
	if (formatVersion == document.end () || !formatVersion->is_number_integer ())
		return Refuse (SerializeStatus::NotAGraph, "formatVersion");

	// Plus RECENT que ce binaire : refus en entier, on ne devine pas des champs
	// qu'on ne connait pas. Plus ancien : accepte, ce module sait relire ce
	// qu'il a lui-meme ecrit.
	if (formatVersion->get<int> () > kDocumentFormatVersion)
		return Refuse (SerializeStatus::UnsupportedFormat, std::to_string (formatVersion->get<int> ()));

	const json::const_iterator nodes = document.find ("nodes");
	if (nodes == document.end () || !nodes->is_array ())
		return Refuse (SerializeStatus::NotAGraph, "nodes");

	LoadResult result;

	for (const json &entry : *nodes)
	{
		if (!entry.is_object ())
			return Refuse (SerializeStatus::BadNode, std::string ());

		const json::const_iterator id = entry.find ("id");
		const json::const_iterator type = entry.find ("type");
		const json::const_iterator version = entry.find ("version");
		if (id == entry.end () || !id->is_number_unsigned () || type == entry.end ()
		    || !type->is_string () || version == entry.end () || !version->is_number_integer ())
			return Refuse (SerializeStatus::BadNode, std::string ());

		const NodeId nodeId = id->get<NodeId> ();
		const std::string typeName = type->get<std::string> ();
		if (nodeId == kInvalidNodeId || typeName.empty ())
			return Refuse (SerializeStatus::BadNode, typeName);

		std::unique_ptr<Node> node = factory.Create (typeName);
		if (node == nullptr)
			return Refuse (SerializeStatus::UnknownNodeType, typeName);

		// Le DOCUMENT fait foi sur les parametres : ce que le constructeur du
		// noeud avait pose est efface, sans quoi un parametre absent du fichier
		// y reapparaitrait a la re-sauvegarde.
		node->GetParams ().Clear ();
		node->SetDocumentVersion (version->get<int> ());
		const bool compatible = node->IsVersionCompatible ();

		const json::const_iterator params = entry.find ("params");
		if (params != entry.end ())
		{
			if (!params->is_array ())
				return Refuse (SerializeStatus::BadNode, typeName);
			for (const json &param : *params)
			{
				std::string detail;
				if (!LoadParam (param, node->GetParams (), detail))
					return Refuse (SerializeStatus::BadNode, typeName + ": " + detail);
			}
		}

		if (graph.AddNodeWithId (nodeId, std::move (node)) == kInvalidNodeId)
			return Refuse (SerializeStatus::DuplicateNodeId, typeName);

		if (!compatible)
			result.incompatible.push_back (nodeId);

		const json::const_iterator x = entry.find ("x");
		const json::const_iterator y = entry.find ("y");
		if (x != entry.end () && y != entry.end () && x->is_number () && y->is_number ())
			graph.SetNodePosition (nodeId, x->get<float> (), y->get<float> ());

		// DEUX FORMES pour le meme champ, et le type les distingue :
		//   "subgraph": "corps.json"   -- un CHEMIN, resolu au calcul ;
		//   "subgraph": { ... }        -- le document EMBARQUE, autonome.
		//
		// La seconde est relue par le meme lecteur que le document parent, donc
		// une erreur dedans est nommee comme n'importe quelle autre. Elle est
		// re-serialisee telle quelle : l'objet est stocke sous forme de TEXTE et
		// non d'arbre, ce qui evite au graphe -- couche de base -- de porter un
		// type JSON dans son etat.
		const json::const_iterator subgraph = entry.find ("subgraph");
		if (subgraph != entry.end ())
		{
			if (subgraph->is_string ())
				graph.SetNodeSubgraph (nodeId, subgraph->get<std::string> ());
			else if (subgraph->is_object ())
				graph.SetNodeSubgraphDocument (nodeId, subgraph->dump ());
			else
				return Refuse (SerializeStatus::BadNode, typeName + ": subgraph");
		}
	}

	const json::const_iterator links = document.find ("links");
	if (links == document.end () || !links->is_array ())
		return Refuse (SerializeStatus::NotAGraph, "links");

	for (const json &entry : *links)
	{
		if (!entry.is_object ())
			return Refuse (SerializeStatus::BadLink, std::string ());

		const json::const_iterator from = entry.find ("from");
		const json::const_iterator fromPort = entry.find ("fromPort");
		const json::const_iterator to = entry.find ("to");
		const json::const_iterator toPort = entry.find ("toPort");
		if (from == entry.end () || !from->is_number_unsigned () || fromPort == entry.end ()
		    || !fromPort->is_number_unsigned () || to == entry.end () || !to->is_number_unsigned ()
		    || toPort == entry.end () || !toPort->is_number_unsigned ())
			return Refuse (SerializeStatus::BadLink, std::string ());

		// La relecture passe par la MEME validation que l'API publique. Un lien
		// qu'un document porte et que Connect refuse -- un port disparu d'une
		// version a l'autre, par exemple -- est refuse ici plutot que pose dans
		// le graphe : le chargeur n'est pas une porte derobee vers un etat que
		// l'editeur ne saurait pas produire.
		const ConnectStatus status =
			graph.Connect (from->get<NodeId> (), fromPort->get<PortIdx> (), to->get<NodeId> (),
			               toPort->get<PortIdx> ());
		if (status != ConnectStatus::Ok)
			return Refuse (SerializeStatus::BadLink, ConnectStatusName (status));
	}

	return result;
}

SerializeStatus SaveGraphToFile (const Graph &graph, const std::string &path)
{
	// Binaire : le document est ecrit tel quel, sans traduction de fin de ligne
	// par la bibliotheque -- sans quoi l'aller-retour ne serait fidele que sur
	// un seul systeme.
	std::ofstream file (path.c_str (), std::ios::binary);
	if (!file.is_open ())
		return SerializeStatus::FileNotWritable;

	const std::string text = SaveGraph (graph);
	file.write (text.data (), static_cast<std::streamsize> (text.size ()));
	if (!file.good ())
		return SerializeStatus::FileNotWritable;
	return SerializeStatus::Ok;
}

LoadResult LoadGraphFromFile (const std::string &path, const NodeFactory &factory, Graph &graph)
{
	std::ifstream file (path.c_str (), std::ios::binary);
	if (!file.is_open ())
		return Refuse (SerializeStatus::FileNotReadable, path);

	std::ostringstream text;
	text << file.rdbuf ();
	return LoadGraph (text.str (), factory, graph);
}

} // namespace cggraph
