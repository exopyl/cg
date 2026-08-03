#!/bin/sh

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

export BUILD_DIR=build_coverage

echo "Configuration ... "
cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=On -DENABLE_OPENNURBS=On -DENABLE_OCCT=On -DENABLE_POISSON=On
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
REPO_ROOT=$PWD
TU_RUN_DIR=$PWD/${BUILD_DIR}/test/test-run
( cd ${TU_RUN_DIR} && LLVM_PROFILE_FILE="${REPO_ROOT}/code-%p.profraw" ${CXX_EXECUTABLE} )

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

echo "Launching 'llvm-cov export' ..."
llvm-cov-18 export ${CXX_EXECUTABLE} \
  -format=lcov -instr-profile=$PWD/code.profdata > $PWD/coverage.info
echo "=> DONE"

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
