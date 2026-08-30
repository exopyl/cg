#include <gtest/gtest.h>

#include "../src/cggraph/canvas/canvas_style.h"
#include "../src/cggraph/nodes/value_types.h"

#include <cmath>
#include <string>
#include <vector>

// ===========================================================================
//  Teintes du canvas -- LA SEULE PARTIE DU DESSIN QUI SE TESTE
// ===========================================================================
//
// ⚠ CE QUI N'EST PAS COUVERT, ET LE RESTE. Le rendu du canvas -- bandeaux,
// pastilles, colonnes, liens -- n'est verifie par aucun test, ici ni ailleurs :
// `TU` ne lie pas cggraph_canvas, qui n'est bati que sous
// ENABLE_CGGRAPH_BOILERPLATE, et un filet qui ne tourne pas en CI ne protege rien.
// La zone est DECLAREE non couverte, conformement a ce que l'etape 4 a deja
// inscrit pour ce fichier. Ce qui suit est le seul morceau du style qui soit
// une fonction PURE : elle ne dessine pas, elle decide d'une couleur, et cette
// decision-la se verifie.
//
// canvas_style.h est inclus et non lie : tout y est `inline` et rien n'y tire
// ImGui. C'est la propriete qui rend ce fichier possible, et c'est pourquoi
// elle est ecrite dans l'en-tete comme une contrainte et non comme un hasard.

using namespace cggraph_canvas;

namespace {

float Distance (const Rgb &a, const Rgb &b)
{
	const float dr = a.r - b.r;
	const float dg = a.g - b.g;
	const float db = a.b - b.b;
	return std::sqrt (dr * dr + dg * dg + db * db);
}

int Byte (float channel)
{
	return static_cast<int> (channel * 255.0f + 0.5f);
}

// Les noms REELLEMENT enregistres, lus dans le registre de domaine et non
// recopies : un type ajoute a value_types.cpp entre dans ce test sans qu'une
// ligne soit ecrite ici, et c'est la seule facon d'eprouver la separation sur
// l'ensemble qui existe plutot que sur celui qu'on se rappelle.
std::vector<std::string> RegisteredTypeNames ()
{
	const cggraph_nodes::DomainTypes &types = cggraph_nodes::Types ();
	const cggraph::TypeDesc *const all[] = { types.mesh,          types.meshArray,
		                                     types.font,          types.selection,
		                                     types.scalarField,   types.glyphContours,
		                                     types.extrudeContours, types.splayProfile,
		                                     types.barProfile };

	std::vector<std::string> names;
	for (const cggraph::TypeDesc *type : all)
		if (type != nullptr)
			names.push_back (type->name);
	return names;
}

} // namespace

// ---------------------------------------------------------------------------
//  Le hachage
// ---------------------------------------------------------------------------

// Vecteurs d'essai PUBLIES de FNV-1a 32 bits. Ils etablissent que la fonction
// est bien FNV-1a et non une variante voisine ecrite de memoire -- une teinte
// « stable » calculee par un hachage faux serait stable et fausse.
TEST (canvas_style, the_hash_matches_the_published_fnv1a_vectors)
{
	EXPECT_EQ (0x811c9dc5u, HashName ("", 0));
	EXPECT_EQ (0xe40c292cu, HashName ("a", 1));
	EXPECT_EQ (0xbf9cf968u, HashName ("foobar", 6));
}

TEST (canvas_style, the_hash_does_not_depend_on_the_signedness_of_char)
{
	// Un octet au-dela de 0x7f : lu comme `char` signe, il serait etendu en
	// negatif et le hachage differerait d'une plateforme a l'autre. Les deux
	// hotes doivent teinter un meme type de la meme facon.
	const char high[] = { static_cast<char> (0xe9), '\0' };
	EXPECT_EQ (HashName (high, 1), HashName (std::string (high, 1)));
	EXPECT_EQ (0x6c0b6c44u, HashName (high, 1));
}

// ---------------------------------------------------------------------------
//  Determinisme et stabilite
// ---------------------------------------------------------------------------

TEST (canvas_style, the_same_name_always_yields_the_same_tint)
{
	for (const std::string &name : RegisteredTypeNames ())
	{
		const Rgb first = NameTint (name);
		const Rgb second = NameTint (name);
		EXPECT_FLOAT_EQ (first.r, second.r) << name;
		EXPECT_FLOAT_EQ (first.g, second.g) << name;
		EXPECT_FLOAT_EQ (first.b, second.b) << name;
	}
}

// La teinte d'un type est une propriete DURABLE : l'utilisateur apprend que le
// maillage est de cette couleur-la. Ce cas fige donc la correspondance pour
// deux noms, a un pas de quantification pres -- la tolerance couvre l'ecart de
// virgule flottante entre compilateurs, pas un changement de formule.
TEST (canvas_style, the_tint_of_a_given_name_is_frozen)
{
	const Rgb mesh = NameTint (std::string ("cgmesh.Mesh"));
	EXPECT_NEAR (191, Byte (mesh.r), 1);
	EXPECT_NEAR (90, Byte (mesh.g), 1);
	EXPECT_NEAR (185, Byte (mesh.b), 1);

	const Rgb field = NameTint (std::string ("nodes.ScalarField"));
	EXPECT_NEAR (57, Byte (field.r), 1);
	EXPECT_NEAR (79, Byte (field.g), 1);
	EXPECT_NEAR (191, Byte (field.b), 1);
}

// ---------------------------------------------------------------------------
//  Lisibilite
// ---------------------------------------------------------------------------

// Une teinte hachee tombe ou elle veut : rien ne l'empeche a priori de tomber
// sur un noir ou sur un delave illisible. Ce sont les bornes de saturation et
// de valeur qui l'en empechent, et ce cas les eprouve sur un balayage large
// plutot que sur les seuls noms du jour.
TEST (canvas_style, every_tint_stays_inside_the_readable_band)
{
	for (int i = 0; i < 4096; ++i)
	{
		const std::string name = "type." + std::to_string (i);
		const Rgb tint = NameTint (name);

		const float high = tint.r > tint.g ? (tint.r > tint.b ? tint.r : tint.b)
		                                   : (tint.g > tint.b ? tint.g : tint.b);
		const float low = tint.r < tint.g ? (tint.r < tint.b ? tint.r : tint.b)
		                                  : (tint.g < tint.b ? tint.g : tint.b);

		// La composante la plus forte est la « valeur » de la teinte : jamais
		// sombre au point de disparaitre sur le fond d'un editeur nodal.
		EXPECT_GE (high, 0.71f) << name;
		EXPECT_LE (high, 0.99f) << name;
		// L'ecart entre la plus forte et la plus faible est la « saturation » :
		// jamais assez faible pour rendre un gris.
		EXPECT_GE (high - low, 0.36f * high) << name;
		EXPECT_GE (low, 0.0f) << name;
	}
}

TEST (canvas_style, the_luminance_separates_light_tints_from_dark_ones)
{
	Rgb white;
	white.r = white.g = white.b = 1.0f;
	Rgb black;
	EXPECT_NEAR (1.0f, Luminance (white), 1e-5f);
	EXPECT_NEAR (0.0f, Luminance (black), 1e-5f);

	// Le vert pese plus que le bleu : c'est ce qui fait ecrire un titre en
	// sombre sur un fond vert et en clair sur un fond bleu.
	Rgb green;
	green.g = 1.0f;
	Rgb blue;
	blue.b = 1.0f;
	EXPECT_GT (Luminance (green), Luminance (blue));
}

// ---------------------------------------------------------------------------
//  Separation
// ---------------------------------------------------------------------------

// ⚠ CE CAS PEUT ROUGIR SANS QU'AUCUN CODE DE DESSIN AIT CHANGE : il suffit
// qu'un type ajoute au registre tombe, par hasard, sur la teinte d'un autre. Ce
// n'est pas un defaut du test, c'est sa raison d'etre -- un hachage repartit et
// ne separe pas, et le seul moment ou la collision peut se voir est celui ou le
// type entre au registre. Le plancher est en dessous de la separation mesuree
// aujourd'hui (0,17 sur les neuf types enregistres), assez haut pour attraper
// deux teintes que l'oeil confondrait.
TEST (canvas_style, no_two_registered_types_share_a_tint)
{
	const std::vector<std::string> names = RegisteredTypeNames ();
	ASSERT_GE (names.size (), 2u);

	float worst = 10.0f;
	std::string left;
	std::string right;
	for (std::size_t i = 0; i < names.size (); ++i)
		for (std::size_t k = i + 1; k < names.size (); ++k)
		{
			const float distance = Distance (NameTint (names[i]), NameTint (names[k]));
			if (distance < worst)
			{
				worst = distance;
				left = names[i];
				right = names[k];
			}
		}

	EXPECT_GT (worst, 0.12f) << "teintes trop proches : " << left << " et " << right;
}

// Deux noms VOISINS -- meme prefixe, meme domaine, meme famille de types -- ne
// doivent pas rendre deux teintes voisines. Ce sont ces deux-la que la
// connexion refuse de croiser (voir value_types.h), donc ceux qu'un utilisateur
// a le plus besoin de distinguer d'un coup d'oeil.
TEST (canvas_style, two_neighbouring_type_names_are_told_apart)
{
	const cggraph_nodes::DomainTypes &types = cggraph_nodes::Types ();
	ASSERT_NE (nullptr, types.splayProfile);
	ASSERT_NE (nullptr, types.barProfile);

	const float distance =
	    Distance (NameTint (types.splayProfile->name), NameTint (types.barProfile->name));
	EXPECT_GT (distance, 0.25f) << types.splayProfile->name << " / " << types.barProfile->name;

	// Un seul caractere de difference suffit a changer la teinte : sans cette
	// propriete, une famille de types nommes en serie serait toute de la meme
	// couleur.
	EXPECT_GT (Distance (NameTint (std::string ("nodes.Field1")),
	                     NameTint (std::string ("nodes.Field2"))),
	           0.25f);
}
