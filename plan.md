# Helios/Selene C++ Curve Library — Implementation Plan

## 1. Project Context & Motivation

The existing Rust `helioselene` crate (kayabaNerve / Lederstrumpf's optimized submission) is the only production-grade implementation of this curve cycle. Monero's FCMP++ integration currently calls into Rust via FFI from C++. The goal of this project is a native, standalone C++ library that:

- Eliminates the Rust FFI boundary and its attendant complexity for C++-native consumers.
- Provides a hardened, auditable implementation suitable for deployment in security-critical contexts.
- Achieves superior performance through architecture-specific SIMD paths that the Rust ecosystem doesn't yet expose cleanly.
- Serves as the foundation layer for generic Bulletproofs and Curve Trees that will be built downstream.

---

## 2. Curve Parameters (Canonical Reference)

All parameters sourced from the Veridise security assessment and tevador's original construction. CM discriminant D = −7857907.

### 2.1 Helios

| Property | Value |
|---|---|
| **Base field** | F_p, p = 2^255 − 19 (the Ed25519 base field) |
| **Equation** | y² = x³ − 3x + b |
| **b** | `15789920373731020205926570676277057129217619222203920395806844808978996083412` |
| **Group order** | q = 2^255 − 85737960593035654572250192257530476641 |
| **Base point** | (3, `37760095087190773158272406437720879471285821656958791565335581949097084993268`) |
| **Form** | Short Weierstrass, a = −3 |
| **Cofactor** | 1 (prime order) |

### 2.2 Selene

| Property | Value |
|---|---|
| **Base field** | F_q, q = 2^255 − 85737960593035654572250192257530476641 |
| **Equation** | y² = x³ − 3x + b |
| **b** | `50691664119640283727448954162351551669994268339720539671652090628799494505816` |
| **Group order** | p = 2^255 − 19 |
| **Base point** | (1, `55227837453588766352929163364143300868577356225733378474337919561890377498066`) |
| **Form** | Short Weierstrass, a = −3 |
| **Cofactor** | 1 (prime order) |

### 2.3 Cycle Relationship

```
|Helios(F_p)| = q        (Helios is defined over F_p, has q rational points)
|Selene(F_q)| = p        (Selene is defined over F_q, has p rational points)
Selene_scalar_field = F_p = Ed25519_base_field
Helios_scalar_field = F_q = Selene_base_field
```

This means Wei25519 (the Weierstrass form of Ed25519) shares its base field with Helios. Leaves in the FCMP++ curve tree are Selene scalars (which live in F_p), and the tree alternates between Selene and Helios layers.

### 2.4 Key Properties for Implementation

- **q ≡ 3 (mod 4)**: Enables Tonelli-Shanks fast path for square roots in F_q (just exponentiation by (q+1)/4).
- **p ≡ 5 (mod 8)**: Square roots in F_p use the Atkin method (standard for Ed25519 field).
- **Crandall prime structure**: q = 2^255 − γ where γ ≈ 2^127. Crandall's reduction algorithm applies, but because γ is 127 bits (not small like ed25519's 19), the reduction requires a multi-limb wide multiply rather than a simple per-limb fold-back. See §4.2 for the full reduction algorithm.
- **a = −3 for both curves**: Enables the optimized Jacobian doubling formula (saves one field multiplication per doubling vs general short Weierstrass).
- **b is a non-square**: No point has x-coordinate 0 on either curve, which prevents certain power-analysis attack vectors.
- **Twist security is weak**: ~99 bits (Selene twist) and ~107 bits (Helios twist). Curve membership checks are **mandatory** on all received points. This must be enforced at the API level.

---

## 3. Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                        Public API Layer                          │
│   helios::Point, selene::Point                                   │
│   ScalarMul, MultiExp, PedersenCommit, MapToCurve                │
├──────────────────────────────────────────────────────────────────┤
│                     Curve Arithmetic Layer                        │
│   Jacobian coords, mixed addition, wNAF, Pippenger               │
│   EC-divisor polynomial arithmetic                                │
├──────────────────────────────────────────────────────────────────┤
│                     Field Arithmetic Layer                        │
│   Fp (2^255 − 19)  │  Fq (Crandall prime)  │  Scalar fields     │
│   ┌─────────────────┼───────────────────────┼──────────────────┐ │
│   │  Generic (32b)  │  Generic (32b)        │  Generic (32b)   │ │
│   │  x86_64 (asm)   │  x86_64 (Crandall)   │  x86_64          │ │
│   │  AVX2 (4-way)   │  AVX2 (4-way)        │  AVX2            │ │
│   │  AVX-512 (8-way)│  AVX-512 (8-way)     │  AVX-512         │ │
│   └─────────────────┴───────────────────────┴──────────────────┘ │
├──────────────────────────────────────────────────────────────────┤
│                     Platform Abstraction                          │
│   CPUID detection, runtime dispatch, CT memory barriers           │
│   helioselene_secure_erase, side-channel countermeasures            │
└──────────────────────────────────────────────────────────────────┘
```

### 3.1 Reference Architecture: `gibme-c/ed25519`

This library is **not** a greenfield build. The `gibme-c/ed25519` library (development branch) provides a proven, high-performance C++ architecture that we will use as the **structural blueprint**. The ed25519 library already solves the same class of problems — field arithmetic over a 255-bit prime, point operations on an elliptic curve, SIMD-accelerated multi-scalar multiplication — and its patterns transfer almost 1:1 to Helios/Selene with modifications only at the mathematical level (different reduction constants, different point addition formulas). Helioselene has **no build-time dependency on ed25519** — we study the architecture, adapt the patterns, and produce a standalone library.

**What transfers directly (reuse with minimal changes):**

- **Platform detection & dispatch model** (`ed25519_platform.h` → `helioselene_platform.h`): The compile-time `ED25519_PLATFORM_X64` / `ED25519_PLATFORM_ARM64` / `ED25519_PLATFORM_64BIT` macro hierarchy, the 128-bit multiply abstraction (`__int128` vs `_umul128`), and the `FORCE_REF10` override mechanism. Mirror this 1:1.
- **Runtime SIMD dispatch** (`ed25519_dispatch.h`, `dispatch.cpp`): The function-pointer dispatch table pattern with `ed25519_init()` (CPUID heuristic) and `ed25519_autotune()` (per-function benchmarking). The table structure, the `std::once_flag` thread-safety, and the auto-tune timing harness are all reusable patterns to replicate. For helioselene, the dispatch table has different function signatures (Jacobian points instead of extended Edwards), but the dispatch mechanism is identical.
- **CPUID detection** (`ed25519_cpuid.h`, `cpuid.cpp`): The CPUID + XGETBV detection for AVX2 / AVX-512F / AVX-512IFMA. This is CPU-specific, not curve-specific — replicate directly as `helioselene_cpuid.h`.
- **Constant-time primitives** (`ct_barrier.h`, `fe_cmov.h`): The `ct_barrier_u32`/`ct_barrier_u64` inline asm barriers (GCC/Clang: `+r` constraint, MSVC: volatile round-trip) and the XOR-blend `fe_cmov` pattern. These are the building blocks for all constant-time operations. Replicate as helioselene internal headers.
- **Secure memory erasure** (`ed25519_secure_erase.h`): The platform-abstracted `SecureZeroMemory` / `memset_s` / `explicit_bzero` / volatile-function-pointer chain. Replicate as `helioselene_secure_erase.h`.
- **Build system patterns** (`CMakeLists.txt`): The per-TU SIMD compile flag model where AVX2 sources get `-mavx2` and IFMA sources get `-mavx512f -mavx512ifma` via `set_source_files_properties()`. The `ED25519_SIMD` / `ED25519_NO_AVX2` / `ED25519_NO_AVX512` compile definitions for conditional compilation. The auto-vectorization suppression flags (`-fno-tree-vectorize` for GCC, `-fno-vectorize -fno-slp-vectorize` for Clang) that protect constant-time guarantees. Replicate this CMake structure with `HELIOSELENE_` prefixed definitions.
- **MSM algorithm selection** (Straus for n≤32, Pippenger for n>32): The signed-digit encoding (`encode_signed_w4`, `encode_signed_wbit`), the window-size selection heuristic (`pippenger_window_size`), the bucket-accumulation + running-sum structure, and the crossover threshold. The point addition calls change, but the outer algorithm is curve-agnostic.
- **Normalization discipline** (the `_nn` / `_n` suffixed IFMA variants): The insight that IFMA outputs are ≤51 bits, `fe_add` of two ≤51-bit values produces ≤52 bits (safe for IFMA input), so chained operations can skip re-normalization. This exact discipline applies to F_q Crandall arithmetic with different bounds.

**What requires adaptation (same pattern, different math):**

- **Field element type**: ed25519 uses `typedef uint64_t fe[5]` for radix-2^51. Helioselene needs `typedef uint64_t fp_fe[5]` (identical for F_p) and `typedef uint64_t fq_fe[5]` (same layout for F_q, different reduction). The 32-bit path uses `int32_t[10]` alternating 26/25-bit limbs.
- **Field multiplication reduction tail**: ed25519 reduces by folding upper limbs with a per-limb multiply by 19 (5-bit constant, fits in 128-bit intermediates). For F_q (Crandall), γ is **127 bits** — per-limb fold-back overflows 128-bit intermediates. Instead, the full 510-bit product is split at 255 bits and reduced via a multi-limb wide multiply `hi × 2γ`. This is the most significant structural difference from ed25519's field arithmetic and requires careful implementation (see §4.2 for full algorithm).
- **IFMA field mul core** (`fe_mul_ifma_core`): The `_mm512_alignr_epi64` schoolbook structure and the lo/hi accumulator split are reusable. The fold-back step changes from single-instruction `19 * lo[5..7]` to a multi-step Crandall reduction with `hi × 2γ` requiring multiple IFMA pairs and additional register pressure. The `fe_normalize_weak` carry chain transfers directly.
- **AVX2 4-way field arithmetic** (`fe10x4_avx2.h`): The `fe10x4` struct (10 × `__m256i` holding 4 field elements), the `_mm256_mul_epu32` schoolbook with pre-multiplied wrap-around terms, and the interleaved carry chain all transfer. Replace `FE10X4_19` with the Crandall constant for F_q.
- **IFMA 8-way batch** (`fe51x8_ifma.h`): The `fe51x8` struct (5 × `__m512i` holding 8 field elements), the `FE51X8_MUL19` shift-and-add macro (which avoids `_mm512_mullo_epi64` to skip the AVX-512DQ dependency), and the full carry chain. For F_q, replace the ×19 with ×2γ (still decomposable via shift-and-add since γ has structure).
- **Point representations**: ed25519 uses extended twisted Edwards coordinates (ge_p2, ge_p3, ge_p1p1, ge_precomp, ge_cached). Helios/Selene use Jacobian short Weierstrass. The type hierarchy (intermediate representation → full representation → cached/precomp for repeated addition) is the same pattern, but the structs have different fields and the formulas differ.
- **Curve-specific constants**: The `d2` constant in Edwards addition, the base point table (`ge_precomp_base.inl`), the group order for scalar reduction — all change to Helios/Selene values.
- **Scalar arithmetic**: ed25519 reduces mod ℓ ≈ 2^252. Helios scalars reduce mod q, Selene scalars mod p. The Barrett/schoolbook reduction structure from `sc_reduce.cpp` / `sc_muladd.cpp` adapts with different modulus constants.

**What is genuinely new (no ed25519 analog):**

- **F_q Crandall reduction**: The two-round reduction algorithm specific to q = 2^255 − γ where γ ≈ 2^127. This is the most significant novel field arithmetic work — the wide γ constant means the reduction is structurally different from ed25519's per-limb ×19 fold (see §4.2).
- **Jacobian point formulas**: The `a = −3` doubling optimization (3M + 4S), mixed Jacobian+affine addition (7M + 4S), general Jacobian addition (11M + 5S). These replace the Edwards unified addition. Note: GLV endomorphism does not apply to these curves (CM discriminant D = −7857907 does not yield an efficient endomorphism).
- **Wei25519 bridge (receive-side only)**: Helioselene accepts a raw 32-byte Wei25519 x-coordinate and returns a Selene scalar. The Ed25519 → Wei25519 coordinate transform is the caller's responsibility — helioselene has no dependency on any ed25519 library. This is a trivial function (~5 lines: deserialize bytes as F_p element, validate range, return as SeleneScalar).
- **Hash-to-curve (SSWU)**: RFC 9380 mapping for short Weierstrass curves. Ed25519 uses Elligator, which is a different mapping.
- **Dual-curve API**: The ed25519 library operates over a single curve. Helioselene exposes two curves with cross-curve scalar extraction, which requires careful API design to prevent mixing types.

---

## 4. Field Arithmetic — The Performance Foundation

Field arithmetic is where 80%+ of the wall-clock time lives. We need two fully independent field implementations.

### 4.1 F_p: The Ed25519 Field (2^255 − 19)

This is the most studied 255-bit prime in existence. **The F_p implementation can be lifted almost entirely from `gibme-c/ed25519`**, since it operates over the identical field. The core arithmetic (mul, sq, add, sub, invert, pow, tobytes, frombytes) is field-specific, not curve-specific.

**Representation**: `uint64_t limbs[5]` in unsaturated radix-2^51 for 64-bit (ref: `fe.h`, `x64/fe51.h`), or `int32_t limbs[10]` in radix-2^25.5 (alternating 26/25-bit limbs) for 32-bit (ref: `ref10/` directory).

**Multiplication strategy by arch** (all proven in ed25519):
- **32-bit**: Schoolbook 10×10 with delayed carry, ref10 path. Reference: `src/ref10/fe_mul.cpp`.
- **x86_64 scalar**: 5×5 schoolbook using `__int128` or `_umul128` with carry-by-19 wrap. Reference: `x64/fe51_inline.h` (`fe51_mul_inline`). The `mul128.h` abstraction handles GCC/Clang (`__int128`) vs MSVC (`_umul128` + `ed25519_uint128_emu`) portably.
- **AVX2 4-way**: `fe10x4` struct — 10 × `__m256i` holding 4 independent field elements in radix-2^25.5. Schoolbook with `_mm256_mul_epu32`, pre-multiplied ×19 wrap-around terms, pre-doubled odd-indexed limbs, interleaved carry chain for ILP. Reference: `x64/avx2/fe10x4_avx2.h` (`fe10x4_mul`). Uses 2p bias subtraction pattern for constant-time negation.
- **AVX-512 IFMA single-op**: Packs one field element's 5 limbs across 5 lanes of a `__m512i`, broadcasts each limb of the second operand, uses `_mm512_alignr_epi64` for shifted views, accumulates via `vpmadd52lo`/`vpmadd52hi` IFMA instructions. Lo/hi split at bit 52, recombined with ×2 shift (since radix is 2^51, not 2^52). Reference: `x64/ifma/fe_ifma.h` (`fe_mul_ifma_core`). Normalizing vs non-normalizing variants (`_nn` suffix) skip input carry propagation when limb bounds are known ≤52 bits.
- **AVX-512 IFMA 8-way batch**: `fe51x8` struct — 5 × `__m512i` holding 8 independent field elements in radix-2^51. Full 5×5 schoolbook across 25 IFMA pairs into 9 lo/hi accumulators. The ×19 wrap uses shift-and-add (`19x = 16x + 2x + x`) to avoid `_mm512_mullo_epi64` which requires AVX-512DQ. Reference: `x64/ifma/fe51x8_ifma.h` (`fe51x8_mul`). Register budget: 28 of 32 ZMM registers.

**Inversion**: Fermat exponentiation via addition chain (`x64/fe_invert.cpp`, `x64/fe_pow22523.cpp`). The chain macros (`x64/fe51_chain.h`, `x64/ifma/fe_ifma_chain.h`) provide normalizing and non-normalizing variants for chained squarings. Bernstein-Yang constant-time GCD is an alternative for secret-dependent paths if needed.

**Square root**: Atkin's algorithm for p ≡ 5 (mod 8). Compute candidate r = a^((p+3)/8), then conditionally multiply by sqrt(−1) if r² ≠ a. Reference: `x64/fe_divpowm1.cpp`.

### 4.2 F_q: The Crandall Field (2^255 − γ)

This is the novel field introduced by the Helios/Selene cycle. γ = 85737960593035654572250192257530476641, which is approximately 2^126.

**Representation**: Unsaturated radix-2^51 (five 64-bit limbs) on 64-bit. For 32-bit, radix-2^25.5 (ten 32-bit limbs). **Same layout as F_p** — the `fe` type structure is identical, only the reduction constant differs.

**Adaptation from ed25519 — CRITICAL DIFFERENCE**: The schoolbook product phase of `fe51_mul_inline` (the 25 `mul64()` calls and 128-bit accumulations) is **identical** for F_q — multiplication is just integer multiplication of limbs. However, the carry-wrap is **fundamentally different** from ed25519's ×19 fold and cannot be a simple constant swap.

In ed25519, after the 5×5 schoolbook you have 9 partial sums h0..h8, and the upper limbs fold back as `h0 += 19*h5, h1 += 19*h6, ...`. This works because 19 is 5 bits — so 19 × a ~105-bit accumulator ≈ 110 bits, fitting in 128-bit intermediates. For F_q, γ is **127 bits**. A naive `γ * h5` where h5 is ~105 bits gives ~232 bits — this overflows 128-bit intermediates and silently produces wrong results.

**Correct approach for F_q reduction**: Instead of per-limb fold-back, the Crandall reduction operates on the full 510-bit product:

1. **Full schoolbook product → 10 limbs** (h0..h9 in radix-2^51, representing a 510-bit value). Do NOT fold upper limbs during accumulation. This differs from ed25519 which folds inline.
2. **Split at 255 bits**: `lo = h0..h4` (the low 255 bits), `hi = h5..h9` (the upper ~255 bits).
3. **Multiply hi × 2γ**: Since hi is 5 limbs and 2γ ≈ 2^128, this is a 5-limb × 3-limb multiply (2γ fits in 3 radix-2^51 limbs). Each pairwise product fits in 128-bit intermediates because individual limbs are ≤51 bits. Result is at most 8 limbs.
4. **Add to lo with carries**: lo += (hi × 2γ), producing at most ~383 bits (8 limbs).
5. **Second reduction round**: If the result exceeds 255 bits, split again and repeat with the much smaller overflow (at most ~128 bits × 2γ). This second round always fits.
6. **Final conditional subtraction**: If result ≥ q, subtract q.

The θ optimization (γ − 1 divisible by 2^10, so 2γ = 2 + 2^11 × θ where θ is 119 bits / 3 limbs) reduces the wide multiply: `hi × 2γ = 2*hi + (hi << 11) × θ`, where `hi × θ` is a 5-limb × 3-limb multiply.

**Squaring**: Same reduction approach. The schoolbook squaring saves ~40% of multiplies via symmetry (same as ed25519's `fe51_sq_inline`), but the reduction tail uses the full Crandall method above.

**Addition/subtraction**: These are identical to F_p — just limb-wise add/sub with bias constants for subtraction. The bias values change from 2p to 2q multiples.

**Crandall reduction**: After a 510-bit product `[lo256, hi254]`:
1. Split: `result = lo + hi × 2γ` where `2γ < 2^128`
2. The multiplication `hi × 2γ` is a 254-bit × 128-bit multiply producing at most 382 bits
3. Add to lo (256 bits), producing at most 383 bits
4. One more round of Crandall reduction clears any remaining overflow
5. Final conditional subtraction

The key optimization (flagged in the competition docs) is that `γ − 1` is divisible by 2^10, so `θ = (γ−1)/2^10` is only 119 bits. This means `2γ = 2 + 2^11 × θ`, enabling the multiply-by-2γ step to be decomposed into a shift-by-11 plus a 119-bit multiply, which is cheaper than a full 128-bit multiply.

**SIMD adaptation strategy**: For each SIMD tier, the schoolbook product phase clones directly from ed25519. The reduction tail requires a fundamentally different structure (full wide multiply by 2γ, not per-limb fold-back):
- **AVX2 4-way** (`fe10x4`): The 10-limb radix-2^25.5 schoolbook (100 `_mm256_mul_epu32` calls) is identical. The reduction changes from per-limb `_mm256_mul_epu32(c, FE10X4_19)` to a multi-limb Crandall reduction across the upper 10 limbs. The 2p bias values in subtraction (`FE10X4_BIAS0`, `FE10X4_BIAS_EVEN`, `FE10X4_BIAS_ODD`) change to 2q bias values.
- **IFMA single-op** (`fe_mul_ifma_core`): The `_mm512_alignr_epi64` schoolbook and `vpmadd52lo/hi` accumulation are identical. The fold-back step changes from `lo[0] += 19 * lo[5]` (single IFMA pair) to a multi-step Crandall reduction where `hi[0..4] × 2γ[0..2]` requires multiple IFMA pairs. This expands the register budget by ~3-4 ZMM registers. The normalization discipline (`_nn` variants) still applies but bounds analysis needs redoing for the wider reduction.
- **IFMA 8-way batch** (`fe51x8`): Same schoolbook, same reduction change. The `FE51X8_MUL19(x)` shift-and-add macro becomes a multi-limb reduction sequence. This is the most register-pressure-constrained path (ed25519 uses 28/32 ZMM registers); the wider reduction may require spilling or restructuring.

**Square root**: q ≡ 3 (mod 4), so simply compute a^((q+1)/4). This is the fast path.

**Inversion**: Same approach as F_p — Fermat exponentiation (q−2) via addition chain for both constant-time and variable-time paths. The chain macros from ed25519 (`fe_ifma_chain.h` pattern) provide the template for normalizing vs non-normalizing chained operations.

### 4.3 Constant-Time Discipline

All field operations on secret-dependent values must be constant-time:

- No branches on secret-dependent conditions. Use constant-time conditional select via the `ct_barrier` + XOR-blend pattern from ed25519's `fe_cmov.h`: `mask = 0 - (uint64_t)ct_barrier_u32(b); f[i] ^= mask & (f[i] ^ g[i])`. The `ct_barrier_u32`/`ct_barrier_u64` functions (`ct_barrier.h`) use inline asm constraints on GCC/Clang and volatile round-trips on MSVC to prevent the compiler from reasoning about the mask value.
- No secret-dependent memory access patterns. Precomputed table lookups must use full-table scan with masked selection, exactly as in ed25519's `ge_precomp_cmov.h` and `ge_cached_cmov.h`.
- No variable-time instructions. On x86, `div` is variable-time; never use it in the hot path. `imul` is constant-time on all modern x86 microarchitectures.
- Compiler auto-vectorization must be suppressed globally (as in ed25519's CMakeLists.txt: `-fno-tree-vectorize` for GCC, `-fno-vectorize -fno-slp-vectorize` for Clang) to prevent the compiler from introducing data-dependent SIMD instructions that break constant-time guarantees.
- Verification paths (explicitly tagged in the API) may use variable-time operations including early-exit comparisons and non-constant-time table lookups.
- Secure erasure of secret data must use `helioselene_secure_erase` which employs platform-specific tricks the compiler cannot optimize away (replicating ed25519's `SecureZeroMemory` / `memset_s` / `explicit_bzero` / volatile-function-pointer chain).

---

## 5. Curve Arithmetic

### 5.1 Coordinate Systems

Both Helios and Selene are short Weierstrass curves of prime order with a = −3. The natural coordinate system choices are:

**Jacobian coordinates (X:Y:Z)** where x = X/Z², y = Y/Z³:
- **Doubling** with a = −3: 3M + 4S (using the `a = −3` optimization where `3x² + a = 3(x−1)(x+1)` in affine, translating to `3(X−Z²)(X+Z²)` in Jacobian). This is the primary reason the curves were constructed with a = −3.
- **Mixed addition** (Jacobian + Affine): 7M + 4S. Critical for precomputed-table scalar multiplication.
- **General addition** (Jacobian + Jacobian): 11M + 5S.

**Extended Jacobian (X:Y:Z:aZ⁴)** as an alternative that caches aZ⁴:
- Doubling: 4M + 4S (saves the re-computation of aZ⁴). Worth benchmarking against standard Jacobian for the double-heavy patterns in scalar mul.

We will implement Jacobian as the primary representation with affine conversion only at input/output boundaries.

### 5.2 Point Validation (Mandatory)

Due to weak twist security (~99/107 bits), **every point received from an external source must be validated**:

1. Coordinates are in the correct field (0 ≤ x, y < field_modulus).
2. Point satisfies the curve equation: y² = x³ − 3x + b (mod field_modulus).
3. Point is not the identity (for contexts where identity is invalid).
4. If in a subgroup-sensitive context: since both curves have prime order (cofactor 1), any on-curve point other than identity is in the correct subgroup. No cofactor multiplication needed.

**Serialization convention**: All field elements and scalars are serialized as 32-byte **little-endian** byte arrays, matching the Ed25519/Curve25519 convention used in the ed25519 library (`fe_tobytes`/`fe_frombytes`). Point compression uses **32-byte high-bit packing**: the x-coordinate is serialized little-endian with the y-coordinate parity stored in bit 255 (the top bit of byte 31). This works because both p and q are less than 2^255, so bit 255 of any canonical field element is always 0. Decompression: mask off bit 255 to recover x, compute y² = x³ − 3x + b, take square root (q ≡ 3 mod 4: exponentiate by (q+1)/4; p ≡ 5 mod 8: Atkin's method), select the root matching the parity bit.

The API should make it impossible to use an unvalidated external point in a secret-dependent operation.

### 5.3 Scalar Multiplication

**Secret-path (constant-time)**:
- Fixed-window method with width w=5 (2^(w-1) = 16 precomputed points).
- Precomputation uses mixed coordinates; table stored in affine.
- Table lookup via constant-time full-scan: iterate over all 16 entries, conditionally selecting the correct one.
- Main loop: 51 window positions, each consisting of 5 doublings followed by 1 mixed addition from the precomputed table (total: 255 doublings + 51 additions).
- Regular recoding of the scalar to avoid branches on zero digits (use signed digit representation with no zeros).

**Variable-time verification path**:
- wNAF (width-5 Non-Adjacent Form) with standard precomputation.
- Skip zero digits with early-exit.
- Interleaved double-and-add.
- ~30% faster than constant-time for single scalar mul.

### 5.4 Multi-Scalar Multiplication (Pippenger / Bucket Method)

This is the single most critical operation for Bulletproofs and curve tree hashing. The FCMP++ competition showed that multiexp dominates total runtime. **The entire MSM orchestration layer is proven in `gibme-c/ed25519` and transfers directly** — only the inner point-addition calls change.

**Algorithm selection** (from ed25519's `ge_multiscalar_mul_vartime.h`):
- **Straus (interleaved)** for n ≤ 32: signed 4-bit fixed-window with per-point precomputed tables, 252 shared doublings + ~n×32 additions. The 8-way IFMA Straus variant (`ifma/ge_multiscalar_mul_vartime.cpp`) processes groups of 8 points in parallel using `fe51x8` field arithmetic.
- **Pippenger (bucket method)** for n > 32: signed w-bit windows with bucket sorting. Window size heuristic from `pippenger_window_size()`: w=5 for n<96, w=6 for n<288, ..., w=11 for n≥23328.

**Pippenger's algorithm** (proven implementation in ed25519, three variants: x64/avx2/ifma):
- For n scalar-point pairs, partition each scalar into c-bit windows (c ≈ log2(n)).
- Signed digit encoding (`encode_signed_wbit`): carry-propagated radix-2^w encoding producing digits in [−2^(w−1), 2^(w−1)], halving bucket count.
- For each window position: accumulate points into 2^(c−1) buckets (signed encoding), then running-sum combination.
- Horner evaluation: multiply accumulated result by 2^w between windows.
- For n=256 (typical Bulletproof inner-product), optimal c≈8, giving ~4x speedup over naive.
- For n=4096+ (curve tree layers), c≈12 is optimal.

**Adaptation for Jacobian coordinates**: The ed25519 Pippenger uses `ge_p3_to_cached` → `ge_add` → `ge_p1p1_to_p3` for bucket accumulation. Helioselene replaces this with Jacobian `point_to_cached` → `jacobian_add` → `result_to_jacobian`, same flow, different formulas. The IFMA variants use `ge_add_ifma` / `ge_p3_to_cached_ifma` etc. which inline the IFMA field arithmetic — the same pattern applies for helioselene Jacobian operations with IFMA.

**Batch affine conversion**: When accumulating bucket sums, use Montgomery's batch inversion trick to convert multiple Jacobian→affine simultaneously. One inversion + 3(n−1) multiplications instead of n inversions.

**SIMD acceleration of Pippenger**: The bucket accumulation phase is embarrassingly parallel across buckets. The ed25519 library demonstrates this at three tiers: x64 scalar (baseline), AVX2 4-way (`avx2/ge_multiscalar_mul_vartime.cpp`), and IFMA 8-way (`ifma/ge_multiscalar_mul_vartime.cpp`). Each tier uses its corresponding batch field arithmetic (`fe10x4` for AVX2, `fe51x8` for IFMA) to process multiple independent point operations simultaneously.

### 5.5 Hash-to-Curve (Map-to-Curve Only)

For Helios/Selene, we need a mapping from field elements to curve points for generators and for the divisor challenge points. The FCMP++ spec indicates using RFC 9380 (Hashing to Elliptic Curves) with Simplified SWU mapping.

**Division of responsibility**: Helioselene provides only the **map-to-curve** step, not the full hash-to-curve pipeline. The caller is responsible for hashing their input to field elements (via `expand_message_xmd` with SHA-512, BLAKE2b, or whatever hash they use). This mirrors the ed25519 library's `ge_fromfe_frombytes_vartime` — accept serialized field element bytes, return a curve point.

For short Weierstrass curves y² = x³ + ax + b, Simplified SWU mapping requires an isogenous curve with specific properties. Implementation:

1. **Caller** hashes input to two field elements using `expand_message_xmd` (their choice of hash function).
2. **Helioselene** maps each field element to a point on the isogenous curve using SSWU (`map_to_curve_sswu`).
3. **Helioselene** maps both points back to the target curve via the isogeny.
4. **Helioselene** adds the two points (cofactor clearing is trivial since cofactor = 1).

API:
```cpp
// Caller provides pre-hashed field elements, helioselene maps to curve
HeliosPoint map_to_helios(const unsigned char u0[32], const unsigned char u1[32]);
SelenePoint map_to_selene(const unsigned char u0[32], const unsigned char u1[32]);

// Single field element → single point (for callers who want to add themselves)
HeliosPoint map_to_helios_single(const unsigned char u[32]);
SelenePoint map_to_selene_single(const unsigned char u[32]);
```

Note: For Wei25519, torsion clearing is needed after mapping since Ed25519 has cofactor 8 — but that's the caller's concern on the ed25519 side, not helioselene's.

---

## 6. Wei25519 Integration & The 2-Cycle Ladder

Wei25519 is the short Weierstrass form birational to Ed25519/Curve25519. The birationality is:

```
Ed25519: -x² + y² = 1 − (121665/121666)x²y²
    ↕ (birational map)
Wei25519: y² = x³ + ax + b  (over F_p, order 8ℓ where ℓ = |Helios|)
```

The Ed25519 prime-order subgroup of order ℓ shares its order with the Helios group order q. This is the bridge: a point in the Ed25519 prime-order subgroup, when mapped to Wei25519 and then having its x-coordinate extracted, yields a Selene scalar.

**Implementation requirements**:
- Birational map constants: The Wei25519 curve constants (a, b in y² = x³ + ax + b) are derived from Ed25519 parameters and stored as compile-time constants in helioselene. The actual Ed25519 → Wei25519 coordinate transform is the **caller's responsibility** — helioselene doesn't need to know about Ed25519 point types.
- x-coordinate ingestion: Helioselene exposes a function like `selene_scalar_from_bytes(const uint8_t[32])` that accepts a 32-byte little-endian field element (the Wei25519 affine x-coordinate) and returns a Selene scalar. The caller's ed25519 library handles the point decompression, cofactor clearing, and birational map — then passes the resulting x-coordinate to helioselene.
- **No ed25519 dependency**: Helioselene is a standalone library. The `ge_p3` structure from ed25519 is not referenced — the API boundary is raw bytes. This keeps helioselene decoupled and lets downstream consumers use any Ed25519 implementation (libsodium, gibme-c/ed25519, dalek via FFI, etc.).

**Tree structure** (for downstream Curve Trees):
```
Leaves:       [O.x, I.x, C.x, ...]           (Selene scalars from Wei25519 points)
Layer 1:      Pedersen hash → Selene points    (hash Selene scalars → Selene curve)
Layer 2:      Convert Selene points → Helios scalars, Pedersen hash → Helios points
Layer 3:      Convert Helios points → Selene scalars, Pedersen hash → Selene points
  ...alternating up to root...
```

---

## 7. Pedersen Vector Commitments

Central to both Bulletproofs and Curve Trees. A Pedersen vector commitment to scalars (a₁, ..., aₙ) with blinding factor r is:

```
C = r·H + a₁·G₁ + a₂·G₂ + ... + aₙ·Gₙ
```

This is exactly a multi-scalar multiplication, so the Pippenger implementation from §5.4 is the workhorse here. The generators G₁...Gₙ and H are fixed and known ahead of time, which enables:

- **Precomputed tables for fixed generators**: Compute and cache wNAF lookup tables for each generator at startup. This converts each scalar-mul-by-generator into a series of additions against precomputed affine points.
- **Combined fixed+variable multiexp**: When some bases are fixed (generators) and some are variable (in verification), split the computation and use the optimal strategy for each.

Chunk widths for the tree (how many scalars per Pedersen hash) are defined per-curve in the FCMP++ spec.

---

## 8. Downstream Interface: Bulletproofs & Curve Trees

While the full Bulletproofs and Curve Trees implementations are downstream projects, this library must expose the right primitives:

### 8.1 For Generic Bulletproofs (GBPs)

- Multi-scalar multiplication (the inner-product argument core loop).
- Scalar arithmetic in both F_p and F_q (for cross-curve circuit statements).
- Pedersen commitments with configurable generator sets.
- Point serialization/deserialization with mandatory validation.
- Transcript/Fiat-Shamir binding (provide a clean interface for feeding curve points into a hash transcript).

### 8.2 For Curve Trees

- Pedersen hash: multi-scalar mul with chunk-width points, returning a curve point.
- Point-to-scalar conversion: extract x-coordinate of a curve point as a scalar in the next curve's field. For Selene→Helios: take Selene point (x, y) ∈ F_q × F_q, output x as an element of F_q = Helios scalar field. For Helios→Selene: take Helios point (x, y) ∈ F_p × F_p, output x as an element of F_p = Selene scalar field.
- Batch tree hashing: given a layer of scalars, produce the next layer of points using chunked Pedersen hashes. This should exploit parallelism across independent chunks.

### 8.3 For EC-Divisors (First-Class Module)

The divisor computation (proving that a set of points sums to zero via the divisor witness a(x) − y·b(x)) requires polynomial arithmetic over the base field. **This is included as a first-class module within helioselene, not an external dependency.**

The module provides:
- Polynomial arithmetic over F_p and F_q: multiplication, division, evaluation, interpolation.
- Polynomial multiplication strategy: schoolbook for small degree (n ≤ 64), Karatsuba for moderate degree (64 < n ≤ 1024), and a built-in NTT/FFT implementation for larger polynomials if the field supports sufficient 2-adicity (q−1 divisibility by powers of 2 needs verification — if insufficient, Bluestein's algorithm or Schönhage–Strassen).
- Point evaluation of divisors along challenge lines.
- SIMD-accelerated polynomial operations: the field element vectors used for polynomial coefficients naturally benefit from the same AVX2/IFMA batch arithmetic used elsewhere in the library (`fe10x4` / `fe51x8` for 4-way/8-way parallel coefficient operations).

API:
```cpp
namespace helioselene::poly {

// Polynomial over a field (coefficients stored low-degree first)
template<typename Fe>  // Fe = Fp or Fq
class Poly {
public:
    static Poly from_coeffs(const Fe *coeffs, size_t n);
    static Poly from_roots(const Fe *roots, size_t n);  // (x - r0)(x - r1)...
    
    size_t degree() const;
    Fe evaluate(const Fe& x) const;
    
    Poly operator*(const Poly& rhs) const;  // auto-selects schoolbook/Karatsuba/NTT
    Poly operator+(const Poly& rhs) const;
    Poly operator-(const Poly& rhs) const;
    std::pair<Poly, Poly> divmod(const Poly& divisor) const;
};

// EC-divisor witness: D = a(x) - y·b(x)
template<typename Point, typename Fe>
struct Divisor {
    Poly<Fe> a;  // numerator polynomial
    Poly<Fe> b;  // y-coefficient polynomial
};

// Compute divisor witness for a set of points summing to zero
template<typename Point, typename Fe>
Divisor<Point, Fe> compute_divisor(const Point *points, size_t n);

// Evaluate divisor at a challenge point (for proof generation)
template<typename Fe>
Fe evaluate_divisor(const Divisor<auto, Fe>& d, const Fe& x, const Fe& y);

} // namespace helioselene::poly
```

The ec-divisors competition showed that FFT-based polynomial multiplication yielded a 95%+ improvement (Fabrizio's submission). This module must deliver comparable performance.

---

## 9. Platform & SIMD Strategy

### 9.1 Build Matrix

Mirrors the ed25519 platform matrix, which has been validated across MSVC, GCC, Clang, AppleClang, and MinGW:

| Target | Field Repr | SIMD | ed25519 Reference | Notes |
|---|---|---|---|---|
| 32-bit (ARM32, WASM) | 10×26-bit limbs | None (scalar) | `ref10/` directory | Must still be constant-time. WASM via Emscripten. |
| ARM64 (AArch64) | 5×51-bit limbs | None (scalar) | `x64/fe51_inline.h` | Uses `__int128` natively. No NEON SIMD paths initially. |
| x86_64 baseline | 5×51-bit limbs | None (scalar) | `x64/fe51_inline.h` | Hand-tuned with `__int128`/`_umul128`. Audit reference. |
| x86_64 + AVX2 | 10×26-bit limbs | `vpmuludq` 4-way | `x64/avx2/fe10x4_avx2.h` | 4 independent field ops per instruction. |
| x86_64 + AVX-512 IFMA (single) | 5×51-bit limbs | `vpmadd52lo/hi` | `x64/ifma/fe_ifma.h` | Single-op speedup via IFMA field mul. |
| x86_64 + AVX-512 IFMA (batch) | 5×51-bit limbs | `vpmadd52lo/hi` 8-way | `x64/ifma/fe51x8_ifma.h` | 8 independent field ops per instruction. |

ARM64 NEON SIMD paths are out of scope for initial implementation. The scalar 64-bit path is already fast on ARM64 due to native `__int128` support and wide multiply instructions (`umulh`). NEON field arithmetic could be a future optimization but ARM64 lacks an equivalent to IFMA's fused multiply-accumulate for 52-bit integers.

### 9.2 Runtime Dispatch

**Reuse the ed25519 dispatch model directly.** The pattern:

1. Static dispatch table initialized to x64/ref10 baseline at program startup (always correct without init).
2. `helioselene_init()`: CPUID heuristic selects best backend (~microseconds). Thread-safe via `std::once_flag`.
3. `helioselene_autotune()`: Per-function benchmarking selects empirically fastest backend (~1-2 seconds).

The dispatch table struct mirrors `ed25519_dispatch_table` but with Jacobian point signatures:

```cpp
struct helioselene_dispatch_table {
    // Helios operations
    void (*helios_scalarmult_ct)(helios_jacobian *, const unsigned char *, const helios_jacobian *);
    void (*helios_scalarmult_ct_batch)(helios_jacobian *, const unsigned char *, const helios_jacobian *, size_t);
    void (*helios_msm_vartime)(helios_jacobian *, const unsigned char *, const helios_jacobian *, size_t);
    // Selene operations (mirror)
    void (*selene_scalarmult_ct)(selene_jacobian *, const unsigned char *, const selene_jacobian *);
    void (*selene_scalarmult_ct_batch)(selene_jacobian *, const unsigned char *, const selene_jacobian *, size_t);
    void (*selene_msm_vartime)(selene_jacobian *, const unsigned char *, const selene_jacobian *, size_t);
};
```

### 9.3 SIMD Implementation Notes

**AVX2 field multiplication (F_q, radix-2^26)**: Clone `fe10x4_mul` schoolbook phase from ed25519. The product accumulation (100 `_mm256_mul_epu32` + `_mm256_add_epi64` calls) is identical. The reduction tail is a multi-limb Crandall reduction (not a single-constant swap like F_p's ×19). The 2p bias constants for subtraction become 2q bias constants.

**AVX-512 IFMA field multiplication (F_q)**: Clone `fe_mul_ifma_core` schoolbook phase. The `_mm512_alignr_epi64` structure and `vpmadd52lo/hi` accumulation are identical. The fold-back becomes a multi-step Crandall reduction: `hi[0..4] × 2γ[0..2]` requires multiple IFMA multiply-accumulate pairs instead of the single ×19 fold. Adds ~3-4 ZMM registers of pressure.

**8-way batch IFMA (F_q)**: Clone `fe51x8_mul` schoolbook phase. The single-instruction `FE51X8_MUL19(x)` wrap becomes a multi-limb Crandall reduction sequence. Register pressure is the key constraint — ed25519 already uses 28/32 ZMM registers, so the wider reduction may require spilling or restructuring the accumulator layout.

**Where SIMD pays off most**: Pippenger bucket accumulation (many independent additions, proven in ed25519 at all three SIMD tiers), batch point validation, batch Pedersen hashing (independent chunks), and polynomial evaluation for divisors.

---

## 10. Security Hardening

### 10.1 Memory Safety

- No custom allocator. Stack allocation for field elements, point types, and scalar mul intermediates. `std::vector` for MSM heap allocations (bucket arrays, digit encodings) — these hold variable-time public data and don't need secure allocation.
- `helioselene_secure_erase()` utility (replicating ed25519's platform-abstracted `SecureZeroMemory` / `memset_s` / `explicit_bzero` / volatile function pointer chain) exposed for callers to manage their own secret lifecycle.
- Constant-time scalar multiplication paths call `secure_erase` on stack locals (precomputed tables, intermediate scalars) before return as defense-in-depth.
- No dynamic allocation in the field arithmetic hot path (all stack-allocated limb arrays).

### 10.2 Side-Channel Countermeasures

- **Timing**: All secret-path operations constant-time as described in §4.3. Validate with `timecop`/`ctgrind`-style tools (valgrind with secret-marking).
- **Power/EM**: Point blinding for scalar multiplication — randomize the base point representation before starting the ladder: replace P with P + r·G for random r, subtract r·G from result. This randomizes intermediate values against DPA.
- **Cache**: No secret-dependent array indexing. Table lookups via linear scan with conditional move. This is the single largest performance cost of constant-time code and must be benchmarked carefully.

### 10.3 Input Validation

- Every deserialized point passes on-curve check (§5.2). No exceptions.
- Scalar inputs are reduced mod group order before use.
- The API should make invalid states unrepresentable where possible (e.g., `ValidatedPoint` type that can only be constructed through validation).
- Reject identity point in contexts where it's invalid (e.g., generator inputs to Pedersen).

### 10.4 Testing Strategy

- **Known-answer tests**: Test vectors generated from the Rust `helioselene` crate and cross-checked against SageMath.
- **Edge cases**: Identity point, points at infinity, x=0 (should be impossible by construction but test anyway), maximum-value scalars, zero scalar.
- **Constant-time validation**: Run the test suite under `valgrind --tool=callgrind` with secret-marked memory to detect branches on secrets. Integrate with CI.
- **Fuzzing**: AFL/libfuzzer on deserialization, point validation, and arithmetic operations.
- **Cross-validation**: For every operation, compare C++ output against Rust helioselene output for a large random sample.

---

## 11. API Design

```cpp
namespace helioselene {

// Field elements (opaque, constant-time by default)
class Fp { /* 2^255 - 19 */ };
class Fq { /* Crandall prime */ };

// Points on each curve
class HeliosPoint {
public:
    // 32-byte compressed: x-coordinate LE with y parity in bit 255 (top bit)
    // Both p,q < 2^255 so bit 255 of canonical x is always 0 — free for sign bit
    static std::optional<HeliosPoint> from_bytes(const unsigned char compressed[32]);
    static HeliosPoint generator();
    std::array<uint8_t, 32> to_bytes() const;
    
    Fp x_coordinate() const;  // Helios lives over F_p; x-coord is an F_p element = SeleneScalar
    bool is_identity() const;

    HeliosPoint operator+(const HeliosPoint& rhs) const;
    HeliosPoint operator-() const;
    HeliosPoint dbl() const;
};

class SelenePoint { /* mirror of HeliosPoint over Fq */ };

// Scalar types
class HeliosScalar { /* element of Fq */ };
class SeleneScalar { /* element of Fp */ };

// Scalar multiplication
HeliosPoint scalar_mul(const HeliosScalar& s, const HeliosPoint& p,
                       bool constant_time = true);

// Multi-scalar multiplication (Pippenger)
HeliosPoint multi_scalar_mul(
    const HeliosScalar *scalars,
    const HeliosPoint *points,
    size_t n,
    bool constant_time = false);  // default variable-time for verification

// Pedersen commitment
HeliosPoint pedersen_commit(
    const HeliosScalar *values,
    const HeliosPoint *generators,
    size_t n,
    const HeliosScalar& blinding,
    const HeliosPoint& blinding_gen);

// Wei25519 bridge — accepts raw x-coordinate bytes, caller handles Ed25519 → Wei25519 transform
SeleneScalar selene_scalar_from_wei25519_x(const unsigned char x_bytes[32]);

// Map-to-curve (caller provides pre-hashed field elements, not raw messages)
HeliosPoint map_to_helios(const unsigned char u0[32], const unsigned char u1[32]);
SelenePoint map_to_selene(const unsigned char u0[32], const unsigned char u1[32]);

// Point serialization for transcript binding (caller manages their own transcript/hash)
std::array<uint8_t, 32> point_to_transcript_bytes(const HeliosPoint& p);
std::array<uint8_t, 32> point_to_transcript_bytes(const SelenePoint& p);

} // namespace helioselene
```

Key design decisions:
- **Two-layer API**: Internally, the library uses C-style structs and function pointers (matching ed25519's patterns) for the dispatch table and SIMD backends — e.g., `helios_jacobian` as a plain struct of `fp_fe` arrays, free functions like `helios_point_dbl(helios_jacobian *r, const helios_jacobian *p)`. The public C++ API (`class HeliosPoint`, `class SelenePoint`) wraps these internal types with validation, RAII, and type safety. The dispatch table (§9.2) operates on the internal C-style types; the public API calls through the dispatch table.
- `constant_time` parameter is explicit, not hidden. Caller must opt into variable-time.
- Points returned from deserialization are `std::optional` — validation failure is a normal code path, not an exception.
- Multi-scalar mul defaults to variable-time because its primary use case is verification.
- Generator tables are lazily initialized and cached as `static` data.

---

## 12. Build System & Dependencies

**Build**: CMake 3.10+ (matching ed25519), with architecture detection mirroring ed25519's `CMakeLists.txt`.

**Dependencies**: **None.** Helioselene is a fully standalone, zero-dependency static library.

- **No `gibme-c/ed25519` at build time.** The ed25519 library is the reference architecture we study and adapt patterns from, not a link-time dependency. The downstream consumer (FCMP++ layer, Monero, etc.) already has ed25519 in their stack and is responsible for the Ed25519 → Wei25519 coordinate transform. Helioselene accepts raw field element bytes / affine coordinates at the API boundary.
- **No hash library.** Hash-to-curve accepts a pre-computed digest (field elements) and maps it to a curve point. The caller handles the hash function (SHA-512, BLAKE2b, etc.) and `expand_message_xmd` externally, then passes the resulting field elements to helioselene's mapping function. This mirrors the ed25519 library's `ge_fromfe_frombytes_vartime` pattern — accept bytes, map to point — and ensures consistency between the two library APIs.
- **No wide integer types.** The 32-bit path uses `int32_t[10]` limbs in radix-2^25.5 (alternating 26/25-bit) where every pairwise product fits in `int64_t`, exactly as ed25519's ref10 does. Scalar arithmetic operates on raw byte arrays with schoolbook long multiplication and reduction — no `uint256`/`uint512` needed. Ed25519 proves this approach is sufficient.
- **EC-divisor polynomial arithmetic is included** as a first-class module within the library (see §8.3). No external FFT/NTT dependency.
- No Boost. No STL allocators for secret data. Minimal `<algorithm>` use.

**Compile flags** (mirroring ed25519's proven configuration):
```cmake
# Global flags — matches ed25519 exactly
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Wall -Wextra -Wuninitialized -fstack-protector-strong")

# CRITICAL: Suppress auto-vectorization to protect constant-time guarantees
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-tree-vectorize")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-vectorize -fno-slp-vectorize")
endif()

# Per-TU SIMD flags — same pattern as ed25519
foreach(IFMA_SRC ${HELIOSELENE_IFMA_SRC})
    set_source_files_properties(${IFMA_SRC} PROPERTIES COMPILE_OPTIONS "-mavx512f;-mavx512ifma")
endforeach()
foreach(AVX2_SRC ${HELIOSELENE_AVX2_SRC})
    set_source_files_properties(${AVX2_SRC} PROPERTIES COMPILE_OPTIONS "-mavx2")
endforeach()
```

Note: ed25519 uses `-O3` for release builds. This is safe because auto-vectorization is explicitly disabled via the flags above. The SIMD paths are hand-written and explicitly enabled per-TU, not compiler-generated. We follow the same approach rather than the more conservative `-O2`.

---

## 13. Phased Rollout

**Timeline reduction**: Because the `gibme-c/ed25519` library provides proven implementations of the platform detection, SIMD dispatch, constant-time primitives, MSM orchestration, and build system patterns, the overall effort shifts from "build from scratch" to "adapt proven architecture." The SIMD paths in particular go from high-risk R&D to systematic adaptation — clone, swap reduction constants, swap point formulas, cross-validate. This compresses the original 20-week estimate significantly.

### Phase 1: Scaffolding & Field Arithmetic (Weeks 1–3)

- Fork ed25519's platform detection (`ed25519_platform.h`), CT barriers (`ct_barrier.h`), secure erase, and CPUID infrastructure as `helioselene_platform.h` etc.
- **F_p scalar 64-bit**: Lift directly from ed25519 `x64/fe51_inline.h` and the `x64/fe_*.cpp` files. These operate over the same field — this is a copy, not an adaptation. Cross-validate against SageMath.
- **F_q scalar 64-bit**: Clone F_p implementation, replace carry-wrap ×19 with Crandall ×2γ reduction. This is the primary novel work in Phase 1. Cross-validate against SageMath and the Rust `helioselene` crate.
- Implement 32-bit fallback for both fields (clone ed25519 `ref10/` path with `int32_t[10]` limbs, adapt reduction for F_q). No wide integer types — schoolbook limb arithmetic with `int64_t` products, byte-level scalar operations.
- Benchmark: target ≤ 40ns for F_p field mul (should match ed25519), ≤ 60ns for F_q (Crandall overhead).

### Phase 2: Curve Arithmetic & Wei25519 Bridge (Weeks 4–6)

- Jacobian point types (mirror ed25519's `ge_p2`/`ge_p3`/`ge_p1p1`/`ge_cached` hierarchy but for Weierstrass).
- Point operations: doubling (a=−3 optimization, 3M+4S), mixed addition (7M+4S), general addition (11M+5S).
- Scalar multiplication: constant-time fixed-window (adapt ed25519's `ge_scalarmult_ct` pattern — same table scan, different point formulas) + variable-time wNAF (adapt `ge_double_scalarmult_negate_vartime` pattern).
- Point serialization/deserialization with mandatory validation.
- Wei25519 bridge: `selene_scalar_from_wei25519_x()` accepting raw 32-byte x-coordinate. The caller's ed25519 library handles the Ed25519 → Wei25519 transform externally — helioselene just ingests the resulting field element.
- Cross-validate all operations against Rust `helioselene` crate.

### Phase 3: Multi-Scalar Mul, Pedersen, & EC-Divisors (Weeks 7–9)

- MSM: Clone ed25519's `ge_multiscalar_mul_vartime.cpp` (x64 baseline). Replace point operations with Jacobian equivalents. The signed-digit encoding, Straus/Pippenger selection, window-size heuristic, and bucket accumulation + running-sum structure transfer directly.
- Pedersen vector commitments (multi-scalar mul with fixed generators).
- Map-to-curve (SSWU per RFC 9380): accepts pre-hashed field elements, maps to curve points. Caller handles the hash function externally.
- EC-divisor polynomial arithmetic module: schoolbook, Karatsuba, and NTT/FFT polynomial multiplication over F_p and F_q. Polynomial division, evaluation, interpolation. Verify F_q 2-adicity for NTT applicability.
- Benchmark against Rust reference: target ≥ parity, ideally 20%+ faster.

### Phase 4: SIMD Paths (Weeks 10–14)

- **AVX2 field arithmetic**: Clone ed25519's `fe10x4_avx2.h` for F_p (direct copy) and F_q (swap reduction constant). Clone `fe10_avx2.h` single-element AVX2 path.
- **AVX-512 IFMA single-op**: Clone `fe_ifma.h`, swap reduction tail for F_q.
- **AVX-512 IFMA 8-way batch**: Clone `fe51x8_ifma.h`, swap `FE51X8_MUL19` to `FE51X8_MUL2GAMMA`.
- **SIMD point operations**: Adapt ed25519's IFMA chain macros (`fe_ifma_chain.h`) and inline point operations (`ge_add_ifma`, `ge_p2_dbl_ifma`, etc.) to Jacobian formulas.
- **SIMD MSM**: Adapt ed25519's AVX2 and IFMA Pippenger variants.
- Runtime dispatch table and `helioselene_init()` / `helioselene_autotune()` (clone ed25519 dispatch infrastructure).
- Performance validation: target 2–3x throughput improvement over scalar for multi-scalar mul (matching ed25519's observed gains).

### Phase 5: Hardening & Audit Prep (Weeks 15–18)

- `helioselene_secure_erase()` integration in constant-time scalar multiplication paths.
- Comprehensive fuzzing campaign (AFL/libfuzzer on deserialization, validation, arithmetic).
- Side-channel testing: constant-time validation under valgrind with secret-marked memory.
- API finalization and documentation.
- Test vector generation for downstream consumers.
- Cross-validation: compare all outputs against Rust `helioselene` for large random samples.
- Package for independent security audit.

---

## 14. Open Questions & Decision Points

1. **Curve parameters**: **RESOLVED (architecturally)** — The library isolates all curve-specific constants (b, generator coordinates, isogeny coefficients for SSWU) in dedicated headers so they can be swapped without touching arithmetic code. The Veridise assessment recommends re-searching with twist security as a criterion (e.g., D = −31617403 or D = −31750123 give 200+ bit twist security), but that's a Monero-level decision. If parameters change, it's a header swap and base point table regeneration — the field primes (p, q) stay the same, so all field arithmetic and SIMD paths are unaffected.

2. **Ed25519 library choice**: **RESOLVED** — Helioselene has no build-time or link-time dependency on any ed25519 library. The Wei25519 bridge accepts raw bytes at the API boundary (a 32-byte x-coordinate). The downstream caller (FCMP++ layer, Monero, etc.) owns the Ed25519 → Wei25519 coordinate transform using whatever ed25519 implementation they already have. `gibme-c/ed25519` (development branch) is the **reference architecture** we study for patterns, not a dependency.

3. **Allocator model**: **RESOLVED** — Same approach as ed25519: no custom allocator. Stack allocation for the hot path (field elements, point types, scalar mul intermediates), `std::vector` for MSM heap allocations (bucket arrays, digit encodings). `helioselene_secure_erase()` exposed as a utility for callers to manage their own secret lifecycle. No RAII wrappers, no mlock, no arena. Constant-time scalar multiplication paths will `secure_erase` stack locals before return as a defense-in-depth measure.

4. **FFT for ec-divisors**: **RESOLVED** — EC-divisor polynomial arithmetic is a first-class module within helioselene with no external dependency. The Fq field's 2-adicity (q−1 divisibility by powers of 2) still needs verification to determine whether NTT is viable or whether Bluestein's/Schönhage–Strassen is needed for large-degree polynomials.

5. **WASM target**: **RESOLVED** — 32-bit WASM follows the generic ref10 32-bit path via Emscripten. No special WASM-specific optimizations. The ref10 `int32_t[10]` representation works natively in WASM's 32-bit integer arithmetic.

---

## 15. Maintenance Notes

- **Shared F_p code**: Since ed25519 and helioselene both operate over F_p with identical arithmetic, there will be two copies of the same field implementation. This is intentional (helioselene is standalone), but the implementations should be periodically cross-validated to ensure they stay in sync for correctness.

---

## 16. Reference Materials

- **`gibme-c/ed25519` (development branch)** — Reference architecture for platform detection, SIMD dispatch, field arithmetic, MSM, constant-time primitives, and build system. https://github.com/gibme-c/ed25519/tree/development
- Veridise Security Assessment for Helios–Selene (Bassa & Sepanski, Aug 2025)
- FCMP++ Specification (kayabaNerve) — circuit, gadgets, tree structure
- FCMP++ Optimization Competition results (Lederstrumpf's winning helioselene submission)
- Monero PR #9436 (j-berman) — C++/Rust FFI integration reference
- Handbook of Elliptic and Hyperelliptic Curve Cryptography (Cohen et al.) — Chapters 13–14 for implementation formulas
- Guide to ECC (Hankerson, Menezes, Vanstone) — Chapters 3–4 for coordinate systems and scalar mul
- djb's curve25519 paper and donna implementations — F_p arithmetic reference
- Bernstein-Yang constant-time GCD (https://eprint.iacr.org/2019/266) — field inversion
- Pippenger's algorithm as described in Daniel J. Bernstein's "Pippenger's exponentiation algorithm"
- RFC 9380: Hashing to Elliptic Curves — hash-to-point specification