/* Fox hunt (ARDF) beacon transmitter
 *
 * Turns the radio into a hidden-transmitter "fox" controller: while armed it
 * periodically keys the transmitter on a configured frequency and sends the
 * configured message in morse code (MCW - a keyed audio tone on FM), like a
 * typical dedicated fox controller.
 *
 * Runs entirely as a non-blocking state machine off the 10ms scheduler slice,
 * so the radio stays fully responsive between (and during) beacons.
 */

#ifndef APP_FOX_H
#define APP_FOX_H

#ifdef ENABLE_FOXHUNT_TX

#include <stdbool.h>
#include <stdint.h>

void FOX_Init(void);              // call once at boot, after SETTINGS_InitEEPROM
void FOX_TimeSlice10ms(void);     // call from APP_TimeSlice10ms

void FOX_Enable(bool enable);     // arm/disarm the beacon (arming starts a short countdown)
bool FOX_IsEnabled(void);
bool FOX_RxSleepWanted(void);     // RxOff: keep the receiver asleep between beacons

void FOX_ToggleFound(void);       // toggle "fox found" mode - beacon message becomes "FOUND <msg>"
bool FOX_IsFoundMode(void);
uint16_t FOX_SecondsToNextTx(void);   // 0 while transmitting

bool FOX_IsTxActive(void);        // true while the beacon owns the transmitter
void FOX_SetupTx(void);           // called by FUNCTION_Transmit() when FOX_IsTxActive()

#endif

#endif // APP_FOX_H
