# Full run: RanShaw curve-cycle search

Reproducibility artifacts for the complete discriminant search that produced the selected Ran/Shaw cycle. Cited by the paper *Ran and Shaw: A High-Security Curve Cycle over the Curve25519 Field* (§5 and §8); all counts below are derivable from the JSONL files in this directory.

## Scope

- Discriminant range scanned: D ∈ [3, 1.61 × 10¹³] (16.1 trillion values). Recorded in `candidates_checkpoint.json`.
- Cycle base field: p = 2²⁵⁵ − 19 (Curve25519 prime).
- Playbook filter: γ ≤ 128 bits, q mod 8 ∈ {3, 5, 7}, both twist orders fully factored, deduplicated by q.

## Invocation

```
cuda-curve-search --start-d 3 --end-d 16100000000000 \
                  --min-twist 0 --max-gamma-bits 128 \
                  --q-mod-8 3,5,7 --rho-seconds 30
```

Identical flags to the sample run, with `--end-d` raised from 2 × 10⁸ to 1.61 × 10¹³.

## Hardware and wall-clock

| Stage | Machine | Wall-clock |
| --- | --- | --- |
| GPU discriminant scan (Cornacchia + trace signs) | NVIDIA RTX 5090 | ~19 h (~253 M D/s) |
| CPU twist-order factoring (trial / Pollard rho / PARI fallback) | 16 threads | ~17 h |
| `classpoly`, j-invariant mode (`--inv 0`) | 128-core AMD EPYC | ~56 h per curve |
| `classpoly`, auto mode (double-eta w₃,₅, invariant type 515) | 128-core AMD EPYC | ~6 h per curve |
| Root finding + curve realization | 16-core machine | ~2.5 h per curve (~5 h total) |

Total GPU + CPU search time: ~36 h. Class-polynomial generation and realization are separate downstream stages and are not part of the search wall-clock.

## Pipeline outputs

| File | Lines / entries | Stage |
| --- | --- | --- |
| `candidates_gpu.jsonl` | 342,436 | Raw GPU candidates (squarefree, Jacobi (−D\|p) = 1, local-solve sieve, Cornacchia t² + Ds² = 4p, both trace signs) |
| `candidates_scanned.jsonl` | 25,627 | CPU-finalized 2-cycles with both twist orders fully factored (across 25,537 unique discriminants) |
| `candidates_filtered.jsonl` | 4,786 | After playbook filter, deduplicated by q |
| `candidates_top.json` | 30 | Ranked top candidates (max min-twist security, tiebreak on γ bits, then D) |
| `candidates_winner.json` | 1 | Rank-1 candidate: the selected RanShaw cycle |

## Selected cycle

D = 15,620,419,594,011,  h(D) = 656,568.

| Parameter | Value |
| --- | --- |
| t (trace) | 239666463199878229209741112730228557711 |
| γ (with q = 2²⁵⁵ − γ) | 239666463199878229209741112730228557729 |
| γ bits | 128 |
| q mod 8 | 7 |
| Ran twist order | 2(p + 1) − q = 3 · P₁, with P₁ a 254-bit prime |
| Shaw twist order | 2(q + 1) − p = 3 · P₂, with P₂ a 254-bit prime |
| min twist security | 254 bits |
| p (Ran base field) | 2²⁵⁵ − 19 |
| q (Shaw base field) | p + 1 − t |

This cycle sits at the extreme right tail of the filtered distribution. No other cycle in the 4,786-candidate pool reached 254-bit twist security on both sides; the runner-up (rank 2) reached 252 / 252.

## Class-polynomial artifacts

Two invariants were run through `classpoly` for D = 15,620,419,594,011. Both outputs are archived so downstream workers can pick whichever they prefer; j-invariant roots feed directly into realization, alternative-invariant roots require a modular-polynomial translation step.

- `hilbert/`: classical Hilbert class polynomial H_D(X), j-invariant mode (`classpoly --inv 0`). These are the coefficients used for the published Ran and Shaw curve construction.
- `w3w5/`: double-eta quotient w₃,₅(τ), the invariant type 515 selected by `classpoly` auto mode for this discriminant (~9× faster than j-invariant mode: ~6 h vs ~56 h).

Each directory contains one `.coeffs.tar.gz` per cycle prime, plus a matching `.sha1` sidecar:

- `D15620419594011_p<Ran-prime>.coeffs.tar.gz`: coefficients reduced modulo the Ran field (p = 2²⁵⁵ − 19, ends …6564819949).
- `D15620419594011_p<Shaw-prime>.coeffs.tar.gz`: coefficients reduced modulo the Shaw field (q = p + 1 − t, ends …6336262239).

Each archive expands to a single `.coeffs` file containing the CRT-reconstructed class-polynomial coefficients written by `classpoly`. The `.sha1` sidecars cover the extracted `.coeffs` file, so verification requires extracting first:

```bash
for dir in hilbert w3w5; do
    ( cd "$dir" && \
      for f in *.coeffs.tar.gz; do tar -xzf "$f"; done && \
      sha1sum -c *.sha1 )
done
```

## Realized curves and independent verification

Output of the `curve-search-realizer` root-finding and realization pass, plus an independent PARI/GP verification. Closes the loop from class-polynomial tarballs to the final Ran and Shaw curve parameters:

| File | Contents |
| --- | --- |
| `winner.json` | `realize` output: one j-root per cycle prime (extracted by factoring the Hilbert polynomial under NTL), normalized to a = −3 short Weierstrass form with a non-square b, plus generator (gx, gy). Includes all winner fields from `candidates_winner.json` for self-containment. |
| `constants.txt` | The same parameters in library-ready form, with decimal + hex encodings and limb packings for the radix-25 (10×26/25-bit), radix-51 (5×51-bit), and radix-64 (4×64-bit) field backends. |
| `verification.out` | PARI/GP report from `scripts/verify.py` (in the realizer repo): primality of p and q, curve orders, generator on curve, generator full-order, b non-square in each base field, plus twist-order factorizations and bit counts. 11 PASS checks. |

Label mapping: the `finder` tool that produced `constants.txt` and drove `verification.out` uses the generic labels `CurveA` and `CurveB`. For this cycle:

- `CurveA` = Ran (defined over F_p, group order q).
- `CurveB` = Shaw (defined over F_q, group order p).

So `curve_a_b`, `curve_a_gx`, `curve_a_gy` in `winner.json` and `constants.txt` are the Ran parameters, and `curve_b_*` are the Shaw parameters. The decimal values match §5.2 of the paper (Ran and Shaw b, generator coordinates) and the twist bits and min-twist value match §5.1.

## Tooling

- GPU search: [`cuda-curve-search`](https://github.com/brandonlehmann/cuda-curve-search), commit `9e42f75e418271ee720c98b843348cc266e8cb22`
- Class polynomial (pipelined parallel fork): [`classpoly`](https://github.com/brandonlehmann/classpoly), commit `490272a8278781bdfb25c17d6db08352ad82f2ca`
- Root finding and curve realization: [`curve-search-realizer`](https://github.com/brandonlehmann/curve-search-realizer), commit `2c1f7d073556988695f0468b70af099b09ce794b`

SHAs pinned to upstream HEAD as of 2026-04-18.

See the paper for methodology, ranking criteria, and security analysis.
