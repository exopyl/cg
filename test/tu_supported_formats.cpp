// ---------------------------------------------------------------------------
//  LE MECANISME ANTI-DIVERGENCE DES DEUX CATALOGUES DE FORMATS
// ---------------------------------------------------------------------------
//
// peles/core/file_catalog.{h,cpp} est la version DETYPEE du catalogue de
// sinaia/SupportedFormats.h : meme contenu, sans wxString ni wxChar. Deux
// listes qui decrivent la meme chose finissent par diverger, et la divergence
// est SILENCIEUSE -- un format ajoute d'un cote s'ouvre, de l'autre il
// n'apparait meme pas dans la liste. Ce fichier fait echouer la CI ce jour-la.
//
// ⚠ IL INCLUT L'EN-TETE DE sinaia SANS LE MODIFIER. Le perimetre interdit
// d'ECRIRE dans sinaia/, pas de le LIRE : c'est precisement parce que la source
// historique reste intouchee que la comparaison a un sens.
//
// ⚠ LES DEUX CATALOGUES SONT LUS SOUS LES MEMES MACROS -- MAIS PAS DANS LA MEME
// UNITE DE TRADUCTION, ET L'ARGUMENT « donc identiques ici » SERAIT FAUX.
// `sinaia::SupportedFormats()` est `inline` et compile DANS CE FICHIER, tandis
// que `peles::core::SupportedFormats()` est compile dans la bibliotheque
// peles_core. Ce qui rend les gardes egales n'est pas l'unite de traduction :
// c'est que les deux cibles heritent les MEMES definitions, publiees en PUBLIC
// par cgmesh. Un `target_compile_definitions(peles_core PRIVATE CG_HAS_...)`
// romprait cette egalite SANS ALERTE, et ce test comparerait alors deux
// catalogues compiles sous des gardes differentes.
//
// ⚠ ET LE TEST NE VOIT QUE LES FAMILLES QUE LA CONFIGURATION BATIE OUVRE.
// Quatre familles sont conditionnelles -- CG_HAS_OPENNURBS, CG_HAS_OCCT,
// CG_HAS_ZLIB, CG_HAS_LIB3MF : sous une configuration qui n'en active qu'une,
// une divergence portant sur les trois autres est invisible PAR CONSTRUCTION.
// La couverture annoncee vaut donc pour les gardes ouvertes, et pour elles
// seules. La liste effectivement ouverte est imprimee par le premier test.
//
// ⚠ LE FICHIER N'EST PLUS VIDE HORS DE SA CONFIGURATION. Sous ENABLE_SINAIA=OFF
// il ne compilait A RIEN : ctest etait vert, aucun test n'etait decouvert, et
// l'absence du mecanisme anti-divergence ne produisait AUCUN SIGNAL. Le `#else`
// ci-dessous rend un GTEST_SKIP qui nomme la configuration manquante.

#include <gtest/gtest.h>

#ifdef CG_PELES_SINAIA_FORMATS

	// peles, detype.
	#include "file_catalog.h"

	// sinaia, type wxString/wxChar. LECTURE SEULE.
	#include "SupportedFormats.h"

	#include <cstdio>
	#include <set>
	#include <string>

namespace
{

// Les extensions du catalogue sont ASCII par construction : ce sont des
// suffixes de format de fichier. La conversion le VERIFIE plutot que de le
// supposer -- un caractere hors ASCII rendrait la comparaison fausse en
// silence.
std::string Narrow (const wxChar* text)
{
	std::string narrow;
	for (const wxChar* cursor = text; cursor != nullptr && *cursor != 0; ++cursor)
	{
		const unsigned long code = (unsigned long)*cursor;
		EXPECT_LT (code, 128ul) << "extension non ASCII dans sinaia::SupportedFormats()";
		narrow.push_back ((char)code);
	}
	return narrow;
}

std::set<std::string> SinaiaExtensions ()
{
	std::set<std::string> extensions;
	for (const sinaia::Format3D& format : sinaia::SupportedFormats ())
	{
		for (const wxChar* extension : format.exts)
			extensions.insert (Narrow (extension));
	}
	return extensions;
}

std::set<std::string> PelesExtensions ()
{
	std::set<std::string> extensions;
	for (const peles::core::Format3D& format : peles::core::SupportedFormats ())
	{
		for (const std::string& extension : format.extensions)
			extensions.insert (extension);
	}
	return extensions;
}

std::string Join (const std::set<std::string>& values)
{
	std::string text;
	for (const std::string& value : values)
	{
		if (!text.empty ())
			text += ", ";
		text += value;
	}
	return text.empty () ? std::string ("(aucune)") : text;
}

std::set<std::string> Missing (const std::set<std::string>& from, const std::set<std::string>& in)
{
	std::set<std::string> missing;
	for (const std::string& value : from)
	{
		if (in.find (value) == in.end ())
			missing.insert (value);
	}
	return missing;
}

} // namespace

// LE CRITERE : les deux ENSEMBLES d'extensions sont EXACTEMENT egaux.
//
// ⚠ EGAUX, PAS « compatibles ». Une extension presente d'un seul cote est une
// divergence quel qu'en soit le sens : dans sinaia seule, peles refuse un
// fichier que l'autre ouvre ; dans peles seul, peles annonce un format que
// l'importateur ne traitera pas.
TEST (SupportedFormats, PelesEtSinaiaDeclarentLesMemesExtensions)
{
	// ⚠ LA LIMITE DU TEST EST DANS LE TEST, PAS SEULEMENT DANS UN COMMENTAIRE.
	// Les familles conditionnelles absentes de cette construction ne sont PAS
	// comparees : une divergence portant sur elles est invisible ici. L'imprimer
	// est la seule facon qu'un lecteur du journal ait de savoir ce que le
	// « PASSED » recouvre.
	std::string gardes;
	#ifdef CG_HAS_OPENNURBS
	gardes += "CG_HAS_OPENNURBS ";
	#endif
	#ifdef CG_HAS_OCCT
	gardes += "CG_HAS_OCCT ";
	#endif
	#ifdef CG_HAS_ZLIB
	gardes += "CG_HAS_ZLIB ";
	#endif
	#ifdef CG_HAS_LIB3MF
	gardes += "CG_HAS_LIB3MF ";
	#endif
	std::printf ("[          ] gardes ouvertes dans cette construction : %s\n",
	             gardes.empty () ? "(aucune)" : gardes.c_str ());
	std::printf ("[          ] les familles dont la garde est fermee ne sont PAS comparees.\n");

	const std::set<std::string> sinaiaSet = SinaiaExtensions ();
	const std::set<std::string> pelesSet  = PelesExtensions ();

	// Le message nomme LES DEUX differences : « les ensembles different » sans
	// dire lesquelles obligerait a relire les deux catalogues a la main.
	EXPECT_EQ (pelesSet, sinaiaSet)
	    << "absentes de peles : " << Join (Missing (sinaiaSet, pelesSet))
	    << " | absentes de sinaia : " << Join (Missing (pelesSet, sinaiaSet));
}

// LA FAMILLE D'ICONE SUIT, ELLE AUSSI. sinaia rend un index dans une liste
// d'images, peles une classe RCSS : ce sont deux ecritures de la meme
// information, et rien d'autre ne les tiendrait ensemble.
//
// ⚠ LE CATALOGUE DE sinaia EST PARCOURU A LA MAIN AU LIEU D'APPELER
// IconForExtension, ET C'EST DELIBERE : cette fonction prend un wxString, donc
// l'appeler ferait LIER wxWidgets a la cible de test -- avec ses DLL a deposer
// a cote de l'executable, sans quoi AUCUN test ne demarrerait. Ce qui est
// compare ici est la DONNEE des deux catalogues, et elle n'a besoin d'aucune
// bibliotheque.
TEST (SupportedFormats, LaClasseRcssSuitLIconeDeSinaia)
{
	for (const peles::core::Format3D& format : peles::core::SupportedFormats ())
	{
		for (const std::string& extension : format.extensions)
		{
			bool found = false;
			for (const sinaia::Format3D& reference : sinaia::SupportedFormats ())
			{
				for (const wxChar* candidate : reference.exts)
				{
					if (Narrow (candidate) != extension)
						continue;

					found                = true;
					const char* expected = "kind-generic";
					if (reference.icon == sinaia::FormatIcon::Obj)
						expected = "kind-obj";
					else if (reference.icon == sinaia::FormatIcon::Stl)
						expected = "kind-stl";

					EXPECT_EQ (peles::core::KindClassForExtension (extension),
					           std::string (expected))
					    << "extension : " << extension;
				}
			}
			EXPECT_TRUE (found) << "extension inconnue de sinaia : " << extension;
		}
	}
}

#else // CG_PELES_SINAIA_FORMATS

// ---------------------------------------------------------------------------
//  L'ABSENCE DU MECANISME EST ELLE-MEME UN RESULTAT
// ---------------------------------------------------------------------------
//
// ⚠ UN TEST QUI DISPARAIT NE PRODUIT AUCUN SIGNAL. Sous ENABLE_SINAIA=OFF, ce
// fichier compilait a RIEN : ctest rendait vert, aucun test n'etait decouvert,
// et rien ne distinguait « les deux catalogues concordent » de « personne ne les
// a compares ». C'est la distinction entre PASS et SKIP, et elle vaut ici autant
// qu'ailleurs.
//
// ⚠ COMPILE INCONDITIONNELLEMENT, et c'est le point : il faut que le rapport de
// ctest contienne une ligne pour ce mecanisme dans TOUTES les configurations.
TEST (SupportedFormats, MecanismeAntiDivergenceNonCompile)
{
	GTEST_SKIP () << "les deux catalogues de formats n'ont PAS ete compares : ce test exige "
	                 "ENABLE_SINAIA=ON *et* ENABLE_PELES=ON (le premier pour que "
	                 "sinaia/SupportedFormats.h soit incluable -- donc wxWidgets --, le second "
	                 "pour peles_core). Aucune configuration de CI ne reunit les deux a ce jour ; "
	                 "seule une construction locale le fait.";
}

#endif // CG_PELES_SINAIA_FORMATS
