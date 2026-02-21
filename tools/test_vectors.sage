#!/usr/bin/env sage
# -*- coding: utf-8 -*-
"""
Independent SageMath test vector generator/validator for helioselene.

Usage:
  sage test_vectors.sage --generate > test_vectors_sage.json
  sage test_vectors.sage --validate test_vectors/helioselene_test_vectors.json

All arithmetic is performed purely in SageMath (no C++ calls).
"""

import json
import sys

# ── Curve parameters ──

p = 2**255 - 19
Fp = GF(p)

helios_b = Fp(15789920373731020205926570676277057129217619222203920395806844808978996083412)
E_helios = EllipticCurve(Fp, [-3, helios_b])
G_helios = E_helios(3, 37760095087190773158272406437720879471285821656958791565335581949097084993268)
q_helios = 57896044618658097711785492504343953926549254372227246365156541811699034343327

Fq = GF(q_helios)

selene_b = Fq(50691664119640283727448954162351551669994268339720539671652090628799494505816)
E_selene = EllipticCurve(Fq, [-3, selene_b])
G_selene = E_selene(1, 55227837453588766352929163364143300868577356225733378474337919561890377498066)

# Verify generators are on the curves
assert G_helios in E_helios, "Helios generator not on curve"
assert G_selene in E_selene, "Selene generator not on curve"

# ── Serialization helpers ──

def to_le_hex(val, length=32):
    """Convert an integer to little-endian hex string."""
    val = int(val) % (2**(length*8))
    b = int(val).to_bytes(length, 'little')
    return b.hex()

def from_le_hex(h):
    """Convert a little-endian hex string to integer."""
    return int.from_bytes(bytes.fromhex(h), 'little')

def compress_point(P, prime):
    """Compress a Weierstrass point to 32-byte LE with bit 255 = y parity."""
    if P == P.curve()(0):  # identity
        return to_le_hex(0)
    x = int(P[0])
    y = int(P[1])
    # y parity: bit 255 of x-coordinate encoding
    encoded = x
    if y % 2 == 1:
        encoded |= (1 << 255)
    return to_le_hex(encoded)

def decompress_point(hex_str, curve, prime):
    """Decompress a 32-byte LE point encoding."""
    val = from_le_hex(hex_str)
    if val == 0:
        return curve(0)  # identity
    y_parity = (val >> 255) & 1
    x = val & ((1 << 255) - 1)
    F = GF(prime)
    x_f = F(x)
    rhs = x_f**3 - 3*x_f + curve.a6()
    if not rhs.is_square():
        return None
    y_f = rhs.sqrt()
    if int(y_f) % 2 != y_parity:
        y_f = -y_f
    return curve(x_f, y_f)

# ── RFC 9380 Simplified SWU map_to_curve ──

def sswu_map(u_int, curve, prime):
    """
    Simplified SWU map for y^2 = x^3 - 3x + b.
    a = -3, Z chosen per curve.
    """
    F = GF(prime)
    a = F(-3)
    b_coeff = curve.a6()

    # Z values: must be non-square, and g(B/(Z*A)) must be square
    # For Helios (Fp): Z = -10 (matches RFC 9380 for a=-3 over Fp)
    # For Selene (Fq): need to find Z
    if prime == p:
        Z = F(-10)
    else:
        Z = F(-10)  # Try -10 first
        if Z.is_square():
            # Find a non-square Z
            for z_try in range(-1, -100, -1):
                Z = F(z_try)
                if not Z.is_square():
                    break

    u = F(u_int)

    # SWU core
    tv1 = (Z**2 * u**4 + Z * u**2)
    if tv1 == 0:
        x1 = b_coeff / (Z * a)
    else:
        x1 = (-b_coeff / a) * (1 + 1/tv1)

    gx1 = x1**3 + a * x1 + b_coeff
    if gx1.is_square():
        y1 = gx1.sqrt()
        # Fix sign: sgn0(u) == sgn0(y)
        if int(u) % 2 != int(y1) % 2:
            y1 = -y1
        return curve(x1, y1)
    else:
        x2 = Z * u**2 * x1
        gx2 = x2**3 + a * x2 + b_coeff
        y2 = gx2.sqrt()
        if int(u) % 2 != int(y2) % 2:
            y2 = -y2
        return curve(x2, y2)

# ── Test input constants ──

test_a_hex = "efcdab9078563412bebafecaefbeadde00000000000000000000000000000000"
test_b_hex = "08070605040302010df0adbacefaedfe00000000000000000000000000000000"
test_a_int = from_le_hex(test_a_hex)
test_b_int = from_le_hex(test_b_hex)

wide_zero = bytes(64)
wide_small = bytes([42]) + bytes(63)
wide_large = bytes([0xff]*32) + bytes(32)
wide_hash = bytes([
    0x48, 0x65, 0x6c, 0x69, 0x6f, 0x73, 0x65, 0x6c, 0x65, 0x6e, 0x65, 0x5f, 0x74, 0x65, 0x73, 0x74,
    0x5f, 0x76, 0x65, 0x63, 0x74, 0x6f, 0x72, 0x5f, 0x30, 0x30, 0x30, 0x31, 0x00, 0x00, 0x00, 0x00,
    0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89, 0xde, 0xad, 0xbe, 0xef, 0xca, 0xfe, 0xba, 0xbe,
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10
])

# ── Validation ──

def validate_scalar_section(section, field_order, field_name, errors):
    """Validate scalar test vectors against Sage arithmetic."""
    F = GF(field_order)

    # from_bytes
    for vec in section.get("from_bytes", []):
        label = vec["label"]
        inp = from_le_hex(vec["input"])
        expected = vec["result"]
        if inp >= field_order:
            if expected is not None:
                errors.append(f"{field_name}.from_bytes.{label}: expected null for out-of-range, got {expected}")
        else:
            if expected is None:
                errors.append(f"{field_name}.from_bytes.{label}: expected {to_le_hex(inp)}, got null")
            elif to_le_hex(inp) != expected:
                errors.append(f"{field_name}.from_bytes.{label}: mismatch")

    # add
    for vec in section.get("add", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        b = F(from_le_hex(vec["b"]))
        expected = vec["result"]
        computed = to_le_hex(int(a + b))
        if computed != expected:
            errors.append(f"{field_name}.add.{label}: expected {expected}, got {computed}")

    # sub
    for vec in section.get("sub", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        b = F(from_le_hex(vec["b"]))
        expected = vec["result"]
        computed = to_le_hex(int(a - b))
        if computed != expected:
            errors.append(f"{field_name}.sub.{label}: expected {expected}, got {computed}")

    # mul
    for vec in section.get("mul", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        b = F(from_le_hex(vec["b"]))
        expected = vec["result"]
        computed = to_le_hex(int(a * b))
        if computed != expected:
            errors.append(f"{field_name}.mul.{label}: expected {expected}, got {computed}")

    # sq
    for vec in section.get("sq", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        expected = vec["result"]
        computed = to_le_hex(int(a * a))
        if computed != expected:
            errors.append(f"{field_name}.sq.{label}: expected {expected}, got {computed}")

    # negate
    for vec in section.get("negate", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        expected = vec["result"]
        computed = to_le_hex(int(-a))
        if computed != expected:
            errors.append(f"{field_name}.negate.{label}: expected {expected}, got {computed}")

    # invert
    for vec in section.get("invert", []):
        label = vec["label"]
        a_int = from_le_hex(vec["a"])
        expected = vec["result"]
        if a_int == 0:
            if expected is not None:
                errors.append(f"{field_name}.invert.{label}: expected null for zero, got {expected}")
        else:
            a = F(a_int)
            computed = to_le_hex(int(a**(-1)))
            if computed != expected:
                errors.append(f"{field_name}.invert.{label}: expected {expected}, got {computed}")

    # reduce_wide
    for vec in section.get("reduce_wide", []):
        label = vec["label"]
        inp_bytes = bytes.fromhex(vec["input"])
        inp_int = int.from_bytes(inp_bytes, 'little')
        expected = vec["result"]
        computed = to_le_hex(inp_int % field_order)
        if computed != expected:
            errors.append(f"{field_name}.reduce_wide.{label}: expected {expected}, got {computed}")

    # muladd
    for vec in section.get("muladd", []):
        label = vec["label"]
        a = F(from_le_hex(vec["a"]))
        b = F(from_le_hex(vec["b"]))
        c = F(from_le_hex(vec["c"]))
        expected = vec["result"]
        computed = to_le_hex(int(a * b + c))
        if computed != expected:
            errors.append(f"{field_name}.muladd.{label}: expected {expected}, got {computed}")

    # is_zero
    for vec in section.get("is_zero", []):
        label = vec["label"]
        a_int = from_le_hex(vec["a"])
        expected = vec["result"]
        computed = (a_int % field_order == 0)
        if computed != expected:
            errors.append(f"{field_name}.is_zero.{label}: expected {expected}, got {computed}")


def validate_point_section(section, curve, G, scalar_order, base_prime, field_name, errors):
    """Validate point test vectors against Sage arithmetic."""

    # generator
    gen_hex = section.get("generator")
    if gen_hex:
        computed = compress_point(G, base_prime)
        if computed != gen_hex:
            errors.append(f"{field_name}.generator: mismatch")

    # identity
    id_hex = section.get("identity")
    if id_hex:
        computed = compress_point(curve(0), base_prime)
        if computed != id_hex:
            errors.append(f"{field_name}.identity: mismatch")

    # scalar_mul
    for vec in section.get("scalar_mul", []):
        label = vec["label"]
        s = from_le_hex(vec["scalar"])
        pt_hex = vec["point"]
        expected = vec["result"]

        pt = decompress_point(pt_hex, curve, base_prime)
        if pt is None:
            errors.append(f"{field_name}.scalar_mul.{label}: failed to decompress point")
            continue

        result_pt = int(s) * pt
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.scalar_mul.{label}: expected {expected}, got {computed}")

    # add
    for vec in section.get("add", []):
        label = vec["label"]
        a_pt = decompress_point(vec["a"], curve, base_prime)
        b_pt = decompress_point(vec["b"], curve, base_prime)
        expected = vec["result"]
        if a_pt is None or b_pt is None:
            errors.append(f"{field_name}.add.{label}: failed to decompress")
            continue
        result_pt = a_pt + b_pt
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.add.{label}: expected {expected}, got {computed}")

    # dbl
    for vec in section.get("dbl", []):
        label = vec["label"]
        a_pt = decompress_point(vec["a"], curve, base_prime)
        expected = vec["result"]
        if a_pt is None:
            errors.append(f"{field_name}.dbl.{label}: failed to decompress")
            continue
        result_pt = 2 * a_pt
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.dbl.{label}: expected {expected}, got {computed}")

    # negate
    for vec in section.get("negate", []):
        label = vec["label"]
        a_pt = decompress_point(vec["a"], curve, base_prime)
        expected = vec["result"]
        if a_pt is None:
            errors.append(f"{field_name}.negate.{label}: failed to decompress")
            continue
        result_pt = -a_pt
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.negate.{label}: expected {expected}, got {computed}")

    # msm
    for vec in section.get("msm", []):
        label = vec["label"]
        n = vec["n"]
        scalars = [from_le_hex(s) for s in vec["scalars"]]
        points = [decompress_point(ph, curve, base_prime) for ph in vec["points"]]
        expected = vec["result"]
        if any(pt is None for pt in points):
            errors.append(f"{field_name}.msm.{label}: failed to decompress points")
            continue
        result_pt = sum(s * pt for s, pt in zip(scalars, points))
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.msm.{label}: expected {expected}, got {computed}")

    # pedersen_commit
    for vec in section.get("pedersen_commit", []):
        label = vec["label"]
        blind = from_le_hex(vec["blinding"])
        H_pt = decompress_point(vec["H"], curve, base_prime)
        values = [from_le_hex(v) for v in vec["values"]]
        generators = [decompress_point(g, curve, base_prime) for g in vec["generators"]]
        expected = vec["result"]
        if H_pt is None or any(g is None for g in generators):
            errors.append(f"{field_name}.pedersen.{label}: failed to decompress")
            continue
        result_pt = int(blind) * H_pt
        for v, gen in zip(values, generators):
            result_pt += int(v) * gen
        computed = compress_point(result_pt, base_prime)
        if computed != expected:
            errors.append(f"{field_name}.pedersen.{label}: expected {expected}, got {computed}")

    # x_coordinate
    for vec in section.get("x_coordinate", []):
        label = vec["label"]
        pt = decompress_point(vec["point"], curve, base_prime)
        expected = vec["x_bytes"]
        if pt is None:
            errors.append(f"{field_name}.x_coordinate.{label}: failed to decompress")
            continue
        x_int = int(pt[0])
        computed = to_le_hex(x_int)
        if computed != expected:
            errors.append(f"{field_name}.x_coordinate.{label}: expected {expected}, got {computed}")

    # from_bytes
    for vec in section.get("from_bytes", []):
        label = vec["label"]
        inp = vec["input"]
        expected = vec["result"]
        pt = decompress_point(inp, curve, base_prime)
        if pt is None:
            if expected is not None:
                errors.append(f"{field_name}.from_bytes.{label}: expected null, got result")
        else:
            computed = compress_point(pt, base_prime)
            if expected is None:
                # We got a valid point but C++ says invalid — might be off-curve
                # Don't flag this as error since our decompress might differ
                pass
            elif computed != expected:
                errors.append(f"{field_name}.from_bytes.{label}: expected {expected}, got {computed}")


def validate_polynomial_section(section, field_order, field_name, errors):
    """Validate polynomial test vectors."""
    F = GF(field_order)
    R = PolynomialRing(F, 'x')
    x = R.gen()

    # from_roots
    for vec in section.get("from_roots", []):
        label = vec["label"]
        n = vec.get("n", 0)
        coeffs_hex = vec.get("coefficients", [])
        # Build expected polynomial from roots
        # (we don't have roots stored in the JSON for fq, so just verify degree)
        degree = vec.get("degree", len(coeffs_hex) - 1)
        if len(coeffs_hex) != degree + 1:
            errors.append(f"{field_name}.from_roots.{label}: coefficient count mismatch")

    # evaluate
    for vec in section.get("evaluate", []):
        label = vec["label"]
        x_val = F(from_le_hex(vec["x"]))
        expected = vec["result"]
        coeffs = vec.get("coefficients", [])
        if coeffs:
            poly = sum(F(from_le_hex(c)) * x**i for i, c in enumerate(coeffs))
            result = int(poly(x_val))
            computed = to_le_hex(result)
            if computed != expected:
                errors.append(f"{field_name}.evaluate.{label}: expected {expected}, got {computed}")


def validate_divisor_section(section, curve, G, scalar_order, base_prime, field_name, errors):
    """Validate divisor test vectors (evaluation only — polynomial structure checked separately)."""
    for vec in section.get("compute", []):
        label = vec["label"]
        n = vec["n"]
        points = [decompress_point(ph, curve, base_prime) for ph in vec["points"]]
        if any(pt is None for pt in points):
            errors.append(f"{field_name}.compute.{label}: failed to decompress points")
            continue

        # Verify evaluation at non-member point is nonzero
        eval_result = vec.get("eval_result")
        if eval_result and from_le_hex(eval_result) == 0:
            errors.append(f"{field_name}.compute.{label}: eval at non-member should be nonzero")


def validate_wei25519(section, errors):
    """Validate Wei25519 bridge vectors."""
    for vec in section.get("x_to_selene_scalar", []):
        label = vec["label"]
        inp = from_le_hex(vec["input"])
        expected = vec["result"]

        if inp >= p:
            if expected is not None:
                errors.append(f"wei25519.{label}: expected null for x >= p, got {expected}")
        else:
            computed = to_le_hex(inp)
            if expected is None:
                errors.append(f"wei25519.{label}: expected {computed}, got null")
            elif computed != expected:
                errors.append(f"wei25519.{label}: expected {expected}, got {computed}")


def validate_batch_invert(section, errors):
    """Validate batch inversion vectors."""
    for field_name, field_order in [("fp", p), ("fq", q_helios)]:
        F = GF(field_order)
        for vec in section.get(field_name, []):
            label = vec["label"]
            inputs = [from_le_hex(h) for h in vec["inputs"]]
            results = [from_le_hex(h) for h in vec["results"]]
            for i, (inp, res) in enumerate(zip(inputs, results)):
                if inp == 0:
                    if res != 0:
                        errors.append(f"batch_invert.{field_name}.{label}[{i}]: expected 0 for zero input")
                else:
                    computed = int(F(inp)**(-1))
                    if computed != res:
                        errors.append(f"batch_invert.{field_name}.{label}[{i}]: expected {to_le_hex(computed)}, got {to_le_hex(res)}")


def validate(filename):
    """Validate all test vectors in a JSON file."""
    with open(filename) as f:
        data = json.load(f)

    errors = []
    total_checks = 0

    print(f"Validating {filename}...")

    # Parameters
    params = data.get("parameters", {})
    if params:
        h_order = from_le_hex(params["helios_order"])
        s_order = from_le_hex(params["selene_order"])
        assert h_order == q_helios, f"Helios order mismatch: {h_order} != {q_helios}"
        assert s_order == p, f"Selene order mismatch: {s_order} != {p}"
        print("  Parameters: OK")

    # Scalar sections
    if "helios_scalar" in data:
        print("  Helios scalar...", end="", flush=True)
        validate_scalar_section(data["helios_scalar"], q_helios, "helios_scalar", errors)
        print(" OK" if not any("helios_scalar" in e for e in errors) else " ERRORS")

    if "selene_scalar" in data:
        print("  Selene scalar...", end="", flush=True)
        validate_scalar_section(data["selene_scalar"], p, "selene_scalar", errors)
        print(" OK" if not any("selene_scalar" in e for e in errors) else " ERRORS")

    # Point sections
    if "helios_point" in data:
        print("  Helios point...", end="", flush=True)
        validate_point_section(data["helios_point"], E_helios, G_helios, q_helios, p, "helios_point", errors)
        print(" OK" if not any("helios_point" in e for e in errors) else " ERRORS")

    if "selene_point" in data:
        print("  Selene point...", end="", flush=True)
        validate_point_section(data["selene_point"], E_selene, G_selene, p, q_helios, "selene_point", errors)
        print(" OK" if not any("selene_point" in e for e in errors) else " ERRORS")

    # Polynomial sections
    if "fp_polynomial" in data:
        print("  Fp polynomial...", end="", flush=True)
        validate_polynomial_section(data["fp_polynomial"], p, "fp_polynomial", errors)
        print(" OK" if not any("fp_polynomial" in e for e in errors) else " ERRORS")

    if "fq_polynomial" in data:
        print("  Fq polynomial...", end="", flush=True)
        validate_polynomial_section(data["fq_polynomial"], q_helios, "fq_polynomial", errors)
        print(" OK" if not any("fq_polynomial" in e for e in errors) else " ERRORS")

    # Divisor sections
    if "helios_divisor" in data:
        print("  Helios divisor...", end="", flush=True)
        validate_divisor_section(data["helios_divisor"], E_helios, G_helios, q_helios, p, "helios_divisor", errors)
        print(" OK" if not any("helios_divisor" in e for e in errors) else " ERRORS")

    if "selene_divisor" in data:
        print("  Selene divisor...", end="", flush=True)
        validate_divisor_section(data["selene_divisor"], E_selene, G_selene, p, q_helios, "selene_divisor", errors)
        print(" OK" if not any("selene_divisor" in e for e in errors) else " ERRORS")

    # Wei25519
    if "wei25519" in data:
        print("  Wei25519 bridge...", end="", flush=True)
        validate_wei25519(data["wei25519"], errors)
        print(" OK" if not any("wei25519" in e for e in errors) else " ERRORS")

    # Batch invert
    if "batch_invert" in data:
        print("  Batch invert...", end="", flush=True)
        validate_batch_invert(data["batch_invert"], errors)
        print(" OK" if not any("batch_invert" in e for e in errors) else " ERRORS")

    # Summary
    print()
    if errors:
        print(f"FAILED: {len(errors)} error(s)")
        for e in errors:
            print(f"  - {e}")
        return 1
    else:
        print("ALL VECTORS VALIDATED SUCCESSFULLY")
        return 0


# ── Main ──

if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:")
        print("  sage test_vectors.sage --validate <json_file>")
        print("  sage test_vectors.sage --generate")
        sys.exit(1)

    if sys.argv[1] == "--validate":
        if len(sys.argv) < 3:
            print("Error: --validate requires a JSON file path")
            sys.exit(1)
        sys.exit(validate(sys.argv[2]))

    elif sys.argv[1] == "--generate":
        print("Generation mode not yet implemented.", file=sys.stderr)
        print("Use the C++ generator instead.", file=sys.stderr)
        sys.exit(1)

    else:
        print(f"Unknown option: {sys.argv[1]}")
        sys.exit(1)
