/* Render the benchmark image (pass p) and the two fbm fields as PPM/PGM.
 * Usage: render <outdir> <pass> */
#include "kernel.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    int p = argc > 2 ? atoi(argv[2]) : 0; char f[512];
    cz_ctx c; cz_init(&c);
    uint8_t *img = malloc(1024 * 1024 * 4);
    for (int y = 0; y < 1024; y++) cz_row(&c, img, p, p, 1024, y);
    snprintf(f, sizeof f, "%s/image_pass%d.ppm", argv[1], p);
    FILE *o = fopen(f, "wb"); fprintf(o, "P6 1024 1024 255\n");
    for (int i = 0; i < 1024 * 1024; i++) fwrite(img + i * 4, 1, 3, o);
    fclose(o);
    for (int k = 0; k < 2; k++) {
        float *v = malloc(1024 * 1024 * 4), lo = 1e30f, hi = -1e30f;
        for (int y = 0; y < 1024; y++) for (int x = 0; x < 1024; x++) {
            float t = k ? cz_fbm_b(&c, p + x, p + y) : cz_fbm_a(&c, p + x, p + y);
            v[y * 1024 + x] = t; if (t < lo) lo = t; if (t > hi) hi = t; }
        snprintf(f, sizeof f, "%s/fbm_%c_pass%d.pgm", argv[1], k ? 'b' : 'a', p);
        o = fopen(f, "wb"); fprintf(o, "P5 1024 1024 255\n");
        for (int i = 0; i < 1024 * 1024; i++) fputc((int)((v[i] - lo) / (hi - lo) * 255), o);
        fclose(o); printf("fbm_%c range %g .. %g\n", k ? 'b' : 'a', lo, hi);
    }
}
