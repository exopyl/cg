# ---------------------------------------------------------------------------
#  UN VIDAGE, SON ORACLE, ET LA COMPARAISON OCTET POUR OCTET
# ---------------------------------------------------------------------------
#
# Lance `${PELES_EXE} ${MODE} ${FIXTURE}`, capture sa sortie standard dans
# ${OUTPUT}, puis la compare a ${EXPECTED}.
#
# ⚠ SCRIPT PLUTOT QU'UN `add_test` DIRECT, PARCE QU'UN TEST CTEST NE PEUT PAS
# REDIRIGER : la sortie a comparer est celle de stdout, et ctest ne l'ecrit pas
# dans un fichier. execute_process, lui, le fait -- et sans passer par un shell,
# donc sans dependre de cmd.exe ni de sh.
#
# ⚠ `compare_files` ET NON UNE COMPARAISON DE CHAINES : la propriete verifiee est
# l'egalite OCTET POUR OCTET, fins de ligne comprises. peles bascule stdout en
# mode BINAIRE, donc il ecrit en LF sur les deux plateformes, et les deux fichiers
# attendus sont marques `-text` dans test/data/peles/.gitattributes pour que git
# ne les reecrive pas en CRLF a l'extraction. Une comparaison ligne a ligne
# laisserait passer exactement la regression que ces deux mesures empechent.

foreach (_required PELES_EXE MODE FIXTURE EXPECTED OUTPUT)
	if (NOT DEFINED ${_required})
		message (FATAL_ERROR "CompareDumpOracle.cmake : ${_required} n'est pas defini.")
	endif ()
endforeach ()

get_filename_component (_output_dir "${OUTPUT}" DIRECTORY)
file (MAKE_DIRECTORY "${_output_dir}")

# ⚠ LE REPERTOIRE DE TRAVAIL FAIT PARTIE DE LA MESURE POUR --dump-info, et pour
# lui seul : la ligne « Path » de la table de proprietes rend le chemin TEL QU'IL
# A ETE DONNE. Un chemin absolu y ferait un oracle propre a une machine ; un
# chemin relatif lance depuis la racine du depot rend les memes octets partout.
# --dump-tree et --dump-files n'en ont pas besoin -- ils n'impriment que le
# dernier composant -- donc la variable reste facultative.
if (DEFINED WORKDIR)
	set (_workdir_argument WORKING_DIRECTORY "${WORKDIR}")
else ()
	set (_workdir_argument)
endif ()

execute_process (
	COMMAND "${PELES_EXE}" "${MODE}" "${FIXTURE}"
	${_workdir_argument}
	OUTPUT_FILE "${OUTPUT}"
	ERROR_VARIABLE _stderr
	RESULT_VARIABLE _status)

if (NOT _status EQUAL 0)
	message (FATAL_ERROR
		"peles ${MODE} a rendu ${_status} au lieu de 0.\n"
		"stderr : ${_stderr}")
endif ()

execute_process (
	COMMAND "${CMAKE_COMMAND}" -E compare_files "${OUTPUT}" "${EXPECTED}"
	RESULT_VARIABLE _differ)

if (NOT _differ EQUAL 0)
	# Le diff n'est pas imprime ici : `compare_files` ne le produit pas, et le
	# message nomme les DEUX fichiers pour que la comparaison soit refaisable a
	# la main. Un vidage tronque a l'ecran serait moins utile que son chemin.
	message (FATAL_ERROR
		"peles ${MODE} differe de son oracle.\n"
		"  obtenu  : ${OUTPUT}\n"
		"  attendu : ${EXPECTED}")
endif ()
