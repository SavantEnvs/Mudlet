#!/usr/bin/env bash
#
# mayhem/test.sh — runs upstream's own QtTest cases for the units the harnesses fuzz (MXP, XML import,
# entity/encoding/untrusted-text helpers, updater feed/semver), built by mayhem/build.sh in build-mayhem-tests/.
# Every case asserts values (QCOMPARE/QVERIFY); the counts come from each case's own "Totals:" line, and a
# case that prints none (crashed, or neutered to exit(0)) counts as a failure.
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

BIN="$SRC/build-mayhem-tests/bin"
GROUPED="TMxpTagParserTest TMxpSendTagHandlerTest TMxpLinkTagHandlerTest TMxpEntityTagHandlerTest
  TMxpVersionTagTest TMxpFormattingTagsTest TMxpCustomElementTagHandlerTest TMxpModeSecurityTest
  TMxpEdgeCasesTest TMxpElementDefinitionHandlerTest TMxpFrameDestTagHandlerTest
  TEntityResolverTest TEntityHandlerTest TEncodingHelperTest UntrustedTextTest XmlImportReaderTest LuaLiteralTest
  TAreaSpanIndexTest TAreaZLevelIndexTest TAreaGridIndexTest EventLoopPumpTest"
STANDALONE="SemVerTest ReleaseChecksumPairingTest ReleasePlatformAssetTest ReleaseChangelogSpanTest
  MudletProxyStyleDockTitleTest LayoutMirrorTest CrashReportUploadTest CMakeListsConsistencyTest LuaFunctionListTest
  DesignColourLiteralTest XdgRecipeConsistencyTest IrcMessageFormatterTest IrcMessageComposerTest LuaWriteBarrierTest"
# upstream's bash ctests (test/CMakeLists.txt): offline, pass/fail is the exit status, 77 is a skip
SCRIPTS="ReleaseChecksumsTest:release-checksums-test.sh ReleaseTagVersionTest:release-tag-version-test.sh
  ReleaseNotesFitTest:release-notes-fit-test.sh MilestoneResolutionTest:milestone-resolution-test.sh
  LinuxPackagesKeyTest:linux-packages-key-test.sh"

for exe in "$BIN/mudlet_unit_cases" $(for t in $STANDALONE; do echo "$BIN/$t"; done); do
  [ -x "$exe" ] || { echo "test runner $exe is missing - mayhem/build.sh should have built it" >&2; emit_ctrf "qttest" 0 1 0; exit 1; }
done

export QT_QPA_PLATFORM=offscreen
export XDG_CONFIG_HOME="${TMPDIR:-/tmp}/mayhem-test-config"
mkdir -p "$XDG_CONFIG_HOME"

passed=0; failed=0; skipped=0

# run <name> <command...>: add the Qt Test "Totals: P passed, F failed, S skipped" line to the counts
run() {
  local name="$1"; shift
  local out totals p f s
  out="$(timeout 600 "$@" 2>&1)"; local rc=$?
  totals="$(printf '%s\n' "$out" | grep -E '^Totals: [0-9]+ passed, [0-9]+ failed, [0-9]+ skipped' | tail -n 1)"
  if [ -z "$totals" ]; then
    echo "FAIL $name: no Qt Test totals line (rc=$rc)" >&2
    printf '%s\n' "$out" | tail -n 20 >&2
    failed=$((failed + 1)); return
  fi
  p="$(sed -E 's/^Totals: ([0-9]+) passed.*/\1/' <<<"$totals")"
  f="$(sed -E 's/^Totals: [0-9]+ passed, ([0-9]+) failed.*/\1/' <<<"$totals")"
  s="$(sed -E 's/^Totals: [0-9]+ passed, [0-9]+ failed, ([0-9]+) skipped.*/\1/' <<<"$totals")"
  if [ "$p" -eq 0 ] && [ "$f" -eq 0 ]; then
    echo "FAIL $name: ran no test functions" >&2; failed=$((failed + 1)); return
  fi
  if [ "$rc" -ne 0 ] && [ "$f" -eq 0 ]; then
    echo "FAIL $name: exit status $rc with no reported failure" >&2
    printf '%s\n' "$out" | tail -n 20 >&2
    f=1
  fi
  [ "$f" -gt 0 ] && { echo "FAIL $name: $totals" >&2; printf '%s\n' "$out" | grep -E '^FAIL!|Actual|Expected|Loc:' | head -n 20 >&2; }
  echo "$name: $totals"
  passed=$((passed + p)); failed=$((failed + f)); skipped=$((skipped + s))
}

# run_script <name> <script>: one case; exit 0 passes, 77 is ctest's SKIP_RETURN_CODE, anything else fails
run_script() {
  local name="$1" script="$SRC/test/ci/$2" out rc
  [ -f "$script" ] || { echo "FAIL $name: $script is missing" >&2; failed=$((failed + 1)); return; }
  out="$(timeout 600 bash "$script" 2>&1)"; rc=$?
  case "$rc" in
    0) echo "$name: passed"; passed=$((passed + 1)) ;;
    77) echo "$name: skipped"; printf '%s\n' "$out" | tail -n 3; skipped=$((skipped + 1)) ;;
    *) echo "FAIL $name: exit status $rc" >&2; printf '%s\n' "$out" | tail -n 20 >&2; failed=$((failed + 1)) ;;
  esac
}

# upstream caps these two at 60s: a regression in them hangs rather than fails
for c in $GROUPED; do
  case "$c" in EventLoopPumpTest|TAreaGridIndexTest) run "$c" timeout 60 "$BIN/mudlet_unit_cases" "$c" ;;
    *) run "$c" "$BIN/mudlet_unit_cases" "$c" ;; esac
done
for t in $STANDALONE; do
  case "$t" in IrcMessageComposerTest) run "$t" timeout 60 "$BIN/$t" ;; *) run "$t" "$BIN/$t" ;; esac
done
for e in $SCRIPTS; do run_script "${e%%:*}" "${e#*:}"; done

emit_ctrf "qttest" "$passed" "$failed" "$skipped"
