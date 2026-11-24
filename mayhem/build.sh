#!/usr/bin/env bash
# mayhem/build.sh — build sml's fuzz harnesses and its upstream test suite.
# sml is header-only (include/boost/sml.hpp): the "project" is instrumented by compiling it
# inside each harness TU with $SANITIZER_FLAGS $DEBUG_FLAGS. Fully offline, re-runnable.
set -euo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
OBJ="$SRC/build-mayhem"          # gitignored by upstream's /build*/
mkdir -p "$OBJ"

# 1+2) Harnesses: libFuzzer binary + standalone reproducer each.
$CC  $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$OBJ/standalone_main.o"
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$OBJ/lsan_off.o"
CXXFLAGS_FUZZ="-std=c++17 -O1 -I$SRC/include $SANITIZER_FLAGS $DEBUG_FLAGS"
pids=()
for h in fuzz_queue fuzz_defer fuzz_composite fuzz_dispatch; do
  (
    # fuzzer object carries SanCov (fuzzer-no-link) so libFuzzer sees coverage of sml.hpp;
    # the standalone object is compiled without it so it links with no libFuzzer runtime.
    $CXX $CXXFLAGS_FUZZ -fsanitize=fuzzer-no-link -c "$SRC/mayhem/harnesses/$h.cpp" -o "$OBJ/$h.o"
    $CXX $CXXFLAGS_FUZZ -c "$SRC/mayhem/harnesses/$h.cpp" -o "$OBJ/$h-standalone.o"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$OBJ/$h.o" "$OBJ/lsan_off.o" -o "$SRC/$h"
    $CXX $SANITIZER_FLAGS $DEBUG_FLAGS "$OBJ/$h-standalone.o" "$OBJ/standalone_main.o" "$OBJ/lsan_off.o" -o "$SRC/$h-standalone"
  ) & pids+=($!)
done
for p in "${pids[@]}"; do wait "$p"; done

# 3) Upstream test suite (test/ft, test/ut, test/unit) with NORMAL flags, via upstream CMake.
#    The only addition is a force-included marker header (mayhem/sml_test_marker.hpp) that prints
#    SML_TEST_RAN at normal exit, so test.sh can detect a neutered test binary.
cmake -S "$SRC" -B "$SRC/build-tests" -DCMAKE_BUILD_TYPE=Release -DSML_BUILD_TESTS=ON \
      -DCMAKE_CXX_FLAGS="-include $SRC/mayhem/sml_test_marker.hpp $COVERAGE_FLAGS" \
      -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build "$SRC/build-tests" -j"$MAYHEM_JOBS"
