#include "../user.h"

void main(void) {
    *((volatile int*) 0x80200000) = 0x1234; // istruzione privilegiata, stiamo modificando pagine del kernel
    for (;;);
}