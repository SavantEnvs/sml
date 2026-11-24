#!/usr/bin/env bash
# mayhem/test.sh — RUN sml's upstream functional tests (built by mayhem/build.sh into build-tests/).
# Each upstream test binary runs its `expect(...)` assertions and exits -1 with the failing
# expression on the first failure. A test PASSES only if it exits 0 AND printed the
# SML_TEST_RAN marker (from mayhem/sml_test_marker.hpp) at normal exit.
# The compile-failure tests (test/ft/errors, WILL_FAIL) need a compiler, so they are not run here.
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

B="$SRC/build-tests/test"
mapfile -t tests < <(find "$B/ft" "$B/ut" "$B/unit" -maxdepth 1 -type f -name 'test_*' -perm -u+x 2>/dev/null | sort)
if [ "${#tests[@]}" -lt 30 ]; then
  echo "test.sh: expected the upstream test binaries under $B (found ${#tests[@]}); run mayhem/build.sh" >&2
  emit_ctrf "sml-expect" 0 1
  exit 1
fi

passed=0 failed=0
for t in "${tests[@]}"; do
  out="$(timeout 120 "$t" 2>&1)"; rc=$?
  if [ "$rc" -eq 0 ] && grep -q '^SML_TEST_RAN$' <<<"$out"; then
    passed=$((passed + 1))
  else
    failed=$((failed + 1))
    echo "FAIL: ${t#$SRC/} (exit $rc)"; printf '%s\n' "$out" | grep -v '^SML_TEST_RAN$' | head -5
  fi
done
echo "sml tests: $passed passed, $failed failed"
emit_ctrf "sml-expect" "$passed" "$failed"
