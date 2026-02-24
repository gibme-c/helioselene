# selene — Selene Curve

Selene is the other half of the curve cycle — same curve shape (y² = x³ − 3x + b), same algorithms, same API, but operating over Fq instead of Fp. If you understand [Helios](../helios/README.md), you understand Selene.

```cpp
typedef struct { fq_fe X, Y, Z; } selene_jacobian;
typedef struct { fq_fe x, y; }    selene_affine;
```


## The Key Difference

Selene's base field is Fq (the Crandall prime q = 2^255 − γ), and its scalar field is Fp (p = 2^255 − 19). This is the mirror of Helios, and it's what closes the cycle: Helios group order = q = Selene base field, and Selene group order = p = Helios base field.

The practical consequence is that Selene point arithmetic is more expensive than Helios. Every field multiply involves the 3-stage Crandall reduction (see [fq](../fq/README.md)) instead of the simple ×19 fold that Fp enjoys. The algorithmic structure is identical — same Jacobian formulas, same window sizes, same MSM strategies — but each underlying field operation does more work.

The generator point has affine x = 1, and the curve constant b is a different value than Helios's b (necessarily, since it's over a different field).


## Scalar Operations

Selene scalars live in Fp (not Fq). This means `selene_scalar_add`, `selene_scalar_mul`, and friends are thin wrappers around `fp_*` operations. The reversal is the cycle property in action.

`selene_point_to_bytes` extracts the affine x-coordinate — which is an Fq element — as 32 bytes. Since Fq is the Helios scalar field, these bytes can be interpreted as a Helios scalar. This is Selene's half of the cycle bridge.


## Everything Else

Serialization, validation, hash-to-curve, Pedersen commitments, batch affine conversion, MSM (Straus/Pippenger) — all structurally identical to Helios with `selene_` prefixes and `fq_fe` coordinates. The same backend tiers exist (portable, x64, AVX2, IFMA), and the same 3 operations are runtime-dispatched (`selene_scalarmult`, `selene_scalarmult_vartime`, `selene_msm_vartime`).

The twist security is ~99 bits (vs ~107 for Helios), so on-curve validation at deserialization is equally important here.
