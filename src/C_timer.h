#ifndef __C_TIMER__
#define __C_TIMER__

#include <time.h>
#include <stdio.h>

struct timespec tw1, tw2;
clock_t t1, t2;

double dif, difw;

void timer_start() {
    dif = 0.0; difw = 0.0;
    t1 = clock();
    clock_gettime(CLOCK_MONOTONIC, &tw1);
}
void timer_pause() {
    t2 = clock();
    clock_gettime(CLOCK_MONOTONIC, &tw2);
    dif += 1000.0 * (t2 - t1) / CLOCKS_PER_SEC;
    difw += 1000.0 * tw2.tv_sec + 1e-6 * tw2.tv_nsec
        - (1000.0 * tw1.tv_sec + 1e-6 * tw1.tv_nsec);
}
void timer_resume() {
    t1 = clock();
    clock_gettime(CLOCK_MONOTONIC, &tw1);
}
void timer_stop() {
    timer_pause();
}
void timer_print_full() {
    printf("CPU time used (per clock()): %.2f ms\n", dif);  // double dur;
    printf("Wall time passed: %.2f ms\n\n", difw);    // double posix_wall;
}
void timer_print_wall() {
    printf("%.2f", difw);
}
double timer_get_wall() {
    return difw;
}

#endif
