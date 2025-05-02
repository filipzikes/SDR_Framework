#ifndef __C_SLEEP__
#define __C_SLEEP__

#include <unistd.h>

void sleep_sec(unsigned int seconds) {
    sleep(seconds);
}

#endif
