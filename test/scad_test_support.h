#pragma once

// Accès à OpenSCAD pour les tests : recherche de l'exécutable, et décision
// d'exécuter, de sauter ou de faire ÉCHOUER un test qui en a besoin.
//
// CG_REQUIRE_OPENSCAD : OpenSCAD est EXIGÉ dès que la variable est non vide et
// différente de "0". Une machine qui l'exige (la CI Linux) voit alors un
// OpenSCAD introuvable comme un échec, avec les emplacements essayés, au lieu
// de sauter en silence les tests qui le couvrent. La règle penche vers
// l'exigence : une valeur inattendue ("true", "yes") exige plutôt que de
// laisser passer une CI qui ne teste plus rien. Variable absente, vide ou "0" :
// les tests sont sautés (GTEST_SKIP), comme sur un poste sans OpenSCAD.

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../src/cgmesh/external_process.h"
#include "../src/cgmesh/import_scad.h"

namespace scad_test {

constexpr const char* kRequireVariable = "CG_REQUIRE_OPENSCAD";

enum class Action { Run, Skip, Fail };

inline bool IsRequired (const std::string& requireValue)
{
	return !requireValue.empty () && requireValue != "0";
}

// Fonction pure : ne lit ni l'environnement ni le disque.
inline Action Decide (bool openScadFound, const std::string& requireValue)
{
	if (openScadFound)
		return Action::Run;
	return IsRequired (requireValue) ? Action::Fail : Action::Skip;
}

// Recherche des tests : CG_OPENSCAD, PATH, puis EXTERN_OPENSCAD de la
// configuration (CG_TEST_OPENSCAD_HINT).
inline cgmesh::scad::ExecutableSearch Search ()
{
	cgmesh::scad::ExecutableSearch search;
#ifdef CG_TEST_OPENSCAD_HINT
	search.fallbacks.push_back (std::filesystem::u8path (CG_TEST_OPENSCAD_HINT));
#endif
	return search;
}

inline const cgmesh::scad::ExecutableLookup& Lookup ()
{
	static const cgmesh::scad::ExecutableLookup lookup = cgmesh::scad::FindOpenScad (Search ());
	return lookup;
}

inline std::string Searched ()
{
	std::string out;
	for (const std::string& s : Lookup ().searched)
		out += "\n  - " + s;
	return out;
}

} // namespace scad_test

// À placer en tête d'un TEST qui lance OpenSCAD.
#define REQUIRE_OPENSCAD()                                                                           \
	do {                                                                                             \
		const std::string requireValue_ =                                                            \
			cgmesh::process::GetEnvironmentVariableUtf8 (scad_test::kRequireVariable);              \
		switch (scad_test::Decide (!scad_test::Lookup ().executable.empty (), requireValue_))        \
		{                                                                                            \
			case scad_test::Action::Run:  break;                                                     \
			case scad_test::Action::Skip:                                                            \
				GTEST_SKIP () << "OpenSCAD introuvable ; emplacements essayes :" << scad_test::Searched (); \
			case scad_test::Action::Fail:                                                            \
				FAIL () << "OpenSCAD introuvable alors que " << scad_test::kRequireVariable << "="       \
				        << requireValue_ << " l'exige ; emplacements essayes :" << scad_test::Searched (); \
		}                                                                                            \
	} while (0)
