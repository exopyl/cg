#include <gtest/gtest.h>

#if defined(_MSC_VER)
#include <crtdbg.h>
#include <windows.h>
#endif

// PAS DE BOITE DE DIALOGUE MODALE, jamais.
//
// Sous MSVC en Debug, un controle /RTC1 (variable lue avant initialisation), un
// `assert` ou une erreur du CRT ouvre une FENETRE et attend un clic. Dans un
// lanceur de tests, c'est le pire comportement possible : la suite ne rend pas
// la main, aucun rapport ne sort, et en CI le job se fait tuer sur delai
// d'attente sans le moindre indice de la cause.
//
// C'est exactement ce qui s'est produit en reactivant les tests caches sous
// `#if 0` de tu_cgmesh_set_lines.cpp : trois variables non initialisees dans
// set_lines_new_method.cpp, et une boite bloquante au lieu d'un echec lisible.
//
// On redirige donc tous les rapports du CRT vers stderr, et l'on desarme les
// boites d'erreur systeme. Un defaut devient alors une trace suivie d'une
// sortie non nulle -- ce que gtest et ctest savent lire.
static void RouteCrtReportsToStderr ()
{
#if defined(_MSC_VER)
	for (int report : { _CRT_WARN, _CRT_ERROR, _CRT_ASSERT })
	{
		_CrtSetReportMode (report, _CRTDBG_MODE_FILE);
		_CrtSetReportFile (report, _CRTDBG_FILE_STDERR);
	}
	SetErrorMode (SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
	_set_abort_behavior (0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
}

int main(int argc, char **argv) {
  RouteCrtReportsToStderr ();

  ::testing::InitGoogleTest(&argc, argv);

  //std::cout << "TEST_DIR '" << TEST_DIR << "'" << std::endl;
  for (int i = 1; i < argc; ++i) {
      std::cout << "Input[" << i << "]: "<< argv[i] << std::endl;
  }

  return RUN_ALL_TESTS();
}
