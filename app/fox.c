/* Fox hunt (ARDF) beacon transmitter - see app/fox.h
 *
 * Design notes
 * ------------
 * The beacon never blocks: everything runs in 10ms steps from
 * APP_TimeSlice10ms(), so keys, display, UART and the abort paths all keep
 * working while the fox is transmitting.
 *
 * Each transmission follows the classic dedicated-controller pattern:
 * a period of alternating hi-lo hunt tones (easy to swing a beam on),
 * followed by the message in morse code, then silence until the next cycle.
 *
 * TX keying reuses the firmware's normal transmit path -
 * FUNCTION_Select(FUNCTION_TRANSMIT) - but FUNCTION_Transmit() calls
 * FOX_SetupTx() instead of the voice-TX setup, so the beacon:
 *   - tunes to the fox frequency BEFORE the PA is enabled (no spurious
 *     burst on the user's VFO frequency)
 *   - skips all voice-TX chrome (DTMF BOT/EOT ID, roger beep, Apollo tone,
 *     scrambler)
 *   - transmits with the microphone muted
 *
 * The tone is BK4819 tone1 (the same generator the roger beep and 1750Hz
 * tone use, tuning gain 66), keyed on/off with TX mute - the carrier stays
 * up for the whole transmission, which is standard MCW fox behaviour.
 */

#ifdef ENABLE_FOXHUNT_TX

#include <string.h>

#include "app/chFrScanner.h"
#include "app/fox.h"
#include "app/scanner.h"
#include "audio.h"
#include "dcs.h"
#include "driver/backlight.h"
#include "driver/bk4819.h"
#include "driver/eeprom.h"
#include "driver/system.h"
#include "frequencies.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/ui.h"

#ifdef ENABLE_FMRADIO
	#include "app/fm.h"
#endif

// ---------------------------------------------------------------- constants

#define FOX_ARM_DELAY_10ms       1000     // 10s from arming to the first beacon (when no start delay is set)
#define FOX_BOOT_DELAY_10ms      3000     // 30s minimum before the first beacon after an auto-start boot
#define FOX_RETRY_DELAY_10ms      200     // 2s retry when the radio is busy
#define FOX_TX_WATCHDOG_10ms    20000     // 200s absolute cap on one transmission
#define FOX_MIN_TONES_10ms         30     // >=300ms of carrier before the first element (PLL/PA settle)
#define FOX_WARBLE_10ms            25     // hunt tone alternates hi/lo every 250ms
#define FOX_ID_GAP_10ms            50     // pause between the tones and the CW message

// ------------------------------------------------------------------- morse

static const char *const morse_letters[26] = {
	".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
	"-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
	"..-", "...-", ".--", "-..-", "-.--", "--..",
};

static const char *const morse_digits[10] = {
	"-----", ".----", "..---", "...--", "....-",
	".....", "-....", "--...", "---..", "----.",
};

static const char *get_morse(char c)
{
	if (c >= 'A' && c <= 'Z')
		return morse_letters[c - 'A'];
	if (c >= 'a' && c <= 'z')
		return morse_letters[c - 'a'];
	if (c >= '0' && c <= '9')
		return morse_digits[c - '0'];

	switch (c)
	{	// common callsign/beacon punctuation
		case '/': return "-..-.";
		case '.': return ".-.-.-";
		case '-': return "-....-";
		case '?': return "..--..";
		case '=': return "-...-";
	}

	return "";   // anything else is skipped
}

// ------------------------------------------------------------------- state

typedef enum {
	FOX_TX_IDLE = 0,   // not transmitting (armed = counting down, or off)
	FOX_TX_WAKE,       // waking the radio from power save - nothing keyed yet
	FOX_TX_TONES,      // hi-lo hunt tones (or plain carrier when tones_time = 0)
	FOX_TX_ID_GAP,     // short silence between the tones and the message
	FOX_TX_ELEMENT,    // CW tone on (dit or dah)
	FOX_TX_GAP,        // CW tone off (element/character/word spacing)
} FoxTxPhase_t;

static FoxTxPhase_t gTxPhase;         // what the transmitter is doing right now
static uint16_t     gPhaseTicks;      // 10ms ticks left in the current phase
static uint32_t     gWaitTicks;       // 10ms ticks until the next beacon
static uint32_t     gRunTicks;        // 10ms ticks since arming (for the runtime limit)
static uint16_t     gTxTicks;         // 10ms ticks spent in the current transmission (watchdog)
static uint8_t      gDitTicks;        // 10ms ticks per morse unit for this transmission
static bool         gWarbleHigh;      // hunt tone currently on the high note

static char         gTxMessage[32];   // message being sent (built at key-up)
static const char  *gMsgPos;          // current character
static const char  *gCodePos;         // current element within the character's morse code
static uint8_t      gTxPowerLevel;    // 1..10 power level chosen for this transmission

static bool         gFoundMode;
static uint8_t      gFoundCount;      // FOUND transmissions sent so far
static uint32_t     gRng;

// ------------------------------------------------------------------ helpers

static uint8_t fox_cal_bias(uint8_t power)
{	// PA bias for one of the three calibrated power levels (L/M/H) -
	// same per-band calibration lookup RADIO_ConfigureTXPower() uses
	uint8_t txp[3];
	const uint32_t         freq = gEeprom.FOX.frequency;
	const FREQUENCY_Band_t band = FREQUENCY_GetBand(freq);

	EEPROM_ReadBuffer(0x1ED0 + (band * 16) + (power * 3), txp, 3);

	return FREQUENCY_CalculateOutputPower(txp[0], txp[1], txp[2],
		frequencyBandTable[band].lower,
		(frequencyBandTable[band].lower + frequencyBandTable[band].upper) / 2,
		frequencyBandTable[band].upper,
		freq);
}

static uint8_t fox_pa_bias(uint8_t level)
{	// map the fox 1..10 power scale onto the PA bias curve through the
	// radio's calibration anchors: 4 = calibrated LOW, 7 = MID, 10 = HIGH,
	// and 1..3 slide below LOW into the milliwatt range for close-in hunts
	const uint8_t bias_low  = fox_cal_bias(OUTPUT_POWER_LOW);
	const uint8_t bias_mid  = fox_cal_bias(OUTPUT_POWER_MID);
	const uint8_t bias_high = fox_cal_bias(OUTPUT_POWER_HIGH);

	uint8_t lo, hi, step;

	if (level < 1)
		level = 1;
	else if (level > 10)
		level = 10;

	if (level <= 4) {
		lo   = bias_low / 4;
		hi   = bias_low;
		step = level - 1;
	}
	else if (level <= 7) {
		lo   = bias_low;
		hi   = bias_mid;
		step = level - 4;
	}
	else {
		lo   = bias_mid;
		hi   = bias_high;
		step = level - 7;
	}

	return lo + (uint8_t)(((uint16_t)(hi - lo) * step) / 3u);
}

static uint16_t fox_tone_reg71(uint16_t pitch_hz)
{	// same scaling BK4819_PlayTone() applies (scale_freq is private to the driver)
	return (uint16_t)((((uint32_t)pitch_hz * 1353245u) + (1u << 16)) >> 17);
}

static uint16_t fox_pitch(void)
{	// found mode announces on its own pitch so it sounds different
	return (gFoundMode && gEeprom.FOX.found_pitch_hz != 0)
	       ? gEeprom.FOX.found_pitch_hz : gEeprom.FOX.pitch_hz;
}

static void fox_schedule_next(void)
{
	uint32_t interval = gEeprom.FOX.interval_min;

	if (gEeprom.FOX.random && gEeprom.FOX.interval_max > gEeprom.FOX.interval_min)
		interval += gRng % (gEeprom.FOX.interval_max - gEeprom.FOX.interval_min + 1u);

	gWaitTicks = interval * 100u;
}

static bool fox_tx_allowed(void)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT ||
	    gCurrentFunction == FUNCTION_RECEIVE  ||
	    gCurrentFunction == FUNCTION_INCOMING ||
	    gCurrentFunction == FUNCTION_MONITOR)
		return false;                        // radio is busy

	if (gPttIsPressed || gMonitor)
		return false;                        // user needs the radio

	if (gScreenToDisplay == DISPLAY_MENU)
		return false;                        // user is (re)configuring - don't fire mid-menu

	if (SCANNER_IsScanning() || gScanStateDir != SCAN_OFF)
		return false;

	if (SerialConfigInProgress())
		return false;

#ifdef ENABLE_FMRADIO
	if (gFmRadioMode)
		return false;
#endif

	if (gBatteryDisplayLevel == 0 || gBatteryDisplayLevel > 6)
		return false;                        // battery flat / over-voltage

	if (TX_freq_check(gEeprom.FOX.frequency) != 0)
		return false;                        // frequency not TX-able (F-lock etc)

	return true;
}

// tone keying - the carrier stays up, only the tone is keyed
static void fox_tone_on(void)  { BK4819_ExitTxMute();  }
static void fox_tone_off(void) { BK4819_EnterTxMute(); }

static void fox_set_pitch(uint16_t pitch_hz)
{
	BK4819_WriteRegister(BK4819_REG_71, fox_tone_reg71(pitch_hz));
}

// step to the next element / character. returns false when the message is done.
// on return, gTxPhase/gPhaseTicks describe the next tone or gap period.
static bool fox_advance(void)
{
	for (;;)
	{
		if (gCodePos != NULL)
		{
			if (*gCodePos != '\0')
			{	// next element of the current character
				gTxPhase    = FOX_TX_ELEMENT;
				gPhaseTicks = (*gCodePos++ == '-') ? gDitTicks * 3 : gDitTicks;
				fox_tone_on();
				return true;
			}

			// character finished - inter-character gap (3 units, 1 already elapsed)
			gCodePos    = NULL;
			gTxPhase    = FOX_TX_GAP;
			gPhaseTicks = gDitTicks * 2;
			return true;
		}

		if (*gMsgPos == '\0')
			return false;                    // message complete

		if (*gMsgPos == ' ')
		{	// word gap (7 units, 3 already elapsed as the last character gap)
			while (*gMsgPos == ' ')
				gMsgPos++;
			if (*gMsgPos == '\0')
				return false;                // trailing spaces - done
			gTxPhase    = FOX_TX_GAP;
			gPhaseTicks = gDitTicks * 4;
			return true;
		}

		// start the next character (unsupported characters are skipped)
		const char *code = get_morse(*gMsgPos++);
		if (*code != '\0')
			gCodePos = code;
	}
}

static void fox_end_tx(void)
{
	fox_tone_off();
	BK4819_WriteRegister(BK4819_REG_70, 0);
	BK4819_SetAF(BK4819_AF_MUTE);

	BK4819_ToggleGpioOut(BK4819_GPIO5_PIN1_RED, false);   // red LED off

	gTxPhase = FOX_TX_IDLE;

	// restore normal RX operation (PA off, VFO frequency, mic gain, etc)
	RADIO_SetupRegisters(false);
	FUNCTION_Select(FUNCTION_FOREGROUND);

	// found mode can announce a limited number of times, then stand down
	if (gFoundMode && gEeprom.FOX.enabled && gEeprom.FOX.found_repeat > 0) {
		if (++gFoundCount >= gEeprom.FOX.found_repeat) {
			gEeprom.FOX.enabled  = false;
			gRequestSaveSettings = true;
		}
	}

	fox_schedule_next();

	gUpdateStatus  = true;
	gUpdateDisplay = true;
}

static void fox_start_tx(void)
{
	// build the message - found mode announces the hunt is over
	if (gFoundMode) {
		strcpy(gTxMessage, "FOUND ");
		strncat(gTxMessage, gEeprom.FOX.message, sizeof(gTxMessage) - 7);
	} else {
		strncpy(gTxMessage, gEeprom.FOX.message, sizeof(gTxMessage) - 1);
		gTxMessage[sizeof(gTxMessage) - 1] = '\0';
	}

	gMsgPos   = gTxMessage;
	gCodePos  = NULL;
	gDitTicks = (gEeprom.FOX.wpm >= 5) ? 120 / gEeprom.FOX.wpm : 6;
	if (gDitTicks < 3)
		gDitTicks = 3;
	gTxTicks  = 0;

	// pick this transmission's power - optionally wandering around the set
	// level to make signal-strength hunting more interesting
	{
		int level = gEeprom.FOX.power;
		if (gEeprom.FOX.power_var > 0) {
			const int span = gEeprom.FOX.power_var;
			level += (int)(gRng % (unsigned int)((2 * span) + 1)) - span;
		}
		gTxPowerLevel = (level < 1) ? 1 : (level > 10) ? 10 : (uint8_t)level;
	}

	// make sure the app-level TX timeout machinery stays out of the way -
	// the fox has its own watchdog (gTxTicks)
	gTxTimerCountdown_500ms = 0;
	gTxTimeoutReached       = false;

	gPhaseTicks = (uint16_t)gEeprom.FOX.tones_time * 100u;
	if (gPhaseTicks < FOX_MIN_TONES_10ms)
		gPhaseTicks = FOX_MIN_TONES_10ms;
	gWarbleHigh = false;
	gTxPhase    = FOX_TX_TONES;

	// FUNCTION_Transmit() sees FOX_IsTxActive() and calls FOX_SetupTx()
	FUNCTION_Select(FUNCTION_TRANSMIT);

	// start the hunt tones (plain settle carrier when tones_time = 0)
	if (gEeprom.FOX.tones_time > 0)
		fox_tone_on();

	gUpdateStatus = true;
}

// ---------------------------------------------------------------- public

void FOX_SetupTx(void)
{	// called from FUNCTION_Transmit() - key the TX on the FOX frequency.
	// same ordering as RADIO_SetTxParameters(): tune first, PA last.
	const uint32_t freq = gEeprom.FOX.frequency;

	AUDIO_AudioPathOff();
	gEnableSpeaker = false;

	BK4819_ToggleGpioOut(BK4819_GPIO0_PIN28_RX_ENABLE, false);

	// mute everything before the PA comes up - not even a moment of mic audio
	BK4819_EnterTxMute();
	BK4819_SetAF(BK4819_AF_MUTE);
	BK4819_WriteRegister(BK4819_REG_7D, 0xE940);   // mic gain minimum
	BK4819_DisableScramble();

	BK4819_SetFilterBandwidth(BK4819_FILTER_BW_WIDE, true);

	BK4819_SetFrequency(freq);
	BK4819_SetCompander(0);
	BK4819_PrepareTransmit();
	SYSTEM_DelayMs(10);

	BK4819_PickRXFilterPathBasedOnFrequency(freq);

	BK4819_ToggleGpioOut(BK4819_GPIO1_PIN29_PA_ENABLE, true);
	SYSTEM_DelayMs(5);

	BK4819_SetupPowerAmplifier(fox_pa_bias(gTxPowerLevel), freq);
	SYSTEM_DelayMs(10);

	if (gEeprom.FOX.dcs != 0)
	{
		const bool inverted = (gEeprom.FOX.dcs > 104);
		const uint8_t code  = inverted ? gEeprom.FOX.dcs - 105 : gEeprom.FOX.dcs - 1;
		BK4819_SetCDCSSCodeWord(DCS_GetGolayCodeWord(
			inverted ? CODE_TYPE_REVERSE_DIGITAL : CODE_TYPE_DIGITAL, code));
	}
	else if (gEeprom.FOX.ctcss_hz != 0)
		BK4819_SetCTCSSFrequency(gEeprom.FOX.ctcss_hz);
	else
		BK4819_ExitSubAu();

	// set up the beacon tone (tone1, same gain as the 1750Hz tone), still
	// muted - the tones/CW phases key it with BK4819_Exit/EnterTxMute()
	BK4819_WriteRegister(BK4819_REG_70,
		BK4819_REG_70_MASK_ENABLE_TONE1 |
		(66u << BK4819_REG_70_SHIFT_TONE1_TUNING_GAIN));
	fox_set_pitch(fox_pitch());
	BK4819_EnableTXLink();
}

bool FOX_IsTxActive(void)
{	// true only while the fox actually owns the transmitter - WAKE happens
	// before key-up, so a user PTT in that window must win
	return gTxPhase > FOX_TX_WAKE;
}

bool FOX_IsEnabled(void)
{
	return gEeprom.FOX.enabled;
}

bool FOX_RxSleepWanted(void)
{	// with RxOff set, the receiver stays asleep between beacons - the app's
	// power-save loop checks this instead of doing its periodic RX sniff
	return gEeprom.FOX.enabled && gEeprom.FOX.rx_off && gTxPhase == FOX_TX_IDLE;
}

bool FOX_IsFoundMode(void)
{
	return gFoundMode;
}

void FOX_Enable(bool enable)
{
	if (enable && !gEeprom.FOX.enabled)
	{
		gEeprom.FOX.enabled = true;
		gFoundMode          = false;
		gFoundCount         = 0;
		gRunTicks           = 0;
		gWaitTicks          = (gEeprom.FOX.start_delay > 0)
		                      ? (uint32_t)gEeprom.FOX.start_delay * 6000u
		                      : FOX_ARM_DELAY_10ms;
	}
	else if (!enable)
	{
		gEeprom.FOX.enabled = false;
		// if a beacon is on the air the next time slice will de-key it
	}

	gUpdateStatus  = true;
	gUpdateDisplay = true;
}

void FOX_ToggleFound(void)
{
	gFoundMode  = !gFoundMode;
	gFoundCount = 0;

	// announce promptly (but not instantly - the hunter just pressed a button)
	if (gFoundMode && gTxPhase == FOX_TX_IDLE && gWaitTicks > 300)
		gWaitTicks = 300;

	gUpdateStatus  = true;
	gUpdateDisplay = true;
}

uint16_t FOX_SecondsToNextTx(void)
{
	return (gTxPhase == FOX_TX_IDLE) ? (uint16_t)(gWaitTicks / 100u) : 0;
}

void FOX_Init(void)
{
	gTxPhase   = FOX_TX_IDLE;
	gFoundMode = false;
	gRunTicks  = 0;
	gRng       = 12345;

	// enabled at this point only via the AutoTx power-loss recovery option
	gWaitTicks = (gEeprom.FOX.start_delay > 0)
	             ? (uint32_t)gEeprom.FOX.start_delay * 6000u
	             : FOX_BOOT_DELAY_10ms;
	if (gEeprom.FOX.enabled && gWaitTicks < FOX_BOOT_DELAY_10ms)
		gWaitTicks = FOX_BOOT_DELAY_10ms;
}

void FOX_TimeSlice10ms(void)
{
	gRng = (gRng * 1103515245u) + 12345u;    // stepped every tick - free entropy

	if (gEeprom.FOX.enabled && gScreenToDisplay == DISPLAY_MAIN)
	{	// a hidden fox must not glow: whatever BLTime/BLMin say, the light
		// goes fully dark 10 seconds after it last came on (even "always on",
		// and past the BLMin dim level). any key press lights it right back
		// up for whoever walks up to the radio.
		static uint16_t lit_10ms;

		if (BACKLIGHT_IsOn())
		{
			if (++lit_10ms >= 1000)
				BACKLIGHT_TurnOff();
		}
		else
		{
			lit_10ms = 0;
			if (BACKLIGHT_GetBrightness() > 0)
				BACKLIGHT_SetBrightness(0);
		}
	}

	if (gEeprom.FOX.enabled && gEeprom.FOX.runtime_max > 0)
	{	// total-runtime limit - the fox stops itself when the hunt is over
		if (++gRunTicks > (uint32_t)gEeprom.FOX.runtime_max * 6000u)
		{
			gEeprom.FOX.enabled  = false;
			gRequestSaveSettings = true;
			gUpdateStatus        = true;
			// a transmission in progress is de-keyed by the abort path below
		}
	}

	if (gTxPhase == FOX_TX_IDLE)
	{	// between beacons
		if (!gEeprom.FOX.enabled)
			return;

		if (gWaitTicks > 1) {
			gWaitTicks--;
			// keep the fox screen's next-TX countdown ticking
			if ((gWaitTicks % 100) == 0 && gScreenToDisplay == DISPLAY_MAIN)
				gUpdateDisplay = true;
			return;
		}

		if (!fox_tx_allowed()) {
			gWaitTicks = FOX_RETRY_DELAY_10ms;
			return;
		}

		if (gCurrentFunction == FUNCTION_POWER_SAVE) {
			FUNCTION_Select(FUNCTION_FOREGROUND);   // wake the radio first
			gTxPhase    = FOX_TX_WAKE;
			gPhaseTicks = 10;
			return;
		}

		fox_start_tx();
		return;
	}

	if (gTxPhase == FOX_TX_WAKE)
	{	// nothing is keyed yet - if the user grabbed the radio, just stand down
		if (gPttIsPressed || !gEeprom.FOX.enabled) {
			gTxPhase   = FOX_TX_IDLE;
			gWaitTicks = FOX_RETRY_DELAY_10ms;
			return;
		}
		if (--gPhaseTicks == 0) {
			gTxPhase = FOX_TX_IDLE;
			if (fox_tx_allowed())
				fox_start_tx();
			else
				gWaitTicks = FOX_RETRY_DELAY_10ms;
		}
		return;
	}

	// -------- transmitting --------

	// abort paths: watchdog, user pressed PTT, fox disabled from menu/EXIT key
	if (++gTxTicks > FOX_TX_WATCHDOG_10ms) {
		gEeprom.FOX.enabled = false;         // something is very wrong - stay off
		fox_end_tx();
		return;
	}

	if (gPttIsPressed) {
		gEeprom.FOX.enabled = false;         // user override - give the radio back
		fox_end_tx();
		return;
	}

	if (!gEeprom.FOX.enabled) {
		fox_end_tx();
		return;
	}

	switch (gTxPhase)
	{
		case FOX_TX_TONES:
			// alternate the hunt tone between the set pitch and a higher
			// note - much easier to pick out of the noise than a carrier
			if (gEeprom.FOX.tones_time > 0 && (gPhaseTicks % FOX_WARBLE_10ms) == 0) {
				const uint16_t pitch = fox_pitch();
				gWarbleHigh = !gWarbleHigh;
				fox_set_pitch(gWarbleHigh ? pitch + (pitch >> 2) : pitch);
			}
			if (--gPhaseTicks == 0) {
				fox_tone_off();
				fox_set_pitch(fox_pitch());   // CW ID at the base pitch
				gTxPhase    = FOX_TX_ID_GAP;
				gPhaseTicks = FOX_ID_GAP_10ms;
			}
			break;

		case FOX_TX_ID_GAP:
			if (--gPhaseTicks == 0) {
				if (!fox_advance())          // empty message - just end
					fox_end_tx();
			}
			break;

		case FOX_TX_ELEMENT:
			if (--gPhaseTicks == 0) {
				// inter-element gap (1 unit) - fox_advance() extends it to a
				// character/word gap when the character is finished
				fox_tone_off();
				gTxPhase    = FOX_TX_GAP;
				gPhaseTicks = gDitTicks;
			}
			break;

		case FOX_TX_GAP:
			if (--gPhaseTicks == 0) {
				if (!fox_advance())          // message complete
					fox_end_tx();
			}
			break;

		default:
			fox_end_tx();
			break;
	}
}

#endif // ENABLE_FOXHUNT_TX
