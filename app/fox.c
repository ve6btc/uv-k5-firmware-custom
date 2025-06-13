#ifdef ENABLE_FOXHUNT_TX
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "app/fox.h"
#include <stdio.h>
#include "functions.h"
#include "settings.h"
#include "misc.h"
#include "driver/bk4819.h"
#include "audio.h"
#include "driver/system.h"
#include "app/app.h"
#include "radio.h"
#include "ui/helper.h"
#include "driver/st7565.h"


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

static void play_unit(uint16_t ms, uint16_t pitch)
{
    BK4819_PlaySingleTone(pitch, ms, 0, false);
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

    BK4819_SetFrequency(gEeprom.FOX.frequency);
    if (gEeprom.FOX.ctcss_hz)
        BK4819_SetCTCSSFrequency(gEeprom.FOX.ctcss_hz);
    else
        BK4819_ExitSubAu();

    char displayed[25] = "";
    uint8_t idx = 0;

    UI_DisplayClear();
    char freq[16];
    sprintf(freq, "%3lu.%05lu", gEeprom.FOX.frequency / 100000, gEeprom.FOX.frequency % 100000);
    UI_PrintStringSmallNormal(freq, 0, 127, 0);
    if (gEeprom.FOX.ctcss_hz)
    {
        char tone[10];
        sprintf(tone, "%u.%uHz", gEeprom.FOX.ctcss_hz / 10, gEeprom.FOX.ctcss_hz % 10);
        UI_PrintStringSmallNormal(tone, 0, 127, 1);
    }
    ST7565_BlitFullScreen();

    for (const char *p = msg; *p && idx < sizeof(displayed) - 1; p++) {
        if (*p == ' ') {
            displayed[idx++] = ' ';
            displayed[idx] = '\0';
            UI_PrintStringSmallNormal(displayed, 0, 127, 3);
            ST7565_BlitFullScreen();
            SYSTEM_DelayMs(unit * 7);
            continue;
        }
        const char *code = get_morse(*p);
        displayed[idx++] = *p;
        displayed[idx] = '\0';
        UI_PrintStringSmallNormal(displayed, 0, 127, 3);
        ST7565_BlitFullScreen();
        for (const char *s = code; *s; s++) {
            if (*s == '-')
                play_unit(unit * 3, gEeprom.FOX.pitch_hz);
            else
                play_unit(unit, gEeprom.FOX.pitch_hz);
        }
        SYSTEM_DelayMs(unit * 3);
    }

    APP_EndTransmission();
    FUNCTION_Select(FUNCTION_FOREGROUND);
    UI_DisplayClear();
    ST7565_BlitFullScreen();
}

void FOX_Init(void)
{
    gFoxCountdown_500ms = 0;
    gFoxFoundMode = false;
    if (gEeprom.FOX.pitch_hz == 0)
        gEeprom.FOX.pitch_hz = 800;
    if (gEeprom.FOX.frequency == 0)
        gEeprom.FOX.frequency = gRxVfo->pTX->Frequency;
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
