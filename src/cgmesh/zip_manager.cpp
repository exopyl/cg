#include "zip_manager.h"

#include <cstdio>
#include <cstring>

namespace {

// Table CRC-32 construite une fois. Le calcul bit a bit sur plusieurs Mo serait
// inutilement lent.
const unsigned int* crcTable ()
{
	static unsigned int table[256];
	static bool built = false;
	if (!built)
	{
		for (unsigned int n = 0; n < 256; ++n)
		{
			unsigned int c = n;
			for (int k = 0; k < 8; ++k)
				c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
			table[n] = c;
		}
		built = true;
	}
	return table;
}

// Ecriture petit-boutiste explicite : le format ZIP l'impose, et s'appuyer sur
// l'endianness de la machine casserait sur une cible big-endian.
void put16 (std::string& out, unsigned int v)
{
	out += (char)(v & 0xFF);
	out += (char)((v >> 8) & 0xFF);
}

void put32 (std::string& out, unsigned int v)
{
	out += (char)(v & 0xFF);
	out += (char)((v >> 8) & 0xFF);
	out += (char)((v >> 16) & 0xFF);
	out += (char)((v >> 24) & 0xFF);
}

} // namespace

unsigned int ZipManager::Crc32 (const void *data, size_t size)
{
	const unsigned int* t = crcTable();
	const unsigned char* p = (const unsigned char*)data;
	unsigned int c = 0xFFFFFFFFu;
	for (size_t i = 0; i < size; ++i)
		c = t[(c ^ p[i]) & 0xFF] ^ (c >> 8);
	return c ^ 0xFFFFFFFFu;
}

std::string ZipManager::BuildStored (const std::vector<Entry>& entries)
{
	if (entries.empty())
		return std::string();

	// Date/heure MS-DOS figees au 1980-01-01 00:00 : sortie deterministe.
	const unsigned int kDosTime = 0x0000;
	const unsigned int kDosDate = 0x0021;

	std::string out;
	std::vector<unsigned int> crcs (entries.size());
	std::vector<unsigned int> offsets (entries.size());

	// --- en-tetes locaux + donnees ---
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const Entry& e = entries[i];
		crcs[i]    = Crc32 (e.data.data(), e.data.size());
		offsets[i] = (unsigned int)out.size();

		put32 (out, 0x04034b50u);                 // signature en-tete local
		put16 (out, 20);                          // version requise (2.0)
		put16 (out, 0);                           // aucun drapeau
		put16 (out, 0);                           // methode 0 = stored
		put16 (out, kDosTime);
		put16 (out, kDosDate);
		put32 (out, crcs[i]);
		put32 (out, (unsigned int)e.data.size()); // taille compressee
		put32 (out, (unsigned int)e.data.size()); // taille reelle (identique)
		put16 (out, (unsigned int)e.name.size());
		put16 (out, 0);                           // pas de champ extra
		out += e.name;
		out += e.data;
	}

	// --- repertoire central ---
	const unsigned int centralStart = (unsigned int)out.size();
	for (size_t i = 0; i < entries.size(); ++i)
	{
		const Entry& e = entries[i];
		put32 (out, 0x02014b50u);                 // signature entree centrale
		put16 (out, 20);                          // version d'origine
		put16 (out, 20);                          // version requise
		put16 (out, 0);
		put16 (out, 0);                           // stored
		put16 (out, kDosTime);
		put16 (out, kDosDate);
		put32 (out, crcs[i]);
		put32 (out, (unsigned int)e.data.size());
		put32 (out, (unsigned int)e.data.size());
		put16 (out, (unsigned int)e.name.size());
		put16 (out, 0);                           // extra
		put16 (out, 0);                           // commentaire
		put16 (out, 0);                           // n0 de disque
		put16 (out, 0);                           // attributs internes
		put32 (out, 0);                           // attributs externes
		put32 (out, offsets[i]);                  // offset de l'en-tete local
		out += e.name;
	}
	const unsigned int centralSize = (unsigned int)out.size() - centralStart;

	// --- fin d'archive (EOCD) ---
	put32 (out, 0x06054b50u);
	put16 (out, 0);                                   // n0 de disque
	put16 (out, 0);                                   // disque du repertoire central
	put16 (out, (unsigned int)entries.size());        // entrees sur ce disque
	put16 (out, (unsigned int)entries.size());        // entrees au total
	put32 (out, centralSize);
	put32 (out, centralStart);
	put16 (out, 0);                                   // longueur du commentaire

	return out;
}

int ZipManager::WriteStored (const char *filename, const std::vector<Entry>& entries)
{
	if (!filename)
		return -1;
	const std::string bytes = BuildStored (entries);
	if (bytes.empty())
		return -1;

	FILE *fp = fopen (filename, "wb");
	if (!fp)
		return -1;
	const size_t written = fwrite (bytes.data(), 1, bytes.size(), fp);
	fclose (fp);
	return (written == bytes.size()) ? 0 : -1;
}
