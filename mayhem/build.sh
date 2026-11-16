#!/usr/bin/env bash
#
# mayhem/build.sh — builds the Mudlet units that parse untrusted input, as libFuzzer harnesses
# (sanitized) plus upstream's own QtTest cases for the same units (normal flags) for mayhem/test.sh.
# Not the full Qt application: the units are compiled straight from src/ via mayhem/CMakeLists.txt.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

TARGETS="xml_import mxp release_feed encoding uri_text unzip"

# Leak detection stays off (SPEC §6.2): mayhem/lsan_off.cc is linked into every fuzz and -standalone binary.
# 1+2) Units + harnesses, sanitized, DWARF 3. <t> links libFuzzer, <t>-standalone the run-once driver.
cmake -S mayhem -B build-mayhem-fuzz -G Ninja \
    -DCMAKE_BUILD_TYPE= \
    -DMAYHEM_MODE=fuzz -DMAYHEM_ENGINE="$LIB_FUZZING_ENGINE" -DMAYHEM_STANDALONE_MAIN="$STANDALONE_FUZZ_MAIN" \
    -DMAYHEM_LSAN_OFF_SRC="$SRC/mayhem/lsan_off.cc" \
    -DCMAKE_C_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O2 -fsanitize=fuzzer-no-link" \
    -DCMAKE_CXX_FLAGS="$SANITIZER_FLAGS $DEBUG_FLAGS -O2 -fsanitize=fuzzer-no-link" \
    -DCMAKE_EXE_LINKER_FLAGS="$SANITIZER_FLAGS"
cmake --build build-mayhem-fuzz -j"$MAYHEM_JOBS"
for t in $TARGETS; do
    cp -f "build-mayhem-fuzz/bin/$t" "/mayhem/$t"
    cp -f "build-mayhem-fuzz/bin/$t-standalone" "/mayhem/$t-standalone"
done

# 3) Upstream's QtTest cases for these units, normal flags, separate tree.
cmake -S mayhem -B build-mayhem-tests -G Ninja \
    -DCMAKE_BUILD_TYPE= \
    -DMAYHEM_MODE=tests \
    -DCMAKE_C_FLAGS="-O1 $COVERAGE_FLAGS" -DCMAKE_CXX_FLAGS="-O1 $COVERAGE_FLAGS" \
    -DCMAKE_EXE_LINKER_FLAGS="$COVERAGE_FLAGS"
cmake --build build-mayhem-tests -j"$MAYHEM_JOBS"
