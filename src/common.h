#pragma once

// necessari per utilizzare la lista di parametri extra.
// definizioni che servono a gestire le funzioni variadiche, ovvero le funzioni che possono accettare un numero variabile di argomenti.

// todo vedi cosa fanno queste definizioni. Servono sicuramente per gestire la lista di argomenti variabile.

#define va_list __builtin_va_list
#define va_start __builtin_va_start
#define va_end __builtin_va_end
#define va_arg __builtin_va_arg


void printf(const char *fmt, ...);