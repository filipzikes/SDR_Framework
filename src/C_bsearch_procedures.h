#ifndef __C_BSEARCH_PROCEDURES__
#define __C_BSEARCH_PROCEDURES__

#include <stdint.h>     // uint8_t
#include <stdlib.h>     // bsearch


typedef struct {
    const char * word;
    uint8_t code;
} keyword_t;
typedef struct {
    keyword_t * dict;
    uint8_t size;
} dictionary_t;

keyword_t key0 = {};
keyword_t *found_key = NULL;

int keyword_compare(void const* lhs, void const* rhs) {
    return strcmp(((keyword_t const*)(lhs))->word, ((keyword_t const*)(rhs))->word);
}
void get_key(dictionary_t* dic, const char* str, uint8_t* key) {
    key0.word = str;
    found_key = bsearch(&key0, dic->dict, dic->size, sizeof(keyword_t), keyword_compare);
    if (found_key == NULL) {
        *key = 0;
    } else {
        *key = found_key->code;
    }
}

#endif
