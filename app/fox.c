#ifdef ENABLE_FOXHUNT_TX
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "app/fox.h"
#include "functions.h"
#include "settings.h"
#include "misc.h"
#include "driver/bk4819.h"
#include "audio.h"

volatile uint16_t gFoxCountdown_500ms;
bool gFoxFoundMode;

static const char *morse_table[36] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---", "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-", "..-", "...-", ".--", "-..-", "-.--", "--..", // A-Z
    "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...", "---..", "----." // 0-9
};

static const char *get_morse(char c)
{
    if (c >= 'A' && c <= 'Z')
        return morse_table[c - 'A'];
    if (c >= 'a' && c <= 'z')
        return morse_table[c - 'a'];
    if (c >= '0' && c <= '9')
        return morse_table[26 + c - '0'];
    return "";
}

static void play_unit(uint16_t ms)
{
    BK4819_PlaySingleTone(800, ms, 0, false);
    SYSTEM_DelayMs(ms);
    BK4819_EnterTxMute();
    SYSTEM_DelayMs(ms);
}

static void send_morse(const char *msg, uint8_t wpm)
{
    if (wpm == 0)
        wpm = 10;
    uint16_t unit = 1200 / wpm;

    FUNCTION_Select(FUNCTION_TRANSMIT);
    SYSTEM_DelayMs(20);

    for (const char *p = msg; *p; p++) {
        if (*p == ' ') {
            SYSTEM_DelayMs(unit * 7);
            continue;
        }
        const char *code = get_morse(*p);
        for (const char *s = code; *s; s++) {
            if (*s == '-')
                play_unit(unit * 3);
            else
                play_unit(unit);
        }
        SYSTEM_DelayMs(unit * 3);
    }

    APP_EndTransmission();
    FUNCTION_Select(FUNCTION_FOREGROUND);
}

void FOX_Init(void)
{
    gFoxCountdown_500ms = 0;
    gFoxFoundMode = false;
}

void FOX_TimeSlice500ms(void)
{
    if (gFoxFoundMode) {
        send_morse("FOX FOUND", gEeprom.FOX.wpm);
        gFoxFoundMode = false;
        gFoxCountdown_500ms = gEeprom.FOX.interval_min * 2;
        return;
    }

    if (!gEeprom.FOX.enabled)
        return;

    if (gFoxCountdown_500ms > 0) {
        gFoxCountdown_500ms--;
        return;
    }

    send_morse(gEeprom.FOX.message, gEeprom.FOX.wpm);
    uint16_t interval = gEeprom.FOX.interval_min;
    if (gEeprom.FOX.random && gEeprom.FOX.interval_max > gEeprom.FOX.interval_min) {
        interval += rand() % (gEeprom.FOX.interval_max - gEeprom.FOX.interval_min + 1);
    }
    gFoxCountdown_500ms = interval * 2;
}

void FOX_StartFound(void)
{
    gFoxFoundMode = true;
}

#endif
