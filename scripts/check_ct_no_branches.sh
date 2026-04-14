#!/usr/bin/env bash
# check_ct_no_branches.sh
#
# Disassembly gate for the constant-time complete-addition drivers and the
# CT MSM TUs. Runs objdump on each matching object file, greps for conditional
# branch mnemonics inside the target function bodies, and fails if any are
# found.
#
# Gate targets (each must emit zero je/jne/jz/jnz under gcc-Release and
# clang-Release):
#
#   ran_complete_add.cpp.o
#   shaw_complete_add.cpp.o
#   ran_msm_ct.cpp.o                   (lands at C4/C5)
#   shaw_msm_ct.cpp.o                  (lands at C4/C5)
#
# MSVC is exempt: gcc + clang cover the instruction-selection space, and
# dumpbin/disassembly tooling is not consistently available in MinGW sh.
#
# Usage:
#   scripts/check_ct_no_branches.sh [--verbose]
#
# Exit codes:
#   0  all inspected objects are branch-free
#   1  at least one conditional branch found
#   2  objdump unavailable, or no matching objects exist (build first)

set -euo pipefail

cd "$(dirname "$0")/.."
REPO_ROOT="$(pwd)"

VERBOSE=0
for arg in "$@"; do
  case "$arg" in
    --verbose) VERBOSE=1 ;;
    -h|--help)
      sed -n '2,30p' "$0"
      exit 0
      ;;
    *) echo "unknown arg: $arg" >&2; exit 2 ;;
  esac
done

command -v objdump >/dev/null 2>&1 || {
  echo "error: objdump not found on PATH" >&2
  exit 2
}

COMPILERS=(gcc clang)
VARIANTS=(default x64only noifma portable)

# Only the complete-add bodies are gated for strict branch-freeness. The RCB
# Algorithm 4 formula is supposed to compile to a linear block of field-op
# calls with zero secret-dependent control flow. The CT MSM / Pedersen
# drivers that wrap these are NOT gated here because they contain loops over
# public quantities (n points, 64 windows, 4 doublings per window, 8 entries
# per table scan); those loop-count branches are data-independent and safe,
# but they are hard to distinguish from secret-dependent branches without
# semantic analysis. Data-independence of driver loop bounds is enforced by
# code review and the CT-vs-VT cross-check fuzz tests, not by this gate.
TU_NAMES=(ran_complete_add shaw_complete_add)

objects=()
for compiler in "${COMPILERS[@]}"; do
  for variant in "${VARIANTS[@]}"; do
    if [[ "$variant" == "default" ]]; then
      build_dir="build/${compiler}-release"
    else
      build_dir="build/${compiler}-release-${variant}"
    fi
    for tu in "${TU_NAMES[@]}"; do
      # Library object location under CMake's standard layout.
      case "$tu" in
        ran_*) lib="ran" ;;
        shaw_*) lib="shaw" ;;
      esac
      # Windows/MinGW emits .obj, Unix emits .o. Check both.
      for ext in obj o; do
        obj="${REPO_ROOT}/${build_dir}/${lib}/CMakeFiles/ranshaw-${lib}.dir/src/${tu}.cpp.${ext}"
        if [[ -f "$obj" ]]; then
          objects+=("$obj")
          break
        fi
      done
    done
  done
done

if [[ ${#objects[@]} -eq 0 ]]; then
  echo "error: no matching object files found under build/. Build gcc/clang Release first." >&2
  exit 2
fi

# Conditional branch mnemonics that would be disqualifying in CT code if they
# depended on secret data. Unconditional jmp / call / ret are fine. Restricted
# to instruction lines: address, raw bytes, mnemonic.
BRANCH_RE='^[[:space:]]*[0-9a-f]+:[[:space:]]+[0-9a-f ]+[[:space:]]+(je|jne|jz|jnz|js|jns|jg|jl|jge|jle|ja|jb|jae|jbe|jo|jno|jp|jpe|jnp|jpo|jcxz|jecxz|jrcxz)([[:space:]]|$)'

# -fstack-protector-strong emits one canary-check branch per function whose
# target is a call to __stack_chk_fail on canary mismatch. The branch
# direction is data-independent of the function's inputs (canary is set at
# prologue from the master and compared at epilogue), so it does not leak
# timing on secrets. We subtract one expected canary branch per TU that
# carries a __stack_chk_fail relocation.

fail=0
total_branches=0
for obj in "${objects[@]}"; do
  disasm="$(objdump -dr "$obj" 2>/dev/null)"
  all_branches_count="$(printf '%s\n' "$disasm" | grep -cE "$BRANCH_RE" || true)"
  # GNU libc SSP uses __stack_chk_fail. MSVC-ABI clang on Windows uses
  # __security_check_cookie (and __security_cookie for the canary slot
  # compare). Either symbol being present is enough to attribute one
  # conditional branch per function to the data-independent canary check.
  canary_count="$(printf '%s\n' "$disasm" | grep -cE '__stack_chk_fail|__security_check_cookie' || true)"
  # Net CT-relevant branches = total conditional branches minus expected
  # canary-check branches (one jne → call __stack_chk_fail per function).
  net=$((all_branches_count - canary_count))
  rel="${obj#$REPO_ROOT/}"
  if [[ "$net" -le 0 ]]; then
    [[ "$VERBOSE" -eq 1 ]] && echo "ok    $rel (canary=$canary_count, other=0)"
  else
    echo "FAIL  $rel (net=$net branches after excluding $canary_count canary)"
    printf '%s\n' "$disasm" | grep -E "$BRANCH_RE" | head -10 | sed 's/^/        /'
    fail=1
    total_branches=$((total_branches + net))
  fi
done

if [[ "$fail" -eq 0 ]]; then
  echo "ct-disasm-gate: ${#objects[@]} objects clean"
  exit 0
fi
echo "ct-disasm-gate: $total_branches conditional branches across ${#objects[@]} objects" >&2
exit 1
