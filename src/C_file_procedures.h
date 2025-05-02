#ifndef __C_FILE_PROCEDURES__
#define __C_FILE_PROCEDURES__

#include <stdio.h>
#include "AD9361_procedures.h"
#include "C_string_procedures.h"

/********************
 * Count lines
********************/
size_t count_lines(const char* path, size_t line_min_length) {
    if (path == NULL) {
        printf(" (!) No path.\n");
        call_shutdown(200);
    }
    FILE * file = fopen(path, "r");
    if (file == NULL) {
        printf(" (!) Could not open file \"%s\"\n", path);
        call_shutdown(200);
    }
    
    size_t nlines = 0;
    char* line = NULL; size_t len = 0;
    while (getline(&line, &len, file) != -1) {
        char* nl = strline(line);
        if (strlen(line) >= line_min_length) nlines++;
        revert_strline(nl);
    }
    fclose(file);
    return nlines;
}
size_t count_nl(const char* path) {
    if (path == NULL) {
        printf(" (!) No path.\n");
        call_shutdown(201);
    }
    FILE * file = fopen(path, "r");
    if (file == NULL) {
        printf(" (!) Could not open file \"%s\"\n", path);
        call_shutdown(201);
    }
    
    size_t nlines = 0;
    int c = fgetc(file);
    if (c != EOF) {
        nlines = (c == '\n') ? 2 : 1;
        while ((c = fgetc(file)) != EOF) {
            if (c == '\n') nlines++;
        }
    }
    fclose(file);
    return nlines;
}

#endif
