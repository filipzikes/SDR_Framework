#ifndef __C_MATH_ALGORITHM__
#define __C_MATH_ALGORITHM__

#include <stdint.h>

/********************
 * Complex numbers
********************/
typedef struct {
    int16_t* real;
    int16_t* imag;
    size_t size;
} cplxarr_int16;

typedef struct {
    int32_t* real;
    int32_t* imag;
    size_t size;
} cplxarr_int;

typedef struct {
    double* real;
    double* imag;
    size_t size;
} cplxarr_double;

void create_cplx_arr16(cplxarr_int16* arr, size_t size){
    arr->real = (int16_t*) malloc(size * sizeof(int16_t));
    arr->imag = (int16_t*) malloc(size * sizeof(int16_t));
    if ((arr->real == NULL) || (arr->imag == NULL)) {
        if (arr->real != NULL) free(arr->real);
        if (arr->imag != NULL) free(arr->imag);
        arr->real = NULL;
        arr->imag = NULL;
        arr->size = 0;
        return;
    }
    arr->size = size;
}
void create_cplx_arr(cplxarr_int* arr, size_t size){
    arr->real = (int32_t*) malloc(size * sizeof(int32_t));
    arr->imag = (int32_t*) malloc(size * sizeof(int32_t));
    if ((arr->real == NULL) || (arr->imag == NULL)) {
        if (arr->real != NULL) free(arr->real);
        if (arr->imag != NULL) free(arr->imag);
        arr->real = NULL;
        arr->imag = NULL;
        arr->size = 0;
        return;
    }
    arr->size = size;
}
void create_cplx_arr_double(cplxarr_double* arr, size_t size){
    arr->real = (double*) malloc(size * sizeof(double));
    arr->imag = (double*) malloc(size * sizeof(double));
    if ((arr->real == NULL) || (arr->imag == NULL)) {
        if (arr->real != NULL) free(arr->real);
        if (arr->imag != NULL) free(arr->imag);
        arr->real = NULL;
        arr->imag = NULL;
        arr->size = 0;
        return;
    }
    arr->size = size;
}

/********************
 * Cross-correlation
********************/
void xcorr(cplxarr_double* a, cplxarr_double* b, cplxarr_double* corr) {
    int high, low;
    size_t sz = a->size; int N = (int) sz;
    if (sz != b->size) return;
    if (corr->size != (2*sz-1)) return;

    // R[m+N-1] = a[k+m] x b*[k];
    for (int m = -N+1; m < N; m++) {
        if (m < 0) {
            low = -m;
            high = N;
        } else {
            low = 0;
            high = N - m;
        }
        corr->real[m+N-1] = 0;
        corr->imag[m+N-1] = 0;
        for (int k = low; k < high; k++) {
            corr->real[m+N-1] += a->real[k+m] * b->real[k] + a->imag[k+m] * b->imag[k];
            corr->imag[m+N-1] += a->imag[k+m] * b->real[k] - a->real[k+m] * b->imag[k];
        }
    }
}

#endif
