#include "file_identity.h"

#include <cstdio>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include "../core/signature.h"

namespace cggraph_nodes
{

namespace
{

std::string ToDecimal (std::uint64_t value)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "%llu", static_cast<unsigned long long> (value));
	return std::string (buffer);
}

} // namespace

FileIdentity StatFile (const std::string &path)
{
	FileIdentity identity;
	if (path.empty ())
		return identity;

#ifdef _WIN32
	// _stat64 et non _stat : st_size y est un long de 32 bits, donc muet
	// au-dela de 2 Gio -- un maillage y arrive.
	struct __stat64 st;
	if (_stat64 (path.c_str (), &st) != 0)
		return identity;
#else
	struct stat st;
	if (stat (path.c_str (), &st) != 0)
		return identity;
#endif

	identity.exists = true;
	identity.mtime = static_cast<std::uint64_t> (st.st_mtime);
	identity.size = static_cast<std::uint64_t> (st.st_size);
	return identity;
}

std::uint64_t HashBuffer (const void *data, std::size_t size)
{
	// Le hachage du moteur, reutilise tel quel : la signature est un module
	// libre, il n'y a donc pas d'evaluateur a instancier pour s'en servir.
	const cggraph::Hash seed = 14695981039346656037ull;
	const cggraph::Hash sized = cggraph::HashBytes (&size, sizeof (size), seed);
	return cggraph::HashBytes (data, size, sized);
}

bool HashFile (const std::string &path, std::uint64_t &hash)
{
	FILE *file = std::fopen (path.c_str (), "rb");
	if (file == nullptr)
		return false;

	std::vector<unsigned char> bytes;
	unsigned char chunk[16384];
	std::size_t read = 0;
	while ((read = std::fread (chunk, 1, sizeof (chunk), file)) > 0)
		bytes.insert (bytes.end (), chunk, chunk + read);
	std::fclose (file);

	hash = HashBuffer (bytes.data (), bytes.size ());
	return true;
}

std::string StatKey (const FileIdentity &identity)
{
	if (!identity.exists)
		return "absent";
	return "stat:" + ToDecimal (identity.mtime) + ":" + ToDecimal (identity.size);
}

std::string HashKey (std::uint64_t hash)
{
	return "hash:" + HexDigits (hash);
}

std::string HexDigits (std::uint64_t hash)
{
	char buffer[32];
	std::snprintf (buffer, sizeof (buffer), "%016llx", static_cast<unsigned long long> (hash));
	return std::string (buffer);
}

} // namespace cggraph_nodes
