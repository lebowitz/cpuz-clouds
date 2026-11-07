/* Native Linux driver reproducing the CPU-Z 17.01.64 benchmark schedule.
 *  0x140004860  phase driver: clear image, multi run (10000 ms), Sleep(10),
 *               clear image, single run (10000 ms).
 *  0x14002e100  one worker slot per logical CPU, each thread pinned to its CPU
 *               (0x14002e580: alertable wait; rows arrive as QueueUserAPC).
 *  0x14002e970  multi: the dispatcher scans slots for an idle one, harvests its
 *               pixel count, Sleep(0), queues the next row there.
 *  0x14002e5b0  single: one slot on the preferred core; the dispatcher spins
 *               with Sleep(0) until the row is done, then queues the next.
 *  Row k of pass p covers pixels (p + i, p + k), i < 1024; after 1024 rows the
 *  pass number increments, so no pixel is recomputed with the same input.
 *  Score = pixels harvested / (float)elapsed_ms (0x14002e8fd, 0x14002ed19);
 *  rows still in flight at the deadline are not counted.
 * Here a per-slot semaphore stands in for the APC and sched_yield for Sleep(0). */
#define _GNU_SOURCE
#include "kernel.h"
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

enum { W = 1024, H = 1024 };

typedef struct {               /* 0x80-byte slot record at [base+0x18] */
    pthread_t th;
    sem_t go;                  /* QueueUserAPC */
    int cpu;
    int x0, y0, count, row;    /* +0x28 +0x2c +0x30 +0x34 */
    atomic_int done_pixels;    /* +0x3c */
    atomic_int busy;           /* +0x40 */
    char pad[64];
} slot_t;

static cz_ctx ctx;

/* Optional log (CZ_LOG=path): one line per harvested row, for the live-score movie. */
typedef struct { long long t_ns; long pixels; int phase, pass, row; } logrec;
static logrec *lg; static long nlg, cap_lg;
static void log_row(long long t, long px, int phase, const slot_t *s) {
    if (!lg || s->done_pixels == 0 || nlg >= cap_lg) return;
    lg[nlg++] = (logrec){ t, px, phase, s->x0, s->row };
}
static uint8_t *image;

static long long now_ns(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec * 1000000000LL + t.tv_nsec;
}

static void *worker(void *arg) {          /* 0x14002e580 + 0x14004b540 */
    slot_t *s = arg;
    for (;;) {
        sem_wait(&s->go);
        cz_row(&ctx, image, s->x0, s->y0, s->count, s->row);
        atomic_store(&s->done_pixels, s->count);
        atomic_store(&s->busy, 0);
    }
    return NULL;
}

static void start_slot(slot_t *s, int cpu) {
    s->cpu = cpu; sem_init(&s->go, 0, 0);
    pthread_create(&s->th, NULL, worker, s);
    if (cpu >= 0) {
        cpu_set_t m; CPU_ZERO(&m); CPU_SET(cpu, &m);
        pthread_setaffinity_np(s->th, sizeof m, &m);
    }
}

static void queue_row(slot_t *s, int pass, int row) {
    s->x0 = pass; s->y0 = pass; s->count = W; s->row = row;
    atomic_store(&s->done_pixels, 0);
    atomic_store(&s->busy, 1);
    sem_post(&s->go);
}

/* 0x14002e970 */
static float run_multi(slot_t *slots, int n, int ms) {
    int pass = 0, row = 0; long pixels = 0;
    memset(image, 0, W * H * 4);
    long long t0 = now_ns();
    for (;;) {
        int found = -1;
        while (found < 0) {
            for (int i = 0; i < n; i++)
                if (!atomic_load(&slots[i].busy)) { pixels += atomic_load(&slots[i].done_pixels); found = i; log_row(now_ns() - t0, pixels, 0, &slots[i]); break; }
            sched_yield();
        }
        queue_row(&slots[found], pass, row);
        if (++row >= H) { row = 0; pass++; }
        if ((now_ns() - t0) / 1000000 >= ms) break;
    }
    long long el = (now_ns() - t0) / 1000000;
    return (float)(int)pixels / (float)el;
}

/* 0x14002e5b0 */
static float run_single(slot_t *s, int ms) {
    int pass = 0, row = 0, pixels = 0;
    memset(image, 0, W * H * 4);
    long long t0 = now_ns();
    for (;;) {
        while (atomic_load(&s->busy)) sched_yield();
        pixels += atomic_load(&s->done_pixels);
        log_row(now_ns() - t0, pixels, 1, s);
        queue_row(s, pass, row);
        if (++row >= H) { row = 0; pass++; }
        if ((now_ns() - t0) / 1000000 >= ms) break;
    }
    long long el = (now_ns() - t0) / 1000000;
    return (float)pixels / (float)el;
}

/* Preferred core: highest ACPI CPPC highest_perf, first on ties (CPU-Z uses the
 * per-core boost/ranking data its topology code collects). */
static int preferred_cpu(int ncpu) {
    int best = 0, bestv = -1;
    for (int i = 0; i < ncpu; i++) {
        char p[96]; snprintf(p, sizeof p, "/sys/devices/system/cpu/cpu%d/acpi_cppc/highest_perf", i);
        FILE *f = fopen(p, "r"); int v = -1;
        if (f) { if (fscanf(f, "%d", &v) != 1) v = -1; fclose(f); }
        if (v > bestv) { bestv = v; best = i; }
    }
    return best;
}

int main(int argc, char **argv) {
    int ms = 10000, mode = 3, threads = (int)sysconf(_SC_NPROCESSORS_ONLN), cpu = -2, dcpu = -1;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-ms") && i + 1 < argc) ms = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-single")) mode = 1;
        else if (!strcmp(argv[i], "-multi")) mode = 2;
        else if (!strcmp(argv[i], "-t") && i + 1 < argc) threads = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-cpu") && i + 1 < argc) cpu = atoi(argv[++i]);
        else if (!strcmp(argv[i], "-dcpu") && i + 1 < argc) dcpu = atoi(argv[++i]);   /* experiment: pin dispatcher */
        else { fprintf(stderr, "usage: %s [-single|-multi] [-ms N] [-t threads] [-cpu N] [-dcpu N]\n", argv[0]); return 2; }
    }
    cz_init(&ctx);
    if (getenv("CZ_LOG")) { cap_lg = 1 << 20; lg = malloc(sizeof(logrec) * cap_lg); }
    if (dcpu >= 0) { cpu_set_t m; CPU_ZERO(&m); CPU_SET(dcpu, &m); sched_setaffinity(0, sizeof m, &m); }
    image = aligned_alloc(64, W * H * 4);
    float single = -1, multi = -1;
    if (mode & 2) {
        slot_t *slots = aligned_alloc(64, sizeof(slot_t) * threads);
        memset(slots, 0, sizeof(slot_t) * threads);
        for (int i = 0; i < threads; i++) start_slot(&slots[i], i);
        multi = run_multi(slots, threads, ms);
        usleep(10000);                       /* Sleep(10) at 0x140004956 */
        /* let in-flight rows finish before the single phase */
        for (int i = 0; i < threads; i++) while (atomic_load(&slots[i].busy)) sched_yield();
    }
    if (mode & 1) {
        slot_t *s = aligned_alloc(64, sizeof(slot_t)); memset(s, 0, sizeof *s);
        start_slot(s, cpu == -2 ? preferred_cpu(threads) : cpu);
        single = run_single(s, ms);
    }
    printf("\"%.2f\",\"%.2f\"\n", single, multi);
    if (lg) { FILE *f = fopen(getenv("CZ_LOG"), "w"); fprintf(f, "phase,t_ns,pixels,pass,row\n");
        for (long i = 0; i < nlg; i++) fprintf(f, "%d,%lld,%ld,%d,%d\n", lg[i].phase, lg[i].t_ns, lg[i].pixels, lg[i].pass, lg[i].row);
        fclose(f); }
    return 0;
}
