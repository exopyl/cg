#pragma once
//
//  Teintes du canvas -- DERIVEES D'UN NOM, sans table a tenir.
//
// Dans un editeur nodal, la couleur d'un port dit le TYPE DE DONNEE qui y
// transite, et la couleur d'un en-tete dit la FAMILLE du noeud. Les deux
// informations existent deja dans le depot -- TypeDesc::name pour l'un,
// CatalogEntry::category pour l'autre --, et aucune des deux n'a de couleur
// attachee. Une table « nom -> couleur » aurait ete un second endroit a tenir
// a jour : elle aurait rendu GRIS le premier type ajoute apres elle,
// c'est-a-dire rate exactement les types que personne n'a ecrits. La teinte
// est donc HACHEE depuis le nom : tout nom en a une, sans qu'une ligne soit
// ajoutee ici.
//
// ⚠ CE QUE CE PROCEDE NE GARANTIT PAS. Deux noms peuvent tomber sur des
// teintes voisines -- un hachage repartit, il ne separe pas. La separation
// mesuree sur les noms REELLEMENT enregistres est donnee par la suite de tests
// (distance RGB minimale), et la couleur n'est jamais le SEUL porteur de
// l'identite : le libelle du port est ecrit a cote, en toutes lettres.
//
// ⚠ CE FICHIER NE CONNAIT NI ImGui NI LE NODE EDITOR, et c'est delibere : il
// est ainsi le seul morceau du canvas qu'une suite de tests puisse exercer.
// `TU` ne lie pas cggraph_canvas -- la cible n'est batie que sous
// ENABLE_CGGRAPH_BOILERPLATE --, mais il peut INCLURE cet en-tete, dont tout est
// `inline` et sans dependance. Ce qui suit est donc couvert en CI ; le dessin
// qui s'en sert ne l'est pas, et le canvas le dit deja a son endroit.
//
#include <cstddef>
#include <cstdint>
#include <string>

namespace cggraph_canvas
{

// Composantes lineaires dans [0, 1]. Volontairement pas ImVec4 : ce fichier ne
// doit pas tirer ImGui, faute de quoi il cesserait d'etre testable.
struct Rgb
{
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
};

// FNV-1a 32 bits. Choisi pour trois proprietes, dans cet ordre : il tient en
// six lignes, il ne depend d'aucune bibliotheque, et il rend le MEME entier
// partout -- l'arithmetique est entiere et non signee, donc ni la largeur de
// `char` ni l'ordre des octets ne l'influencent. La teinte d'un type doit etre
// la meme sur l'hote natif et sur l'hote web ; un hachage flottant, ou lisant
// un `char` signe, ne le garantirait pas.
inline std::uint32_t HashName (const char *text, std::size_t length)
{
	std::uint32_t hash = 2166136261u;
	for (std::size_t i = 0; i < length; ++i)
	{
		hash ^= static_cast<std::uint32_t> (static_cast<unsigned char> (text[i]));
		hash *= 16777619u;
	}
	return hash;
}

inline std::uint32_t HashName (const std::string &text)
{
	return HashName (text.c_str (), text.size ());
}

// h dans [0, 1), s et v dans [0, 1].
inline Rgb HsvToRgb (float h, float s, float v)
{
	const float sector = h * 6.0f;
	const int index = static_cast<int> (sector) % 6;
	const float f = sector - static_cast<float> (static_cast<int> (sector));
	const float p = v * (1.0f - s);
	const float q = v * (1.0f - s * f);
	const float t = v * (1.0f - s * (1.0f - f));

	Rgb color;
	switch (index)
	{
	case 0: color.r = v; color.g = t; color.b = p; break;
	case 1: color.r = q; color.g = v; color.b = p; break;
	case 2: color.r = p; color.g = v; color.b = t; break;
	case 3: color.r = p; color.g = q; color.b = v; break;
	case 4: color.r = t; color.g = p; color.b = v; break;
	default: color.r = v; color.g = p; color.b = q; break;
	}
	return color;
}

// La TEINTE d'un nom. Le hachage sert les trois canaux, et pas seulement la
// teinte angulaire : deux noms tombes sur des angles voisins gardent alors une
// chance d'etre separes par la saturation ou la valeur. Les bornes -- S dans
// [0,52 ; 0,78], V dans [0,72 ; 0,98] -- excluent le delave et le sombre, qui
// sont illisibles sur le fond d'un editeur nodal.
//
// Les decalages 13 et 22 prennent des tranches de bits DISJOINTES de celle que
// consomme l'angle : sans cela les trois canaux varieraient ensemble et le
// second tour de separation ne servirait a rien.
inline Rgb NameTint (const char *text, std::size_t length)
{
	const std::uint32_t hash = HashName (text, length);
	const float hue = static_cast<float> (hash % 3600u) / 3600.0f;
	const float saturation = 0.52f + static_cast<float> ((hash >> 13) & 0xffu) / 255.0f * 0.26f;
	const float value = 0.72f + static_cast<float> ((hash >> 22) & 0xffu) / 255.0f * 0.26f;
	return HsvToRgb (hue, saturation, value);
}

inline Rgb NameTint (const std::string &text)
{
	return NameTint (text.c_str (), text.size ());
}

// Luminance perceptuelle approchee (Rec. 601). Elle sert a decider si le titre
// d'un en-tete s'ecrit en clair ou en sombre : une teinte hachee peut tomber
// n'importe ou, et un titre blanc sur jaune ne se lit pas.
inline float Luminance (const Rgb &color)
{
	return 0.299f * color.r + 0.587f * color.g + 0.114f * color.b;
}

inline Rgb Scaled (const Rgb &color, float factor)
{
	Rgb result;
	result.r = color.r * factor;
	result.g = color.g * factor;
	result.b = color.b * factor;
	return result;
}

} // namespace cggraph_canvas
