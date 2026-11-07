/* CPU-Z 2.15 benchmark "17.01.64" pixel kernel, reconstructed from cpuz.exe
 * (sha256 d68d80fa...). Each function names the VA it reproduces. Operation
 * order follows the disassembly so results are bit-exact under SSE scalar math
 * with -ffp-contract=off. */
#include "kernel.h"
#include <immintrin.h>
#include <stdlib.h>
#include <string.h>

static inline int cvtt(float f) { return _mm_cvtt_ss2si(_mm_set_ss(f)); }
static inline long long cvtt64(float f) { return _mm_cvttss_si64(_mm_set_ss(f)); }
static inline double u2d(unsigned long long u) { double d; memcpy(&d, &u, 8); return d; }
static inline unsigned long long d2u(double d) { unsigned long long u; memcpy(&u, &d, 8); return u; }

/* 0x1402db000: the CRT sinf linked into cpuz.exe (double-precision
 * Cody-Waite reduction and Taylor polynomials). Only |x| < 500000 is ported;
 * the benchmark calls it with 0 <= x < 101. */
static float crt_sinf(float xf) {
    const double S1 = -0.16666666666666666, S2 = 0.008333333333333333,
                 S3 = -0.0001984126984126984, S4 = 2.7557319223985893e-06;
    const double C1 = -0.5, C2 = 0.041666666666666664, C3 = -0.0013888888888888887,
                 C4 = 2.4801587301587298e-05, C5 = -2.755731922398589e-07;
    const double SIXTH = 0.16666666666666666;
    double x = xf;
    unsigned long long ux = d2u(x), ax = ux & 0x7fffffffffffffffULL;
    if (ax <= 0x3fe921fb54442d18ULL) {             /* |x| <= pi/4 */
        if (ax < 0x3f20000000000000ULL) return (float)x;
        if (ax < 0x3f80000000000000ULL) { double t = x * x; t = t * x; t = t * SIXTH; return (float)(x - t); }
        double x2 = x * x, p = S4 * x2, x4 = x2 * x2, q = S2 * x2, x3 = x * x2;
        p = p + S3; p = p * x4; q = q + S1; p = p + q; p = p * x3;
        return (float)(x + p);
    }
    int neg = ux != ax;
    if (neg) x = 0.0 - x;
    double npd = x * 0.6366197723675814; npd = npd + 0.5;
    int npi2 = _mm_cvttsd_si32(_mm_set_sd(npd));
    double xn = npi2;
    double rhead = x - 1.5707963267341256 * xn;
    double rtail = 6.077100506506192e-11 * xn;
    double r = rhead - rtail;
    long long expdiff = (long long)(ax >> 52) - (long long)((d2u(r) << 1) >> 53);
    unsigned long long rexp = (d2u(r) << 1) >> 53;
    if (expdiff > 15) {
        double t = rhead;
        rtail = 6.077100506303966e-11 * xn;
        rhead = t - rtail;
        rtail = 2.0222662487959506e-21 * xn - ((t - rhead) - rtail);
        r = rhead - rtail;
    }
    double res;
    if (rexp >= 0x3f2) {
        double x2 = r * r;
        if (!(npi2 & 1)) {
            double p = S4 * x2, x4 = x2 * x2, q = S2 * x2, x3 = r * x2;
            p = p + S3; p = p * x4; q = q + S1; p = p + q; p = p * x3;
            res = r + p;
        } else {
            double p = C5 * x2, x4 = x2 * x2, q = C3 * x2, h = C1 * x2;
            p = p + C4; p = p * x4; q = q + C2; p = p + q; p = p * x4;
            h = h + 1.0; res = h + p;
        }
    } else if (rexp > 0x3de) {
        double x2 = r * r;
        if (!(npi2 & 1)) { double t = SIXTH * r; t = t * x2; res = r - t; }
        else res = 1.0 - x2 * 0.5;
    } else {
        res = (npi2 & 1) ? 1.0 : r;
    }
    unsigned q = (unsigned)npi2 >> 1;
    if (((q & neg) | (~q & ~neg)) & 1) return (float)res;
    return (float)(0.0 - res);
}

/* 0x14004a8f0: allocate and fill the 4096-entry table at [this+0x30]. */
void cz_init(cz_ctx *c) {
    c->sin_lut = malloc(4097 * sizeof(float));
    for (int i = 0; i < 4096; i++) {
        float x = (float)i;
        x = x * 6.2831850f; x = x * 0.0009765625f; x = x * 4.0f;
        c->sin_lut[i] = crt_sinf(x);
    }
    c->sin_lut[4096] = c->sin_lut[0];  /* never read: verify counts index 4096 */
}

/* MSVC evaluates the inlined fract twice per component (two table loads, two
 * s*K products); GCC would merge them. OPAQUE keeps both copies so the port does
 * the same work as the original. It changes no value. */
#define OPAQUE(v) __asm__ volatile("" : "+x"(v))

/* One output: fract (negatives wrapped), two table lookups, final fract. */
static inline __attribute__((always_inline)) float hash_component(const cz_ctx *c, float v) {
    const float K = 43758.546875f;
    v = v - (float)cvtt(v);
    float f1 = v, f2 = v;
    OPAQUE(f2);
    if (0.0f > f1) f1 = f1 + 1.0f;
    if (0.0f > f2) f2 = f2 + 1.0f;
    float s1 = c->sin_lut[cvtt(f1 * 1024.0f * 4.0f)] * K;
    float s2 = c->sin_lut[cvtt(f2 * 1024.0f * 4.0f)] * K;
    return (s1 - (float)cvtt(s2)) * 2.0f - 1.0f;
}

/* 0x14004a9c0: hash22. idx = fract(dot/2pi)*4096, out = 2*(s*K - trunc(s*K)) - 1.
 * The first fract wraps negatives to [0,1); the final one does not. */
static void hash22(const cz_ctx *c, const float p[2], float out[2]) {
    float x = p[0], y = p[1];
    float b = y * 183.3f; b = b + x * 269.5f;
    float a = y * 311.7f; a = a + x * 127.1f;
    a = a * 0.15915495f;
    b = b * 0.15915495f;
    out[0] = hash_component(c, a);
    out[1] = hash_component(c, b);
}


static inline int ffloor(float v) { int i = cvtt(v); return i - ((float)i > v); }

/* 0x14004ab60: 2D simplex noise with r^4 falloff (0.5 - r^2), no output scale. */
static float noise(const cz_ctx *c, float x, float y) {
    const float F2 = 0.36602542f, G2 = 0.21132487f, G2x2m1 = 0.42264974f;
    float s = (x + y) * F2;
    float fi = (float)ffloor(s + x), fj = (float)ffloor(s + y);
    float t = (fj + fi) * G2;
    float x0 = (x - fi) + t, y0 = (y - fj) + t;
    float i1 = x0 > y0 ? 1.0f : 0.0f, j1 = x0 > y0 ? 0.0f : 1.0f;
    float x1 = (x0 - i1) + G2, y1 = (y0 - j1) + G2;
    float x2 = (x0 - 1.0f) + G2x2m1, y2 = (y0 - 1.0f) + G2x2m1;
    float t0 = 0.5f - (y0 * y0 + x0 * x0); if (0.0f > t0) t0 = 0.0f;
    float t1 = 0.5f - (y1 * y1 + x1 * x1); if (0.0f > t1) t1 = 0.0f;
    float t2 = 0.5f - (y2 * y2 + x2 * x2); if (0.0f > t2) t2 = 0.0f;
    float p[2], h[2];
    p[0] = fi; p[1] = fj; hash22(c, p, h);
    float n0 = h[1] * y0 + h[0] * x0;
    p[0] = i1 + fi; p[1] = j1 + fj; hash22(c, p, h);
    float n1 = h[1] * y1 + h[0] * x1;
    p[0] = fi + 1.0f; p[1] = fj + 1.0f; hash22(c, p, h);
    float n2 = h[1] * y2 + h[0] * x2;
    t0 = t0 * t0 * t0 * t0; t1 = t1 * t1 * t1 * t1; t2 = t2 * t2 * t2 * t2;
    return (t1 * n1 + t0 * n0) + n2 * t2;
}

/* Octave step shared by both fbm functions: p' = (1.6px - 1.2py, 1.2px + 1.6py). */
#define ROTATE(px, py) do { float nx = px * 1.6f - py * 1.2f; py = px * 1.2f + py * 1.6f; px = nx; } while (0)

/* 0x14004aeb0: 10 octaves of |noise| (amp 0.8, gain 0.7), then 10 octaves of
 * signed noise from the same start (amp 0.7, gain 0.6); returns
 * (s2 + s1) * s2 * 8 * s1 + 0.2. */
float cz_fbm_a(const cz_ctx *c, int x, int y) {
    float px = (float)x * 1.1f * 0.0009765625f, py = (float)y * 1.1f * 0.0009765625f;
    float sx = px, sy = py, s1 = 0.0f, s2 = 0.0f, amp = 0.8f, amp2 = 0.7f;
    for (int k = 0; k < 10; k++) {
        float n = noise(c, sx, sy);
        float a = amp * 40.0f; amp = amp * 0.7f;
        s1 = s1 + __builtin_fabsf(n * a);
        ROTATE(sx, sy);
    }
    for (int k = 0; k < 10; k++) {
        float n = noise(c, px, py);
        float a = amp2 * 40.0f; amp2 = amp2 * 0.6f;
        s2 = s2 + n * a;
        ROTATE(px, py);
    }
    return (s2 + s1) * s2 * 8.0f * s1 + 0.2f;
}

/* 0x14004b0b0: 7 octaves of signed noise at freq 2.2, then 7 octaves of
 * |noise| at freq 3.3000002 (0x40533334, not 3.3f); both loops start at amp 0.4, gain 0.6. */
float cz_fbm_b(const cz_ctx *c, int x, int y) {
    float px = (float)x * 2.2f * 0.0009765625f, py = (float)y * 2.2f * 0.0009765625f;
    float s1 = 0.0f, s2 = 0.0f, amp = 0.4f;
    for (int k = 0; k < 7; k++) {
        float n = noise(c, px, py);
        float a = amp * 40.0f; amp = amp * 0.6f;
        s1 = s1 + n * a;
        ROTATE(px, py);
    }
    px = (float)x * 0x1.a66668p+1f * 0.0009765625f; py = (float)y * 0x1.a66668p+1f * 0.0009765625f;
    amp = 0.4f;
    for (int k = 0; k < 7; k++) {
        float n = noise(c, px, py);
        float a = amp * 40.0f; amp = amp * 0.6f;
        s2 = s2 + __builtin_fabsf(n * a);
        ROTATE(px, py);
    }
    return s2 + s1;
}

static inline float clamp01(float v) {
    if (!(0.0f > v) && !(1.0f > v)) return 1.0f;
    if (0.0f > v) return 0.0f;
    return v;
}

/* 0x14004b2c0: one row of `count` RGBA8 pixels at (x0 + i, y0 + row), written
 * to buf + row*count*4. */
void cz_row(const cz_ctx *c, uint8_t *buf, int x0, int y0, int count, int row) {
    uint8_t *o = buf + (long)count * row * 4;
    int y = y0 + row;
    for (int i = 0; i < count; i++, o += 4) {
        float a = cz_fbm_a(c, x0 + i, y), b = cz_fbm_b(c, x0 + i, y);
        float t = clamp01(b * 0.6f + 0.5f);
        float m = clamp01(b + a), im = 1.0f - m;
        float r = clamp01(t + 0.1f) * m + im * 0.2f;
        float g = clamp01(t + 0.2f) * m + im * 0.4f;
        float bl = clamp01(t * 0.9f + 0.3f) * m + im * 0.6f;
        o[0] = (uint8_t)cvtt64(r * 255.0f);
        o[1] = (uint8_t)cvtt64(g * 255.0f);
        o[2] = (uint8_t)cvtt64(bl * 255.0f);
        o[3] = 0xff;
    }
}
