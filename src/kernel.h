#pragma once
#include <stdint.h>
typedef struct { float *sin_lut; } cz_ctx;   /* [this+0x30] in the original */
void cz_init(cz_ctx *c);
float cz_fbm_a(const cz_ctx *c, int x, int y);
float cz_fbm_b(const cz_ctx *c, int x, int y);
void cz_row(const cz_ctx *c, uint8_t *buf, int x0, int y0, int count, int row);
