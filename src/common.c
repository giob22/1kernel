#include "common.h"

// in modo da poter utilizzare putchar
void putchar(char ch); 

void printf(const char* fmt, ...){
    va_list vargs;
    va_start(vargs, fmt);

    while (*fmt) {
        if (*fmt == '%'){
            fmt++; // skip '%'
            switch (*fmt) { // leggiamo il prossimo carattere
                case '\0': // '%' alla fine della format string
                    putchar('%');
                    goto end;
                case '%': // print '%'
                    putchar('%');
                    break;
                case 's':
                    // print a NULL-terminated string
                    {
                        const char*s = va_arg(vargs, const char*);
                        while (*s) {
                            putchar(*s);
                            s++;
                        }
                        break;
                    }
                case 'd':
                    // printf a integer in decimal
                    {

                        int value = va_arg(vargs, int);
                        unsigned magnitude = value;
                        if (value < 0) {
                            putchar('-');
                            magnitude = -magnitude;
                            //? motivo?
                        }
                        
                        unsigned divisor = 1;
                        while (magnitude / divisor > 9)
                        divisor *= 10;
                    
                        while (divisor > 0) {
                            putchar('0' + magnitude /   divisor);
                            magnitude %= divisor;
                            divisor /= 10;
                        }
                    
                        break;
                    }
                case 'x':
                    // print an integer in hexadecimal
                    {
                        unsigned value = va_arg(vargs, unsigned);
                        for (int i = 7; i >= 0; i--){
                            unsigned nibble = (value >> (i*4) & 0xf);
                            putchar("0123456789abcdef"[nibble]);
                        }
                    }
            }
        }else{
            putchar(*fmt);
        }

        fmt++;
    }

    end:
        va_end(vargs);

}