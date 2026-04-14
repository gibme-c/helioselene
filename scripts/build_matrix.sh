#!/usr/bin/env bash
#
# build_matrix.sh — build, test, fuzz, and benchmark the full
# (compiler × compile-time variant) matrix.
#
# Cells: {gcc, clang, msvc} × {default, x64only, noifma, portable} = 12.
# For each cell:
#   1. configure + build (via cmake-build helper script — handles MSVC vcvars)
#   2. run unit tests under --autotune
#   3. run fuzz tests under --autotune
#   4. run benchmark under --autotune AND --init
# Per-cell logs land in logs/matrix-<date>/<compiler>-<variant>/.
# A summary table prints at the end.
#
# Usage:
#   scripts/build_matrix.sh [--skip-fuzz] [--skip-bench] [--cells=<filter>]
#                           [--ecfft] [--cmake-build=<path>]
#
# --skip-fuzz       Skip the per-cell fuzz run.
# --skip-bench      Skip the --init / --autotune benchmark runs.
# --cells=<filter>  Glob filter on cell names, e.g. "gcc-*" or "*-portable".
#                   When --ecfft is set, cell names are prefixed with "ecfft-"
#                   (e.g. "gcc-ecfft-default"); filter accordingly.
# --ecfft           Append -DENABLE_ECFFT=ON to every cell and use distinct
#                   build + log directories so ECFFT=ON results don't collide
#                   with an ECFFT=OFF run in the same day.
# --cmake-build=<p> Override the path to the cmake-build helper script.
#                   Default: $HOME/.claude/skills/cmake-build/scripts/cmake_build.sh
#
# Exit code: 0 if all selected cells pass build+test (and fuzz unless skipped);
#            non-zero if any cell fails.

set -uo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

SKIP_FUZZ=0
SKIP_BENCH=0
USE_ECFFT=0
CELL_FILTER="*"
CMAKE_BUILD="${HOME}/.claude/skills/cmake-build/scripts/cmake_build.sh"

for arg in "$@"; do
  case "$arg" in
    --skip-fuzz)        SKIP_FUZZ=1 ;;
    --skip-bench)       SKIP_BENCH=1 ;;
    --ecfft)            USE_ECFFT=1 ;;
    --cells=*)          CELL_FILTER="${arg#*=}" ;;
    --cmake-build=*)    CMAKE_BUILD="${arg#*=}" ;;
    -h|--help)
      sed -n '/^# build_matrix/,/^# Exit code/p' "$0"
      exit 0
      ;;
    *)
      echo "Unknown argument: $arg" >&2
      exit 2
      ;;
  esac
done

if [[ ! -x "$CMAKE_BUILD" ]]; then
  echo "ERROR: cmake-build helper not executable at: $CMAKE_BUILD" >&2
  echo "Override with --cmake-build=<path>" >&2
  exit 2
fi

DATE_TAG="$(date +%Y-%m-%d)"
LOG_ROOT="$REPO_ROOT/logs/matrix-${DATE_TAG}"
mkdir -p "$LOG_ROOT"

# Per-variant extra cmake args.
declare -A VARIANT_ARGS=(
  [default]=""
  [x64only]="-DENABLE_AVX2=OFF -DENABLE_AVX512=OFF"
  [noifma]="-DENABLE_AVX512=OFF"
  [portable]="-DFORCE_PORTABLE=ON"
)

# Always pass an explicit ENABLE_ECFFT define so a stale CMake cache from a
# prior session cannot silently flip the value we thought we were testing.
# Without this, a build dir that had ENABLE_ECFFT=ON cached would keep it ON
# even when this driver "intends" OFF, because CMake only overwrites cached
# entries when the define is provided on the command line.
ECFFT_VAL="OFF"
[[ "$USE_ECFFT" -eq 1 ]] && ECFFT_VAL="ON"
for v in "${!VARIANT_ARGS[@]}"; do
  VARIANT_ARGS[$v]="${VARIANT_ARGS[$v]} -DENABLE_ECFFT=${ECFFT_VAL}"
done

COMPILERS=(gcc clang msvc)
VARIANTS=(default x64only noifma portable)

# Cells that pass / fail.
PASSED_CELLS=()
FAILED_CELLS=()

# Per-cell summary rows for the final table.
declare -A SUMMARY_TESTS
declare -A SUMMARY_FUZZ
declare -A SUMMARY_SM_AUTOTUNE
declare -A SUMMARY_SM_INIT
declare -A SUMMARY_BUILD_TIME

# Returns the variant name to pass to the cmake-build helper (the helper uses
# it as the trailing segment of the build directory: build/<compiler>-release-<variant>).
# Empty string means the helper gets no --variant flag and writes to build/<compiler>-release.
helper_variant_name() {
  local variant="$1"
  if [[ "$USE_ECFFT" -eq 1 ]]; then
    if [[ "$variant" == "default" ]]; then
      echo "ecfft"
    else
      echo "ecfft-${variant}"
    fi
  else
    if [[ "$variant" == "default" ]]; then
      echo ""
    else
      echo "$variant"
    fi
  fi
}

# Returns the build-dir basename for a given (compiler, variant). Must agree
# with the path the cmake-build helper will use so that post-build artifact
# lookups hit the right directory.
build_dir_name() {
  local compiler="$1" variant="$2"
  local hvn
  hvn=$(helper_variant_name "$variant")
  if [[ -z "$hvn" ]]; then
    echo "${compiler}-release"
  else
    echo "${compiler}-release-${hvn}"
  fi
}

# Returns the cell label used for log subdirectories and the summary table.
# ECFFT=ON cells get an "ecfft-" infix to avoid colliding with a same-day
# ECFFT=OFF run and to read unambiguously in the summary.
cell_label() {
  local compiler="$1" variant="$2"
  if [[ "$USE_ECFFT" -eq 1 ]]; then
    echo "${compiler}-ecfft-${variant}"
  else
    echo "${compiler}-${variant}"
  fi
}

# Run a command, capture stdout+stderr to a log file. Returns the command's exit code.
run_logged() {
  local logfile="$1"; shift
  "$@" >"$logfile" 2>&1
}

# Extract "Total: N" from a tests/fuzz log; echoes "PASS N/N" or "FAIL".
extract_test_result() {
  local logfile="$1"
  local total passed failed
  total=$(grep -E "^Total:" "$logfile" 2>/dev/null  | awk '{print $2}')
  passed=$(grep -E "^Passed:" "$logfile" 2>/dev/null | awk '{print $2}')
  failed=$(grep -E "^Failed:" "$logfile" 2>/dev/null | awk '{print $2}')
  if [[ -z "$total" || -z "$passed" || -z "$failed" ]]; then
    echo "FAIL(no-output)"
  elif [[ "$failed" -ne 0 ]]; then
    echo "FAIL ${passed}/${total}"
  else
    echo "${passed}/${total}"
  fi
}

# Extract shaw_scalarmult average μs from a benchmark log.
extract_scalarmult_us() {
  local logfile="$1"
  grep -E "^[[:space:]]+shaw_scalarmult:" "$logfile" 2>/dev/null \
    | head -1 | awk '{print $4}'
}

run_cell() {
  local compiler="$1" variant="$2"
  local cell
  cell=$(cell_label "$compiler" "$variant")
  local cell_log_dir="$LOG_ROOT/$cell"
  mkdir -p "$cell_log_dir"

  local build_dir
  build_dir=$(build_dir_name "$compiler" "$variant")

  echo
  echo "=================================================================="
  echo "  CELL: $cell  (build/$build_dir)"
  echo "=================================================================="

  # ---- configure ----
  local extra="${VARIANT_ARGS[$variant]}"
  local helper_variant_arg=()
  local hvn
  hvn=$(helper_variant_name "$variant")
  if [[ -n "$hvn" ]]; then
    helper_variant_arg=(--variant "$hvn")
  fi

  local conf_log="$cell_log_dir/configure.log"
  if ! run_logged "$conf_log" \
       bash "$CMAKE_BUILD" --source "$REPO_ROOT" \
            --compiler "$compiler" --build-type Release \
            "${helper_variant_arg[@]}" \
            --extra-args "-DBUILD_TESTS=ON -DBUILD_BENCHMARKS=ON -DBUILD_TOOLS=ON $extra" \
            --configure; then
    echo "  CONFIGURE FAILED — see $conf_log"
    FAILED_CELLS+=("$cell:configure")
    return 1
  fi

  # ---- build ----
  local build_log="$cell_log_dir/build.log"
  local build_start build_end
  build_start=$(date +%s)
  if ! run_logged "$build_log" \
       bash "$CMAKE_BUILD" --source "$REPO_ROOT" \
            --compiler "$compiler" --build-type Release \
            "${helper_variant_arg[@]}" \
            --build; then
    echo "  BUILD FAILED — see $build_log"
    FAILED_CELLS+=("$cell:build")
    return 1
  fi
  build_end=$(date +%s)
  SUMMARY_BUILD_TIME[$cell]="$((build_end - build_start))s"

  local bin_dir="$REPO_ROOT/build/$build_dir"

  # ---- tests ----
  local test_bin="$bin_dir/ranshaw-tests.exe"
  [[ -x "$test_bin" ]] || test_bin="$bin_dir/ranshaw-tests"
  local test_log="$cell_log_dir/tests.log"
  if [[ -x "$test_bin" ]]; then
    "$test_bin" --autotune >"$test_log" 2>&1 || true
    SUMMARY_TESTS[$cell]=$(extract_test_result "$test_log")
  else
    SUMMARY_TESTS[$cell]="(no-bin)"
  fi
  if [[ "${SUMMARY_TESTS[$cell]}" == FAIL* ]]; then
    FAILED_CELLS+=("$cell:tests")
    echo "  TESTS FAILED: ${SUMMARY_TESTS[$cell]} — see $test_log"
    return 1
  fi

  # ---- fuzz ----
  if [[ "$SKIP_FUZZ" -eq 0 ]]; then
    local fuzz_bin="$bin_dir/ranshaw-fuzz-tests.exe"
    [[ -x "$fuzz_bin" ]] || fuzz_bin="$bin_dir/ranshaw-fuzz-tests"
    local fuzz_log="$cell_log_dir/fuzz.log"
    if [[ -x "$fuzz_bin" ]]; then
      "$fuzz_bin" --autotune >"$fuzz_log" 2>&1 || true
      SUMMARY_FUZZ[$cell]=$(extract_test_result "$fuzz_log")
    else
      SUMMARY_FUZZ[$cell]="(no-bin)"
    fi
    if [[ "${SUMMARY_FUZZ[$cell]}" == FAIL* ]]; then
      FAILED_CELLS+=("$cell:fuzz")
      echo "  FUZZ FAILED: ${SUMMARY_FUZZ[$cell]} — see $fuzz_log"
      return 1
    fi
  else
    SUMMARY_FUZZ[$cell]="(skipped)"
  fi

  # ---- benchmark ----
  if [[ "$SKIP_BENCH" -eq 0 ]]; then
    local bench_bin="$bin_dir/ranshaw-benchmark.exe"
    [[ -x "$bench_bin" ]] || bench_bin="$bin_dir/ranshaw-benchmark"
    if [[ -x "$bench_bin" ]]; then
      local bench_at_log="$cell_log_dir/bench-autotune.log"
      local bench_in_log="$cell_log_dir/bench-init.log"
      "$bench_bin" --autotune >"$bench_at_log" 2>&1 || true
      "$bench_bin" --init     >"$bench_in_log" 2>&1 || true
      SUMMARY_SM_AUTOTUNE[$cell]=$(extract_scalarmult_us "$bench_at_log")
      SUMMARY_SM_INIT[$cell]=$(extract_scalarmult_us "$bench_in_log")
    else
      SUMMARY_SM_AUTOTUNE[$cell]="(no-bin)"
      SUMMARY_SM_INIT[$cell]="(no-bin)"
    fi
  else
    SUMMARY_SM_AUTOTUNE[$cell]="(skipped)"
    SUMMARY_SM_INIT[$cell]="(skipped)"
  fi

  echo "  PASS  tests=${SUMMARY_TESTS[$cell]}  fuzz=${SUMMARY_FUZZ[$cell]}  sm(autotune)=${SUMMARY_SM_AUTOTUNE[$cell]:-?}μs  sm(init)=${SUMMARY_SM_INIT[$cell]:-?}μs  build=${SUMMARY_BUILD_TIME[$cell]:-?}"
  PASSED_CELLS+=("$cell")
  return 0
}

print_summary() {
  echo
  echo "=================================================================="
  echo "  MATRIX SUMMARY — $LOG_ROOT"
  echo "=================================================================="
  printf "%-20s %-12s %-14s %-12s %-12s %-8s\n" \
         "CELL" "TESTS" "FUZZ" "SM_AUTO(μs)" "SM_INIT(μs)" "BUILD"
  printf "%-20s %-12s %-14s %-12s %-12s %-8s\n" \
         "----" "-----" "----" "-----------" "-----------" "-----"
  for compiler in "${COMPILERS[@]}"; do
    for variant in "${VARIANTS[@]}"; do
      local cell
      cell=$(cell_label "$compiler" "$variant")
      [[ ! "$cell" == $CELL_FILTER ]] && continue
      printf "%-20s %-12s %-14s %-12s %-12s %-8s\n" \
             "$cell" \
             "${SUMMARY_TESTS[$cell]:--}" \
             "${SUMMARY_FUZZ[$cell]:--}" \
             "${SUMMARY_SM_AUTOTUNE[$cell]:--}" \
             "${SUMMARY_SM_INIT[$cell]:--}" \
             "${SUMMARY_BUILD_TIME[$cell]:--}"
    done
  done
  echo
  echo "Passed cells: ${#PASSED_CELLS[@]}"
  echo "Failed cells: ${#FAILED_CELLS[@]}"
  if [[ ${#FAILED_CELLS[@]} -gt 0 ]]; then
    printf '  - %s\n' "${FAILED_CELLS[@]}"
  fi
}

# ─── main ───
echo "build_matrix.sh — repo: $REPO_ROOT"
echo "  log root:      $LOG_ROOT"
echo "  cmake-build:   $CMAKE_BUILD"
echo "  cell filter:   $CELL_FILTER"
echo "  skip fuzz:     $SKIP_FUZZ"
echo "  skip bench:    $SKIP_BENCH"
echo "  ecfft:         $USE_ECFFT"

for compiler in "${COMPILERS[@]}"; do
  for variant in "${VARIANTS[@]}"; do
    cell=$(cell_label "$compiler" "$variant")
    [[ ! "$cell" == $CELL_FILTER ]] && continue
    run_cell "$compiler" "$variant" || true
  done
done

print_summary

exit $(( ${#FAILED_CELLS[@]} > 0 ? 1 : 0 ))
