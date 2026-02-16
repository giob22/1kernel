#include "user.h"

extern char __stack_top[];

__attribute__((noreturn)) void exit(void){
    for (;;);
}

void putchar(char ch){
    // TODO
}

__attribute__((section(".text.start")))
__attribute((naked))
void start(void){
    __asm__ volatile(
        "mv sp, %[stack_top]\n"
        "call main \n"
        "call exit \n"

        :: [stack_top] "r" (__stack_top)
    );
}