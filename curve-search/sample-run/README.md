# Sample run: reproducibility demonstration

Narrow-range reproducibility artifacts that re-run the search pipeline end-to-end on a consumer GPU. Cited by the paper *Ran and Shaw: A High-Security Curve Cycle over the Curve25519 Field* (§8.1); all counts below are derivable from the JSONL files in this directory.

## Scope

- Discriminant range scanned: D ∈ [3, 2 × 10⁸] (all ~2 × 10⁸ values), recorded in `candidates_checkpoint.json`.
- Cycle base field: p = 2²⁵⁵ − 19 (Curve25519 prime).
- Same playbook filter as the full run: γ ≤ 128 bits, q mod 8 ∈ {3, 5, 7}, both twist orders fully factored, deduplicated by q.

## Invocation

```
cuda-curve-search --start-d 3 --end-d 200000000 \
                  --min-twist 0 --max-gamma-bits 128 \
                  --q-mod-8 3,5,7 --rho-seconds 30
```

## Hardware and wall-clock

| Stage | Machine | Wall-clock |
| --- | --- | --- |
| GPU discriminant scan | NVIDIA RTX 3090 (consumer; less capable than the RTX 5090 used for the full run) | 2.2 s (~63 M D/s) |
| CPU twist-order factoring | 4 threads, Ryzen 7 9800X3D, WSL2 | ~19 min |

Total run (GPU + twist factoring, end-to-end): under 20 min.

## Pipeline outputs

| File | Lines / entries | Stage |
| --- | --- | --- |
| `candidates_gpu.jsonl` | 1,180 | Raw GPU candidates |
| `candidates_scanned.jsonl` | 90 | CPU-finalized 2-cycles with both twist orders fully factored (across 89 unique discriminants) |
| `candidates_filtered.jsonl` | 18 | After playbook filter, deduplicated by q |
| `candidates_top.json` | 18 | Ranked (max min-twist security, tiebreak on γ bits, then D) |
| `candidates_winner.json` | 1 | Rank-1 candidate |

## Top candidate

D = 92,169,307.

| Parameter | Value |
| --- | --- |
| γ (with q = 2²⁵⁵ − γ) | 90380853465244914229432499087071017579 |
| γ bits | 127 |
| q mod 8 | 5 |
| Ran twist order | 2(p + 1) − q, a 256-bit prime |
| Shaw twist order | 2(q + 1) − p = 7 · 59 · (244-bit prime) |
| min twist security | 247 bits |

This candidate appears at rank 5 of the full run's 4,786 filtered results (see `../full-run/candidates_top.json`) and matches the top-row entry of Table 9 in the paper.

The sample-run top list also cross-validates against the Veridise security assessment: all 22 Veridise-enumerated discriminants in D ∈ [3, 2 × 10⁸] appear in `candidates_scanned.jsonl` with bit-identical twist security; the 18 with q mod 8 ∈ {3, 5, 7} match `candidates_filtered.jsonl` exactly (Table 9 of the paper).

## Class-polynomial artifacts

Two invariants were run through `classpoly` for D = 92,169,307. Both are archived so downstream workers can use either path; j-invariant roots feed directly into realization, alternative-invariant roots require a modular-polynomial translation step.

- `hilbert/`: classical Hilbert class polynomial H_D(X), j-invariant mode (`classpoly --inv 0`).
- `atkin-59/`: Atkin invariant A₅₉ (level 59), the invariant selected by `classpoly` auto mode for this discriminant.

Each directory contains one `.coeffs.tar.gz` per cycle prime, plus a matching `.sha1` sidecar:

- `D92169307_p<Ran-prime>.coeffs.tar.gz`: coefficients reduced modulo the Ran field (p = 2²⁵⁵ − 19, ends …6564819949).
- `D92169307_p<Shaw-prime>.coeffs.tar.gz`: coefficients reduced modulo the Shaw field (q = p + 1 − t, ends …9493802389).

Each archive expands to a single `.coeffs` file containing the CRT-reconstructed class-polynomial coefficients written by `classpoly`. Sizes are small compared to the full run (h(D) is far smaller at D = 92,169,307). The `.sha1` sidecars cover the extracted `.coeffs` file, so verification requires extracting first:

```bash
for dir in hilbert atkin-59; do
    ( cd "$dir" && \
      for f in *.coeffs.tar.gz; do tar -xzf "$f"; done && \
      sha1sum -c *.sha1 )
done
```

## Realized curves and independent verification

Output of the `curve-search-realizer` root-finding and realization pass, plus an independent PARI/GP verification. Closes the loop from class-polynomial tarballs to the realized curve parameters:

| File | Contents |
| --- | --- |
| `winner.json` | `realize` output: one j-root per cycle prime (extracted by factoring the Hilbert polynomial under NTL), normalized to a = −3 short Weierstrass form with a non-square b, plus generator (gx, gy). Includes all winner fields from `candidates_winner.json` for self-containment. |
| `constants.txt` | The same parameters in library-ready form, with decimal + hex encodings and limb packings for the radix-25 (10×26/25-bit), radix-51 (5×51-bit), and radix-64 (4×64-bit) field backends. |
| `verification.out` | PARI/GP report from `scripts/verify.py` (in the realizer repo): primality of p and q, curve orders, generator on curve, generator full-order, b non-square in each base field, plus twist-order factorizations and bit counts. 11 PASS checks. |

Label mapping: `constants.txt` uses the generic `CurveA/CurveB` labels from the `finder` tool:

- `CurveA` is the curve over F_p (group order q).
- `CurveB` is the curve over F_q (group order p).

For the selected RanShaw cycle in `../full-run`, CurveA = Ran and CurveB = Shaw; the same structural mapping applies here, but this candidate (D = 92,169,307) is the sample-run top, not the selected cycle (which lives at D = 15,620,419,594,011).

## Purpose and relation to the full run

This run exists to let an independent reader reproduce the full construction pipeline end-to-end in minutes, on commodity hardware, without needing an RTX 5090 or a 128-core EPYC. It validates that:

1. The GPU search produces the same candidate set (cross-checked against the independent Veridise enumeration).
2. CPU twist factoring reaches the same factorizations bit-for-bit.
3. The ranking applied to this subset selects D = 92,169,307 as its top candidate, which is exactly rank 5 of the full run.
4. `classpoly` → `realize` → PARI/GP verification succeeds end-to-end on a non-trivial discriminant, producing curves that pass all 11 spec-compliance checks (`verification.out`).

It does not produce a better curve than the full run: its winner has only 247-bit min twist security, versus 254 bits for the selected RanShaw cycle at D = 15,620,419,594,011 (see `../full-run`).

## Tooling

- GPU search: [`cuda-curve-search`](https://github.com/brandonlehmann/cuda-curve-search), commit `9e42f75e418271ee720c98b843348cc266e8cb22`
- Class polynomial (pipelined parallel fork): [`classpoly`](https://github.com/brandonlehmann/classpoly), commit `490272a8278781bdfb25c17d6db08352ad82f2ca`
- Root finding and curve realization: [`curve-search-realizer`](https://github.com/brandonlehmann/curve-search-realizer), commit `2c1f7d073556988695f0468b70af099b09ce794b`

SHAs pinned to upstream HEAD as of 2026-04-18.
