# cpuz-clouds

The CPU-Z benchmark (version 17.01.64, as shipped in CPU-Z 2.15) renders this image. The picture never appears on screen: CPU-Z draws it into a 1024×1024 RGBA buffer, and the score is how many pixels per millisecond the CPU finishes in 10 seconds.

![pass 0](img/frame0.jpg)

[img/clouds-pan.mp4](img/clouds-pan.mp4): pass *p* draws pixels (x + p, y + p), so each pass shifts the same cloud field one pixel diagonally.

`src/kernel.c` is a C reconstruction of the pixel code. Its output is bit-exact against CPU-Z's own functions: zero float difference over 3.1M pixels. Build and render a frame:

```
gcc -O2 -ffp-contract=off -o render src/render.c src/kernel.c && ./render . 0
```

## The math

All arithmetic is 32-bit float. For pixel (x, y) on pass p, use X = x + p and Y = y + p.

**Hash.** The input is a lattice point **p**:

- q₁ = **p**·(127.1, 311.7) and q₂ = **p**·(269.5, 183.3)
- gᵢ = 2·frac(43758.547 · T(frac(qᵢ / 2π))) − 1

T is a 4096-entry table, T[i] = sin(2π·i/256), indexed by ⌊4096·u⌋. So T(u) = sin(32πu), quantized. The inner frac wraps negatives into [0, 1). The outer one truncates toward zero, so negative table values give gᵢ in (−3, −1]. This is the shader-style `fract(sin(dot(p, k)) * 43758.5453)` hash, built on a lookup table.

**Noise n(x, y).** 2D simplex noise:

- Skew with F = (√3 − 1)/2 and unskew with G = (3 − √3)/6.
- Three corners. Each contributes (max(0, ½ − |**d**|²))⁴ · (**g**·**d**), where **d** is the offset to the corner and **g** is the hash of that corner.
- The result is the sum of the three contributions, unscaled.

**Octaves.** R = [[1.6, −1.2], [1.2, 1.6]] rotates by 36.87° and doubles the scale.

    u = 1.1·(X, Y)/1024
    S₁ = Σ_{k<10} 40·0.8·0.7ᵏ · |n(Rᵏu)|
    S₂ = Σ_{k<10} 40·0.7·0.6ᵏ · n(Rᵏu)
    A  = 8·S₁·S₂·(S₁ + S₂) + 0.2

    v = 2.2·(X, Y)/1024,  w = 3.3·(X, Y)/1024
    B  = Σ_{k<7} 40·0.4·0.6ᵏ · n(Rᵏv)  +  Σ_{k<7} 40·0.4·0.6ᵏ · |n(Rᵏw)|

A sets cloud cover and B adds billow detail. One pixel costs 34 noise calls and 102 hashes.

**Color.** clamp means clamp to [0, 1].

    c = clamp(0.6·B + 0.5),  m = clamp(A + B)
    RGB = 255 · [ m·(clamp(c + 0.1), clamp(c + 0.2), clamp(0.9c + 0.3)) + (1 − m)·(0.2, 0.4, 0.6) ]

Where m = 0 the pixel is sky blue (51, 102, 153). Where m = 1 it is cloud, shaded by B.

## Benchmark loop

- One worker thread runs per logical CPU. CPU-Z hands out one 1024-pixel row at a time to whichever worker is idle.
- Score = pixels finished ÷ elapsed ms. The multi-threaded phase runs first, then the single-threaded one, 10 s each.

On a Ryzen 9 9950X, CPU-Z under Wine scores 744 single / 8711 multi. The native C port, compiled to match MSVC's code, scores about 934 / 11266. Those are build-1 figures, from one idle run each.

Academic analysis. This repo does not contain the CPU-Z binary or its disassembly. CPU-Z belongs to CPUID.
