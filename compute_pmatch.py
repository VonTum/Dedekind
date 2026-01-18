#!/usr/bin/env python3
import math

def compute_p_match(N,
    Dnp1, exp_np1, SE_np1,
    Dn,   exp_n,   SE_n
):
    """
    Inputs:
      Dnp1      = mantissa of D(N+1)
      exp_np1  = exponent of D(N+1), so real value is Dnp1 * 10^exp_np1
      SE_np1   = standard error of D(N+1) (same scale as Dnp1)

      Dn       = mantissa of D(N)
      exp_n   = exponent of D(N), so real value is Dn * 10^exp_n
      SE_n    = standard error of D(N) (same scale as Dn)

    Returns:
      p_match, SE_p_match   (both as ordinary Python floats)
    """

    # Work entirely in mantissa+exponent form until the end
    #
    # p = D(N+1) / D(N)^2
    #
    # = (Dnp1 * 10^exp_np1) / (Dn^2 * 10^(2*exp_n))
    #
    # = (Dnp1 / Dn^2) * 10^(exp_np1 - 2*exp_n)

    mantissa = Dnp1 / (Dn * Dn)
    exponent = exp_np1 - 2 * exp_n

    # Convert to double now that exponent is small enough
    p_match = mantissa * (10.0 ** exponent)

    # Relative error propagation:
    # (SE_p / p)^2 = (SE_{N+1}/D(N+1))^2 + (2 * SE_N / D(N))^2
    rel_err_sq = (SE_np1 / Dnp1)**2 + (2.0 * SE_n / Dn)**2
    SE_p_match = abs(p_match) * math.sqrt(rel_err_sq)

    print("$p_{match,", N, "}$ & ", p_match, " & ", SE_p_match, "\\\\")
    # return p_match, SE_p_match

# Exact region (no SE, exponent = 0 everywhere)

compute_p_match(0,
    3.0, 0, 0.0,
    2.0, 0, 0.0
)

compute_p_match(1,
    6.0, 0, 0.0,
    3.0, 0, 0.0
)

compute_p_match(2,
    20.0, 0, 0.0,
    6.0, 0, 0.0
)

compute_p_match(3,
    168.0, 0, 0.0,
    20.0, 0, 0.0
)

compute_p_match(4,
    7581.0, 0, 0.0,
    168.0, 0, 0.0
)

compute_p_match(5,
    7828354.0, 0, 0.0,
    7581.0, 0, 0.0
)

compute_p_match(6,
    2.414682040998, 12, 0.0,
    7828354.0,        0, 0.0
)

compute_p_match(7,
    5.613043722868756, 22, 0.0,
    2.414682040998,    12, 0.0
)

compute_p_match(8,
    2.863865776682984, 41, 0.0,
    5.613043722868756, 22, 0.0
)


# Mixed exact / estimated

compute_p_match(9,
    8.93345, 78, 2.44e-4,
    2.863865776682984, 41, 0.0
)


# Fully estimated region

compute_p_match(10,
    3.63437, 144, 1.67e-3,
    8.93345, 78, 2.44e-4
)

compute_p_match(11,
    7.14919, 283, 5.35e-3,
    3.63437, 144, 1.67e-3
)

compute_p_match(12,
    6.03589, 525, 1.33e-2,
    7.14919, 283, 5.35e-3
)

compute_p_match(13,
    5.58483, 1043, 4.06e-2,
    6.03589, 525, 1.33e-2
)

compute_p_match(14,
    3.80603, 1953, 5.30e-2,
    5.58483, 1043, 4.06e-2
)
