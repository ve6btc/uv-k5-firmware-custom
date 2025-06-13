#ifndef APP_FOX_H
#define APP_FOX_H

#ifdef ENABLE_FOXHUNT_TX
#include <stdint.h>
#include <stdbool.h>

void FOX_Init(void);
void FOX_TimeSlice500ms(void);
void FOX_StartFound(void);

#endif

#endif // APP_FOX_H
