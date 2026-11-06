#!/bin/sh

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

export BUILD_DIR=build_coverage

echo "Configuration ... "
cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=On -DENABLE_OPENNURBS=On -DENABLE_OCCT=On -DENABLE_POISSON=On -DENABLE_CGRE=On
echo "=> DONE"

echo "Compilation ... "
cmake --build ${BUILD_DIR}
echo "=> DONE"

echo "Cleaning old possible data ... "
rm -f *profraw *gcov *profdata
echo "=> DONE"

CXX_EXECUTABLE=$PWD/${BUILD_DIR}/test/TU
echo ${CXX_EXECUTABLE}
ls -l $PWD/${BUILD_DIR}
ls -l $PWD/${BUILD_DIR}/test

# Lancer TU depuis son repertoire de travail dedie (cf. TU_RUN_DIR dans
# test/CMakeLists.txt) et non depuis la racine : les tests ecrivent leurs sorties
# via des noms relatifs nus, et depuis la racine ils y deversaient 200+ fichiers.
# C'est aussi la seule facon de resoudre leurs entrees "./test/data/...", qui y
# sont copiees au build.
#
# LLVM_PROFILE_FILE reste ABSOLU : le merge plus bas cherche $PWD/code-*.profraw.
#
# Le code de sortie de TU est CONSERVE dans tu_status et rendu par le script a
# la fin : un test en echec fait echouer le script, mais seulement apres la
# production de la couverture, qui reste utile pour diagnostiquer l'echec. Un
# TU absent (build en echec) donne un code non nul par la meme voie.
REPO_ROOT=$PWD
TU_RUN_DIR=$PWD/${BUILD_DIR}/test/test-run
tu_status=0
( cd ${TU_RUN_DIR} && LLVM_PROFILE_FILE="${REPO_ROOT}/code-%p.profraw" ${CXX_EXECUTABLE} ) || tu_status=$?
echo "TU exit status: ${tu_status}"

echo "Launching 'llvm-profdata merge' ..."
llvm-profdata-18 merge -output=$PWD/code.profdata $PWD/code-*.profraw
echo "=> DONE"

echo "Launching 'llvm-cov report' ..."
llvm-cov-18 report ${CXX_EXECUTABLE} \
  -use-color \
  -instr-profile=$PWD/code.profdata \
  -ignore-filename-regex=test/*
echo "=> DONE"

echo "Launching 'llvm-cov show' ..."
llvm-cov-18 show ${CXX_EXECUTABLE} \
  -instr-profile=$PWD/code.profdata > $PWD/coverage.txt
echo "=> DONE"

# coverage.info n'existe que s'il est exploitable : l'export passe par un
# fichier temporaire, renomme seulement si llvm-cov a reussi ET si le resultat
# decrit au moins un fichier source (ligne SF:). Une redirection directe creerait
# coverage.info meme en cas d'echec, et sq.yml conditionne l'envoi a Sonar a sa
# presence.
echo "Launching 'llvm-cov export' ..."
COVERAGE_INFO=$PWD/coverage.info
COVERAGE_TMP=$PWD/coverage.info.tmp
rm -f "${COVERAGE_INFO}" "${COVERAGE_TMP}"
export_status=0
llvm-cov-18 export ${CXX_EXECUTABLE} \
  -format=lcov -instr-profile=$PWD/code.profdata > "${COVERAGE_TMP}" || export_status=$?
if [ "${export_status}" -eq 0 ] && grep -q '^SF:' "${COVERAGE_TMP}"; then
  mv "${COVERAGE_TMP}" "${COVERAGE_INFO}"
  echo "=> DONE"
else
  rm -f "${COVERAGE_TMP}"
  if [ "${export_status}" -eq 0 ]; then
    export_status=1
  fi
  echo "coverage.info non produit (llvm-cov export en echec ou sans ligne SF:)" >&2
fi

#echo "Launching 'gcov export' ..."
#ls -l ${BUILD_DIR}
#echo "---"
#gcovr --sonarqube coverage.xml \
#      --root . \
#      --verbose \
#      ${BUILD_DIR}/
#echo "=> DONE"
#ls -l $PWD
#head -n 30 coverage.xml

echo "coverage DONE"

# Code de sortie : celui de TU s'il a echoue, sinon celui de l'export.
if [ "${tu_status}" -ne 0 ]; then
  echo "TU a echoue (code ${tu_status}) : etapes de couverture executees, echec rendu" >&2
  exit ${tu_status}
fi
exit ${export_status}
