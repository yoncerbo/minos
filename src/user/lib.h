#ifndef INCLUDE_USER_LIB
#define INCLUDE_USER_LIB

#include "cmn/lib.h"

int main(void);

#include "lib.c"

#ifndef ARCH_X64
#error "Only x64 user programs supported!"
#endif

#endif
