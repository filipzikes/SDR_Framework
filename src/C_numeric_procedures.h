#ifndef __C_NUMERIC_PROCEDURES__
#define __C_NUMERIC_PROCEDURES__

#include "C_bsearch_procedures.h"

keyword_t arr_unit[]  = {{"GHz", 3}, {"Hz", 0}, {"MHz", 2}, {"kHz", 1}};
dictionary_t units = {.dict = arr_unit, .size = 4};

/********************
 * Get value in Hz
********************/
void get_hz(const char* line, long long * freq_hz) {
    double val = 0; char unit[4] = {}; uint8_t code = 0;
    int ret = sscanf(line, "%lf %3s", &val, unit);
    if (ret == 2) {
        get_key(&units, (const char*)unit, &code);
    }
    double exp;
    switch (code) {
        case 1:  exp = 1e3; break;
        case 2:  exp = 1e6; break;
        case 3:  exp = 1e9; break;
        default: exp = 1; break;
    };
    *freq_hz = (long long)(val * exp);
}

#endif
