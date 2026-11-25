# SH4ZAM trigonometry: accuracy probe (for upstream)

Written 2026-11-25.

**Scope.** This is SH4ZAM's software back end (`source/sw`) on the host only. Nothing here was checked on a console. The SH-4 back end may give other numbers.

## Why it came up

`psm_sun_at` (`demo/psm/psm_town.h`) gives the town's sun from the clock. `test/psm_town_test.c` checks it against the story's table: azimuth within 0.6 degrees, elevation within 0.1 degrees.

With `shz_atan2f` and `shz_asinf`, `make test-shz` failed that check: elevation was out by 0.33 degrees. The 14:00 readout said `SUN 214/26` where the table gives 26.5 (so 27).

With `shz_dce_atan2f` (SH4ZAM-DCE), and asin written as `atan2(x, sqrt(1 - x^2))`, the check passes on both paths. The worst errors are then 0.46 degrees in azimuth and 0.05 in elevation, the same as libm.

## Method

- **Program.** One C file, about 20 lines. It compares each call with the libm double function at 2,001 points, i = -1000 .. 1000, x = i / 1000. The source is reproduced below.
- **Build.** `cc -O2 -std=gnu11 -Ithird_party/sh4zam/include -Ithird_party/sh4zam-dce/include probe.c third_party/sh4zam/source/sw/shz_xmtrx_sw.c -lm`. Run in MSYS2 mingw64, with SH4ZAM v0.10.0 checked out in `third_party/sh4zam`.

Inputs per call:

| Call | Input |
|---|---|
| `shz_asinf` | `x * 0.999` |
| `shz_tanf` | `x * 1.2` |
| `shz_sincosf` (sin), `shz_sinf` | `3 x` |
| `shz_atan2f`, `shz_dce_atan2f` | `(x, 0.37)` |

## Result (worst absolute error)

| Call | Worst error |
|---|---|
| `shz_atan2f(y, x)` | **0.5815 degrees** |
| `shz_asinf(x)` | **0.2837 degrees** |
| `shz_dce_atan2f(y, x)` | 0.0007 degrees |
| `shz_sincosf(a).sin` | 0.00000 degrees (on this back end) |
| `shz_sinf(a)` | 0.00000 degrees (on this back end) |
| `shz_tanf(a)` | 0.000001 (absolute value) |

## Reading

`shz_atan2f` and `shz_asinf` look like fast approximations. That fits bearings for animation, but not values a test or a table checks to a tenth of a degree.

What is not known:

- whether the SH-4 back end uses the same approximation;
- what FSCA's own error is for sin and cos on the console.

A console run of the same loop, printing the same table, settles both.

## The probe

```c
#include <stdio.h>
#include <math.h>
#include "sh4zam/shz_sh4zam.h"
#include "sh4zam_dce/shz_dce_atan2.h"

int main(void) {
    double wa = 0, wt = 0, ws = 0, wc = 0, wd = 0, wi = 0;
    int i;

    for (i = -1000; i <= 1000; i++) {
        const float x = (float)i / 1000.0f, a = x * 3.0f;
        const shz_sincos_t sc = shz_sincosf(a);
        double e;

        e = fabs(shz_asinf(x * 0.999f) - asin(x * 0.999)); if (e > wa) wa = e;
        e = fabs(shz_tanf(x * 1.2f) - tan(x * 1.2)); if (e > wt) wt = e;
        e = fabs(sc.sin - sin(a)); if (e > ws) ws = e;
        e = fabs(shz_atan2f(x, 0.37f) - atan2(x, 0.37)); if (e > wc) wc = e;
        e = fabs(shz_dce_atan2f(x, 0.37f) - atan2(x, 0.37)); if (e > wd) wd = e;
        e = fabs(shz_sinf(a) - sin(a)); if (e > wi) wi = e;
    }
    printf("max error, degrees: asin %.4f tan(abs) %.6f sincos.sin %.5f atan2 %.4f dce_atan2 %.5f sinf %.5f\n",
           wa * 57.29578, wt, ws * 57.29578, wc * 57.29578, wd * 57.29578, wi * 57.29578);
    return 0;
}
```
