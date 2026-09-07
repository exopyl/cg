// ===========================================================================
//  Lecteurs binaires : fichiers TRONQUES et CORROMPUS
// ===========================================================================
//
// Ce fichier couvre le trou que `debt_cgmesh.md` signale au § 1 : les lecteurs
// 3DS, PLY et NBT etaient exerces UNIQUEMENT sur des fichiers valides, alors
// que ce sont eux qui lisent des longueurs et des indices venus du fichier pour
// en faire des tailles d'allocation et des offsets d'ecriture. Quatre ecritures
// hors bornes a contenu controle y ont ete trouvees ; sans le present filet,
// rien n'empeche leur retour.
//
// LES FIXTURES SONT FABRIQUES DANS LE TEST, a partir d'un fichier valide du
// depot, et non ajoutes en actifs. Trois raisons :
//
//   1. un fichier volontairement casse dans `test/data` est une invitation
//      permanente a le « reparer » ;
//   2. la troncature a N octets se decrit en une ligne, ce qui rend la table de
//      cas lisible -- alors qu'un binaire opaque ne dit pas ce qu'il teste ;
//   3. la technique existe deja dans le depot
//      (`tu_cgmesh_io.cpp:truncation_recovery_completes_every_cut_position`,
//      qui recopie Ref.jpg sous les noms voulus).
//
// CE QUE CES TESTS PROUVENT, ET CE QU'ILS NE PROUVENT PAS
// -------------------------------------------------------
// Ils prouvent que le lecteur REND LA MAIN sans planter, et que la geometrie
// rendue est coherente avec elle-meme (tout indice de face designe un sommet
// existant). Ils ne prouvent PAS l'absence d'ecriture hors bornes : un
// depassement de quelques octets dans un tas Debug ne se voit pas.
//
// C'est le role du sanitizer, et c'est pourquoi la cible ASan/UBSan
// (CG_ENABLE_SANITIZERS, cf. CMakeLists.txt racine) fait partie du meme
// constat : SOUS ASan, ces memes tests deviennent des detecteurs. Sans lui, ils
// ne valent que comme non-regression fonctionnelle.
//
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "../src/cgmesh/mesh.h"
#include "../src/cgmesh/vmeshes.h"
#include "../src/cgmesh/vmeshes_io.h"

namespace {

// Octets d'un fichier du depot.
std::vector<unsigned char> readAll (const std::filesystem::path& p)
{
	std::ifstream in (p, std::ios::binary);
	if (!in)
		return {};
	return std::vector<unsigned char> (std::istreambuf_iterator<char> (in),
	                                   std::istreambuf_iterator<char> ());
}

bool writeAll (const std::filesystem::path& p, const std::vector<unsigned char>& bytes)
{
	std::ofstream out (p, std::ios::binary | std::ios::trunc);
	if (!out)
		return false;
	if (!bytes.empty ())
		out.write (reinterpret_cast<const char*> (bytes.data ()), (std::streamsize) bytes.size ());
	return (bool) out;
}

// INVARIANT DE COHERENCE. C'est l'oracle de ces tests : quelle que soit la
// corruption de l'entree, un maillage rendu par l'import doit etre lisible sans
// sortir de ses propres tableaux. Un indice de face qui designe un sommet
// inexistant est precisement ce qui faisait ecrire ComputeNormals hors bornes.
void expectSelfConsistent (VMeshes& vm)
{
	for (Mesh* m : vm.GetMeshes ())
	{
		ASSERT_NE(m, nullptr);
		const unsigned int nv = m->GetNVertices ();
		for (unsigned int f = 0; f < m->GetNFaces (); ++f)
		{
			auto face = m->FaceAt (f);
			if (!face)
				continue;
			for (int k = 0; k < face->GetNVertices (); ++k)
			{
				const int vi = face->GetVertex (k);
				ASSERT_GE(vi, 0) << "face " << f << " sommet " << k;
				ASSERT_LT((unsigned int) vi, nv) << "face " << f << " sommet " << k;
			}
		}
		// ComputeNormals est le consommateur qui debordait. On l'appelle donc
		// explicitement : c'est lui qu'on veut voir survivre, pas seulement
		// l'import.
		m->ComputeNormals ();
		m->computebbox ();
	}
}

// Charge `bytes` sous `name` et exige que l'import rende la main. Le VERDICT
// n'est pas « l'import reussit » -- refuser un fichier casse est une reponse
// legitime, et souvent la bonne -- mais « il ne plante pas, et s'il rend une
// geometrie, elle est coherente ».
void expectSurvivesLoad (const std::vector<unsigned char>& bytes, const std::string& name)
{
	ASSERT_TRUE(writeAll (name, bytes)) << "ecriture du fixture impossible : " << name;

	VMeshes vm;
	VMeshesIO::load (vm, name.c_str ());   // le booleen n'est pas l'oracle
	expectSelfConsistent (vm);

	std::error_code ec;
	std::filesystem::remove (name, ec);
}

// Positions de coupure : la table est en FRACTIONS, de sorte qu'elle garde son
// sens quel que soit le fichier source. Les extremes comptent autant que le
// milieu -- 0 octet et 6 octets tombent dans l'en-tete lui-meme, la ou la
// soustraction `length - bytesRead` debordait.
const double kCutFractions[] = { 0.0, 0.001, 0.01, 0.1, 0.25, 0.5, 0.75, 0.9, 0.999 };

}  // namespace

// ---------------------------------------------------------------------------
//  3DS
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_io_malformed, a_truncated_3ds_never_crashes_and_stays_consistent)
{
	const std::vector<unsigned char> full = readAll ("./test/data/sink.3ds");
	ASSERT_FALSE(full.empty ()) << "fichier source absent";

	for (double frac : kCutFractions)
	{
		const size_t cut = (size_t) (frac * (double) full.size ());
		SCOPED_TRACE(::testing::Message () << "3DS tronque a " << cut << " / " << full.size ()
		                                   << " octets");
		expectSurvivesLoad (std::vector<unsigned char> (full.begin (), full.begin () + cut),
		                    "./tu_malformed.3ds");
	}
}

// Les longueurs de chunk sont le vecteur principal : quatre octets a l'offset 2
// pilotaient a la fois la taille lue et la taille allouee.
TEST(TEST_cgmesh_io_malformed, a_3ds_with_a_forged_chunk_length_never_crashes)
{
	const std::vector<unsigned char> full = readAll ("./test/data/sink.3ds");
	ASSERT_FALSE(full.empty ());
	ASSERT_GE(full.size (), 6u);

	// Les valeurs qui comptent : zero et 1 sont INFERIEURES aux six octets de
	// l'en-tete, donc c'est la que `length - bytesRead` bouclait vers ~4 Go.
	const uint32_t forged[] = { 0u, 1u, 5u, 6u, 7u, 0x7fffffffu, 0xfffffff0u, 0xffffffffu };

	for (uint32_t value : forged)
	{
		SCOPED_TRACE(::testing::Message () << "longueur du chunk racine forgee a " << value);
		std::vector<unsigned char> bytes = full;
		bytes[2] = (unsigned char) (value & 0xffu);
		bytes[3] = (unsigned char) ((value >> 8) & 0xffu);
		bytes[4] = (unsigned char) ((value >> 16) & 0xffu);
		bytes[5] = (unsigned char) ((value >> 24) & 0xffu);
		expectSurvivesLoad (bytes, "./tu_forged.3ds");
	}
}

// ---------------------------------------------------------------------------
//  PLY
// ---------------------------------------------------------------------------
TEST(TEST_cgmesh_io_malformed, a_truncated_ply_never_crashes_and_stays_consistent)
{
	const std::vector<unsigned char> full = readAll ("./test/data/sofa.ply");
	ASSERT_FALSE(full.empty ()) << "fichier source absent";

	for (double frac : kCutFractions)
	{
		const size_t cut = (size_t) (frac * (double) full.size ());
		SCOPED_TRACE(::testing::Message () << "PLY tronque a " << cut << " / " << full.size ());
		expectSurvivesLoad (std::vector<unsigned char> (full.begin (), full.begin () + cut),
		                    "./tu_malformed.ply");
	}
}

// Le PLY ASCII declare ses comptes en clair : un `element vertex` mensonger fait
// lire plus d'elements que le corps n'en porte.
TEST(TEST_cgmesh_io_malformed, a_ply_with_a_lying_element_count_never_crashes)
{
	const std::vector<unsigned char> full = readAll ("./test/data/sofa_ascii.ply");
	ASSERT_FALSE(full.empty ());

	std::string text (full.begin (), full.end ());
	const size_t pos = text.find ("element vertex ");
	ASSERT_NE(pos, std::string::npos) << "en-tete PLY inattendu";
	const size_t eol = text.find ('\n', pos);
	ASSERT_NE(eol, std::string::npos);

	// Les comptes restent MODESTES a dessein. RPly lit les elements un par un
	// jusqu'a la fin du flux : avec 2147483647, la lecture aboutit -- en erreur,
	// proprement -- mais apres 90 secondes. Le travail non borne par un compte
	// d'en-tete mensonger est un constat en soi (il n'est PAS dans les quatre du
	// § 1), et ce test n'est pas l'endroit pour le payer a chaque execution.
	for (const char* count : { "0", "1", "44243", "999999" })
	{
		SCOPED_TRACE(::testing::Message () << "element vertex " << count);
		std::string forged = text;
		forged.replace (pos, eol - pos, std::string ("element vertex ") + count);
		expectSurvivesLoad (std::vector<unsigned char> (forged.begin (), forged.end ()),
		                    "./tu_forged_ascii.ply");
	}
}

// ---------------------------------------------------------------------------
//  NBT
// ---------------------------------------------------------------------------
// Le NBT est gzippe : une troncature donne surtout des erreurs de flux, ce qui
// est deja un cas utile (gzread rendait alors 0 sans que personne ne le teste,
// et la longueur non lue servait de taille d'allocation).
TEST(TEST_cgmesh_io_malformed, a_truncated_nbt_never_crashes)
{
	const std::vector<unsigned char> full = readAll ("./test/data/nbt/ChickiSt26.nbt");
	ASSERT_FALSE(full.empty ()) << "fichier source absent";

	for (double frac : kCutFractions)
	{
		const size_t cut = (size_t) (frac * (double) full.size ());
		SCOPED_TRACE(::testing::Message () << "NBT tronque a " << cut << " / " << full.size ());
		expectSurvivesLoad (std::vector<unsigned char> (full.begin (), full.begin () + cut),
		                    "./tu_malformed.nbt");
	}
}

// Corruption A L'INTERIEUR du flux compresse : on retourne des octets au coeur
// du gzip, ce qui produit soit une erreur de CRC, soit -- plus interessant -- un
// arbre NBT syntaxiquement lisible aux longueurs absurdes. C'est ce second cas
// qui atteignait les malloc non testes.
TEST(TEST_cgmesh_io_malformed, an_nbt_with_flipped_bytes_never_crashes)
{
	const std::vector<unsigned char> full = readAll ("./test/data/nbt/ChickiSt26.nbt");
	ASSERT_FALSE(full.empty ());
	ASSERT_GT(full.size (), 32u);

	// Graine FIXE : un test qui echoue une fois sur cent est un test qu'on
	// finit par desactiver. Les positions sont donc reproductibles.
	uint32_t seed = 20260907u;
	auto next = [&seed]() { seed = seed * 1664525u + 1013904223u; return seed; };

	for (int iteration = 0; iteration < 24; ++iteration)
	{
		std::vector<unsigned char> bytes = full;
		// Apres l'en-tete gzip (10 octets), et jamais sur les 8 derniers (CRC +
		// taille) : les toucher ne donnerait qu'une erreur de CRC, sans jamais
		// atteindre l'arbre NBT.
		const size_t span = bytes.size () - 18u;
		for (int k = 0; k < 3; ++k)
		{
			const size_t at = 10u + (next () % span);
			bytes[at] = (unsigned char) (bytes[at] ^ (unsigned char) (1u << (next () % 8u)));
		}
		SCOPED_TRACE(::testing::Message () << "NBT, iteration " << iteration);
		expectSurvivesLoad (bytes, "./tu_flipped.nbt");
	}
}
