# helioselene

A standalone, zero-dependency C++17 elliptic curve library implementing the Helios/Selene curve cycle for [FCMP++](https://github.com/kayabaNerve/fcmp-plus-plus) integration. It replaces the Rust FFI approach with a native C++ implementation.

Helios and Selene are a pair of prime-order short Weierstrass curves that form a **cycle**: Helios operates over the Ed25519 base field **F_p** (p = 2^255 - 19), and its group order is the base field of Selene, and vice versa. This cycle property makes them ideal for recursive proof composition -- a proof verified on one curve produces elements that live natively in the other curve's scalar field, eliminating expensive non-native field arithmetic.

Both curves have the form **y² = x³ - 3x + b** (the a = -3 optimization enables faster point doubling). Both are cofactor 1, so every point on the curve is in the prime-order group.

On 64-bit platforms (x86_64, ARM64), the library automatically uses an optimized radix-2^51 backend with 128-bit multiplication. On x86_64, **SIMD-accelerated backends** -- AVX2 and AVX-512 IFMA -- are selected at runtime via CPUID detection.

## Features

- **Two complete curve implementations** -- Helios (over F_p) and Selene (over F_q), with independent field arithmetic, point operations, and scalar math for each
- **Field arithmetic** over F_p (2^255 - 19) and F_q (2^255 - γ, a Crandall prime) -- add, subtract, multiply, square, invert, square root, and more
- **Jacobian coordinate curve operations** -- point addition, mixed addition, doubling with a = -3 optimization (3M+5S)
- **Constant-time scalar multiplication** -- signed 4-bit fixed-window with no secret-dependent branches or memory access patterns
- **Variable-time scalar multiplication** -- wNAF w=5 for verification and public-data operations
- **Multi-scalar multiplication** -- Straus (n ≤ 32) and Pippenger (n > 32) with signed-digit encoding and bucket accumulation
- **Hash-to-curve** -- RFC 9380 Simplified SWU (SSWU) mapping from field elements to curve points
- **Pedersen commitments** -- `r*H + Σ(s_i * P_i)` computed via MSM
- **Batch affine conversion** -- Montgomery's trick for converting multiple Jacobian points to affine coordinates with a single inversion
- **Polynomial arithmetic** -- multiplication (schoolbook, Karatsuba, ECFFT), evaluation, interpolation, division, and construction from roots (see [Polynomials](#polynomials) below)
- **EC-divisor witnesses** -- compute and evaluate divisor polynomials a(x) - y·b(x) for sets of curve points
- **Wei25519 bridge** -- converts Ed25519 x-coordinates (as raw 32-byte values) to Selene scalars
- **Secure memory erasure** -- `helioselene_secure_erase` zeros secret data using platform-specific methods the compiler can't optimize away
- **SIMD acceleration** (x86_64) -- runtime-dispatched AVX2 and AVX-512 IFMA backends for scalar multiplication and MSM, with automatic CPU feature detection via CPUID
- **Cross-platform** -- MSVC, GCC, Clang, MinGW

## Building

Requires CMake 3.10+ and a C++17 compiler. No external dependencies.

```bash
# Configure
cmake -S . -B build -DBUILD_TESTS=ON

# Build
cmake --build build --config Release -j

# Run tests
./build/helioselene-tests         # Linux/macOS
.\build\Release\helioselene-tests.exe  # Windows (MSVC)
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | `OFF` | Build unit tests (`helioselene-tests`) and benchmark tool (`helioselene-benchmark`) |
| `BUILD_TOOLS` | `OFF` | Build ECFFT precomputation tools (`helioselene-find-ecfft`, `helioselene-gen-ecfft`) |
| `FORCE_PORTABLE` | `OFF` | Force the 32-bit portable implementation on 64-bit platforms (for testing/comparison) |
| `ENABLE_AVX2` | `ON`* | Enable AVX2 SIMD backend with runtime dispatch |
| `ENABLE_AVX512` | `ON`* | Enable AVX-512 IFMA SIMD backend with runtime dispatch |
| `ENABLE_ECFFT` | `ON` | Enable ECFFT polynomial multiplication for large degrees |
| `ENABLE_LTO` | `OFF` | Enable link-time optimization |
| `ARCH` | `native` | Target CPU architecture for `-march` (`native`, `default`, or a specific arch) |

\* On x86_64 only. Both default to `OFF` on other architectures or when `FORCE_PORTABLE` is set.

## Usage

Include the master header to access all operations:

```cpp
#include "helioselene.h"
```

Or include individual headers for specific operations:

```cpp
#include "fp_mul.h"
#include "helios_scalarmult.h"
#include "fq_invert.h"
```

Link against the `helioselene` static library target in your CMake project:

```cmake
add_subdirectory(helioselene)
target_link_libraries(your_target helioselene)
```

## Architecture

The library is organized in layers, matching the math of elliptic curve cryptography:

1. **Field elements (`fp_*`, `fq_*`)** -- Integers modulo p or q. These are the coordinates of curve points. Two independent fields because the two curves operate over different primes.
2. **Curve points (`helios_*`, `selene_*`)** -- Points on each curve in Jacobian coordinates. Addition, doubling, scalar multiplication, encoding/decoding.
3. **Polynomials (`fp_poly_*`, `fq_poly_*`)** -- Polynomial arithmetic over each field, used by the divisor module and the FCMP++ proof system.
4. **Divisors (`helios_divisor_*`, `selene_divisor_*`)** -- EC-divisor witness computation and evaluation for the FCMP++ membership proof protocol.

### Platform Dispatch

The library uses two levels of dispatch:

**Compile-time dispatch** via `helioselene_platform.h` selects the field element representation:

- **64-bit** (x86_64, ARM64): Field elements are `uint64_t[5]` in radix-2^51. Multiplication uses 128-bit products (`__int128` on GCC/Clang, `_umul128` on MSVC).
- **Everything else**: Falls back to the portable implementation with `int32_t[10]` in alternating 26/25-bit limbs (radix-2^25.5).

The `FORCE_PORTABLE` CMake option forces the 32-bit path on 64-bit platforms for testing.

**Runtime dispatch** (x86_64 only) selects SIMD-accelerated implementations for six hot-path operations:

| Slot | Function |
|------|----------|
| `helios_scalarmult` | Constant-time scalar multiplication (Helios) |
| `helios_scalarmult_vartime` | Variable-time scalar multiplication (Helios) |
| `helios_msm_vartime` | Multi-scalar multiplication (Helios) |
| `selene_scalarmult` | Constant-time scalar multiplication (Selene) |
| `selene_scalarmult_vartime` | Variable-time scalar multiplication (Selene) |
| `selene_msm_vartime` | Multi-scalar multiplication (Selene) |

CPU features (AVX2, AVX-512F, AVX-512 IFMA) are detected via CPUID+XGETBV at startup. Call `helioselene_init()` for fast heuristic selection, or `helioselene_autotune()` to benchmark all available implementations and pick the fastest per-function. Both are thread-safe (only the first call executes; subsequent calls are no-ops).

### SIMD Backends (x86_64)

Two SIMD backends accelerate scalar multiplication and MSM:

**AVX2** -- Scalar multiplication uses `int64_t[10]` radix-2^25.5 representation (`fp10`/`fq10`) to avoid 128-bit multiply overhead while keeping values in-register. MSM Straus uses 4-way horizontal parallelism via `fp10x4`/`fq10x4` (10 × `__m256i`), processing 4 independent bucket accumulations per iteration. MSM Pippenger falls back to x64 scalar (irregular access patterns don't benefit from SIMD).

**AVX-512 IFMA** -- Uses `vpmadd52lo`/`vpmadd52hi` for field element multiplication, replacing the multi-instruction schoolbook multiply with hardware 52-bit fused multiply-accumulate. MSM Straus uses 8-way horizontal parallelism via `fp51x8`/`fq51x8` (5 × `__m512i`), processing 8 independent bucket accumulations per iteration. Scalar multiplication uses the same `fp10`/`fq10` path as AVX2 (IFMA's 8-way parallelism doesn't help single-scalar operations).

### Crandall Reduction

The F_q field uses the Crandall prime 2^255 - γ, where γ ≈ 2^127. Unlike F_p (where reduction by 2^255 - 19 folds back with a single-digit multiply by 19), Crandall reduction requires a wide multiply by the 127-bit value 2γ. This is the fundamental difference from Ed25519-style field arithmetic and the source of most implementation complexity in the Fq backends.

## Polynomials

### What is polynomial degree?

A polynomial is an expression like **3x² + 5x + 1**. The **degree** is the highest power of x that appears -- this example has degree 2. In the context of this library, polynomials are used to represent relationships between curve points. The coefficients are not ordinary numbers but field elements (integers modulo a large prime), and the degree corresponds to the number of points being described.

When FCMP++ constructs a membership proof for a set of transaction outputs, the size of that set determines the degree of the polynomials involved. A proof covering 256 outputs produces polynomials of roughly degree 256. Larger anonymity sets mean higher-degree polynomials, which means more arithmetic -- so polynomial multiplication speed directly affects proof generation time.

### Multiplication strategies

The library automatically selects the best multiplication algorithm based on degree:

| Method | Degree range | Complexity | Description |
|--------|-------------|------------|-------------|
| Schoolbook | < 32 | O(n²) | Direct term-by-term multiplication. Simple and fast at small sizes. |
| Karatsuba | 32 – 1023 | O(n^1.585) | Divide-and-conquer algorithm that trades additions for multiplications. Handles all practical FCMP++ polynomial degrees. |
| ECFFT | ≥ 1024 | O(n²)* | Elliptic Curve Fast Fourier Transform. Uses structured evaluation domains derived from isogeny chains on auxiliary elliptic curves. |

\* The ECFFT's ENTER (evaluation) and EXIT (interpolation) operations are currently O(n²), which means coefficient-space polynomial multiplication via ECFFT cannot beat Karatsuba at any practical size. The ECFFT infrastructure is in place for future protocol-level optimizations that work in the evaluation domain (see below).

### Polynomial API

```cpp
// Multiply: r = a * b (auto-selects schoolbook/Karatsuba/ECFFT)
void fp_poly_mul(fp_poly *r, const fp_poly *a, const fp_poly *b);

// Evaluate polynomial at a point (Horner's method)
void fp_poly_eval(fp_fe result, const fp_poly *p, const fp_fe x);

// Build polynomial from roots: (x - r0)(x - r1)...
void fp_poly_from_roots(fp_poly *r, const fp_fe *roots, size_t n);

// Polynomial division: a = b*q + rem
void fp_poly_divmod(fp_poly *q, fp_poly *rem, const fp_poly *a, const fp_poly *b);

// Lagrange interpolation from (x, y) pairs
void fp_poly_interpolate(fp_poly *out, const fp_fe *xs, const fp_fe *ys, size_t n);
```

All operations are mirrored for F_q (`fq_poly_mul`, `fq_poly_eval`, etc.).

### EC-Divisor Witnesses

EC-divisors represent sets of curve points as polynomial pairs a(x) - y·b(x). The divisor "vanishes" (evaluates to zero) at exactly the points it encodes. This is the core primitive used by FCMP++ to prove set membership without revealing which element is being proven.

```cpp
// Compute divisor for a set of affine points
void helios_compute_divisor(helios_divisor *d, const helios_affine *points, size_t n);

// Evaluate divisor at (x, y) -- returns zero iff the point is in the set
void helios_evaluate_divisor(fp_fe result, const helios_divisor *d, const fp_fe x, const fp_fe y);
```

### ECFFT and Evaluation-Domain Operations

The ECFFT (Elliptic Curve Fast Fourier Transform) replaces the multiplicative subgroups used in classical NTT/FFT with structured point sets derived from isogeny chains on auxiliary elliptic curves. This matters because the prime fields used by Helios and Selene lack the large power-of-2 roots of unity that classical FFT requires.

The ECFFT infrastructure provides ENTER (coefficient → evaluation), EXIT (evaluation → coefficient), EXTEND, and REDUCE operations. While ENTER and EXIT are currently O(n²), the real value of the ECFFT lies in evaluation-domain workflows: once polynomials are represented as evaluation vectors, multiplication becomes O(n) pointwise products, and domain size management uses O(n log n) butterfly operations. Unlocking this performance requires protocol-level changes in how FCMP++ constructs and manipulates divisor polynomials -- a library-level optimization alone is not sufficient.

The precomputed ECFFT data (isogeny chain coefficients and domain cosets) is generated by the `helioselene-gen-ecfft` tool and checked into the repository as `.inl` files. Both fields currently use 16-level domains (65,536 evaluation points).

#### ECFFT Auxiliary Curves

| Field | Auxiliary curve | Domain size | Levels |
|-------|----------------|-------------|--------|
| F_p (2^255 - 19) | y² = x³ + x + 3427 | 65,536 | 16 |
| F_q (2^255 - γ) | y² = x³ - 3x + b | 65,536 | 16 |

These auxiliary curves were selected for having group orders with high 2-adic valuation (large power-of-2 factor), which determines the maximum domain size. **Changing the auxiliary curve breaks backwards compatibility** -- the precomputed `.inl` data encodes the isogeny chain for a specific curve, and all downstream protocol proofs depend on it.

## Constant-Time Discipline

All operations on secret data (private scalars, signing keys) are constant-time:

- No branches on secret-dependent values -- branchless conditional select via `ct_barrier` + XOR-blend
- No secret-dependent memory access -- full-table scan with masked selection for lookups
- No variable-time instructions in hot paths
- `helioselene_secure_erase()` on stack locals in constant-time scalar multiplication paths
- Verification-only paths may use variable-time operations (explicitly tagged `_vartime` in the API)

### Twist Security

Helios has ~107 bits of twist security; Selene has ~99 bits. **Every externally-received point must pass on-curve validation** via the `frombytes` functions, which return an error for off-curve points.

## Benchmarking

The benchmark tool (`helioselene-benchmark`) measures all library operations:

- `--init` -- Use CPUID heuristic dispatch (default: x64 baseline)
- `--autotune` -- Use benchmarked best-per-function dispatch

Operations benchmarked include field arithmetic (add, sub, mul, sq, invert, sqrt), point operations (dbl, madd, add), serialization, scalar multiplication (CT and vartime), MSM at multiple sizes (n = 1, 8, 32, 64, 256), SSWU hash-to-curve, batch affine conversion, Pedersen commitments, polynomial multiplication (Karatsuba and ECFFT), and divisor computation.

## Testing

Unit tests use CTest with no external test framework (no Google Test, no Catch2 -- zero dependencies).

```bash
cmake -S . -B build -DBUILD_TESTS=ON
cmake --build build --config Release -j
./build/helioselene-tests
```

300 tests across 42+ test groups covering: F_p/F_q arithmetic, square roots, point operations, scalar multiplication, MSM, SSWU hash-to-curve, batch affine, Pedersen commitments, polynomials (schoolbook, Karatsuba, interpolation, ECFFT), divisors, serialization, edge cases, Wei25519 bridge, and dispatch verification.

### Full Test Matrix

All four build configurations should be tested on both MSVC and MinGW (8 total builds):

| Config | CMake flags | Tests |
|--------|-------------|-------|
| FORCE_PORTABLE | `-DFORCE_PORTABLE=1` | 294 |
| x64 no SIMD | `-DENABLE_AVX2=OFF -DENABLE_AVX512=OFF` | 294 |
| x64 + AVX2 | `-DENABLE_AVX512=OFF` | 300 |
| x64 + AVX2 + IFMA | (default) | 300 |

SIMD configurations include 6 additional dispatch verification tests that confirm each runtime-selected backend produces correct known-answer test (KAT) outputs.

## License

This project is licensed under the [BSD 3-Clause License](LICENSE).
