#!/bin/sh

export CC=/usr/bin/clang
export CXX=/usr/bin/clang++

export BUILD_DIR=build_coverage

echo "Configuration ... "
cmake -B ${BUILD_DIR} -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=On
echo "=> DONE"

echo "Compilation ... "
cmake --build ${BUILD_DIR}
echo "=> DONE"

echo "Cleaning old possible data ... "
rm -f *profraw *gcov *profdata
echo "=> DONE"

CXX_EXECUTABLE=$PWD/${BUILD_DIR}/test/tu_cgmath
#echo ${CXX_EXECUTABLE}
LLVM_PROFILE_FILE="code-%p.profraw" ${CXX_EXECUTABLE}

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
llvm-cov-18 show ${CXX_EXECUTABLE} -instr-profile=$PWD/code.profdata > $PWD/coverage.txt
echo "=> DONE"
