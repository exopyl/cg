#pragma once

// ===========================================================================
//  ZipManager — écriture d'archives ZIP à entrées NON COMPRESSÉES
// ===========================================================================
//
// Utilitaire GÉNÉRIQUE : il ne connaît ni maillage, ni image, ni géométrie. On
// lui donne une liste de (nom, octets), il rend les octets d'une archive valide.
//
// Sert à livrer en UN SEUL fichier des formats qui en comptent plusieurs :
//   - OBJ + son .mtl compagnon (MeshIO::export_obj_zip) ;
//   - le 3MF, qui EST une archive OPC (plusieurs parties XML), quand il viendra.
//
// Pourquoi « stored » plutôt que deflate : aucune dépendance. zlib est vendored
// dans extern/ mais reste désactivé dans plusieurs configurations — dont la cible
// WebAssembly de maker — et le préfixage Z_PREFIX y est déjà une source de pièges.
// Une entrée non compressée est parfaitement conforme à la spec ZIP et s'ouvre
// partout ; on paie la taille, pas la complexité. Seul le CRC-32 est nécessaire.
//
// LIMITES, assumées : écriture seule (pas de lecture), pas de compression, pas de
// Zip64 — donc 4 Go par entrée et au total, ce qui n'est pas vérifié dans le code.
// Pas d'entrées de répertoire : les sous-dossiers passent par le nom
// («3D/3dmodel.model»), ce que les lecteurs acceptent.
//
// EMPLACEMENT : dans cgmesh parce que c'est son seul consommateur aujourd'hui, et
// que le dépôt n'a pas de bibliothèque utilitaire. Si un deuxième module en a
// besoin (cgnet pour servir une archive, cgimg pour un lot d'images), c'est le
// signe qu'il doit descendre d'un niveau.
//
// ===========================================================================

#include <string>
#include <vector>

class ZipManager
{
public:
	// Une entrée de l'archive. `data` est BINAIRE : std::string sert ici de
	// conteneur d'octets, pas de texte.
	struct Entry
	{
		std::string name;   // chemin dans l'archive, séparateurs '/'
		std::string data;
	};

	// Construit l'archive EN MÉMOIRE et renvoie ses octets. Chaîne vide si
	// `entries` est vide.
	//
	// Horodatage figé (1980-01-01) : la sortie est déterministe à contenu égal, ce
	// qui rend les archives reproductibles et les tests comparables.
	static std::string BuildStored (const std::vector<Entry>& entries);

	// Idem, écrit directement dans un fichier. 0 en cas de succès, -1 sinon.
	static int WriteStored (const char *filename, const std::vector<Entry>& entries);

	// CRC-32 (polynôme 0xEDB88320). Exposé parce que vérifier une archive suppose
	// de recalculer les CRC qu'elle déclare — c'est ce que font les tests.
	static unsigned int Crc32 (const void *data, size_t size);
};
