#ifndef __C_STRING_PROCEDURES__
#define __C_STRING_PROCEDURES__

#include <stdio.h>

/********************
 * STRING PROCEDURES
 * @details free str, remove newline
********************/
void free_str(char** str){
    if (*str != NULL) free(*str);
    *str = NULL;
}
char* strline(char* str) {
    char* nl = strpbrk(str, "\n");
    if (nl != NULL) *nl = '\0';
    return nl;
}
void revert_strline(char* nl) {
    if (nl != NULL) *nl = '\n';
}

/********************
 * MALLOC STRING
********************/
void copy_string(const char* str, char** dest) {
    if (*dest != NULL) free(*dest);
    *dest = malloc(strlen(str)+1);
    if (*dest != NULL) strcpy(*dest, str);
}
void set_string(char* str, char** dest) {
    char* nl = strline(str);
    copy_string(str, dest);
    revert_strline(nl);
}

#endif
