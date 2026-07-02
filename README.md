# Open re-implementation of the Quansheng UV-K5/K6/5R v2.1.27 firmware

This repository is a merge of [OneOfEleven custom firmware](https://github.com/OneOfEleven/uv-k5-firmware-custom) with [fagci spectrum analizer](https://github.com/fagci/uv-k5-firmware-fagci-mod/tree/refactor) plus my few changes.<br>
All is a cloned and customized version of DualTachyon's open firmware found [here](https://github.com/DualTachyon/uv-k5-firmware) ... a cool achievement !

> [!TIP]
> There is a work done by others on forks of this repository. I encourage you to take a look at those too. [SEE HERE](https://github.com/egzumer/uv-k5-firmware-custom/discussions/485)

> [!WARNING]  
> Use this firmware at your own risk (entirely). There is absolutely no guarantee that it will work in any way shape or form on your radio(s), it may even brick your radio(s), in which case, you'd need to buy another radio.
Anyway, have fun.

## Table of Contents

* [Main Features](#main-features)
* [Manual](#manual)
* [Radio Performance](#radio-performance)
* [User Customization](#user-customization)
* [Compiler](#compiler)
* [Building](#building)
* [Credits](#credits)
* [Other sources of information](#other-sources-of-information)
* [License](#license)
* [Example changes/updates](#example-changesupdates)

## Main features:
* many of OneOfEleven mods:
   * AM fix, huge improvement in reception quality
   * long press buttons functions replicating F+ action
   * fast scanning
   * channel name editing in the menu
   * channel name + frequency display option
   * shortcut for scan-list assignment (long press `5 NOAA`)
   * scan-list toggle (long press `* Scan` while scanning)
   * configurable button function selectable from menu
   * battery percentage/voltage on status bar, selectable from menu
   * longer backlight times
   * mic bar
   * RSSI s-meter
   * more frequency steps
   * squelch more sensitive
* fagci spectrum analyzer (**F+5** to turn on)
* some other mods introduced by me:
   * SSB demodulation (adopted from fagci)
   * backlight dimming
   * battery voltage calibration from menu
   * better battery percentage calculation, selectable for 1600mAh or 2200mAh
   * more configurable button functions
   * long press MENU as another configurable button
   * better DCS/CTCSS scanning in the menu (`* SCAN` while in RX DCS/CTCSS menu item)
   * Piotr022 style s-meter
   * restore initial freq/channel when scanning stopped with EXIT, remember last found transmission with MENU button
   * reordered and renamed menu entries
   * LCD interference crash fix
   * many others...

 ## Manual

Up to date manual is available in the [Wiki section](https://github.com/egzumer/uv-k5-firmware-custom/wiki)

## Radio performance

Please note that the Quansheng UV-Kx radios are not professional quality transceivers, their
performance is strictly limited. The RX front end has no track-tuned band pass filtering
at all, and so are wide band/wide open to any and all signals over a large frequency range.

Using the radio in high intensity RF environments will most likely make reception anything but
easy (AM mode will suffer far more than FM ever will), the receiver simply doesn't have a
great dynamic range, which results in distorted AM audio with stronger RX'ed signals.
There is nothing more anyone can do in firmware/software to improve that, once the RX gain
adjustment I do (AM fix) reaches the hardwares limit, your AM RX audio will be all but
non-existent (just like Quansheng's firmware).
On the other hand, FM RX audio will/should be fine.

But, they are nice toys for the price, fun to play with.

## User customization

You can customize the firmware by enabling/disabling various compile options, this allows
us to remove certain firmware features in order to make room in the flash for others.
You'll find the options at the top of "Makefile" ('0' = disable, '1' = enable) ..


|Build option | Description |
| --- | ---- |
|🧰 **STOCK QUANSHENG FEATURES**||
| ENABLE_UART | without this you can't configure radio via PC ! |
| ENABLE_AIRCOPY | easier to just enter frequency with butts |
| ENABLE_FMRADIO | WBFM VHF broadcast band receiver |
| ENABLE_NOAA | everything NOAA (only of any use in the USA) |
| ENABLE_VOICE | want to hear voices ? |
| ENABLE_VOX | |
| ENABLE_ALARM | TX alarms |
| ENABLE_TX1750 | side key 1750Hz TX tone (older style repeater access)|
| ENABLE_PWRON_PASSWORD | power-on password stuff |
| ENABLE_DTMF_CALLING | DTMF calling fuctionality, sending calls, receiving calls, group calls, contacts list etc. |
| ENABLE_FLASHLIGHT | enable top flashlight LED (on, blink, SOS) |
|🧰 **CUSTOM MODS**||
| ENABLE_BIG_FREQ | big font frequencies (like original QS firmware) |
| ENABLE_SMALL_BOLD | bold channel name/no. (when name + freq channel display mode) |
| ENABLE_CUSTOM_MENU_LAYOUT | changes how the menu looks like |
| ENABLE_KEEP_MEM_NAME | maintain channel name when (re)saving memory channel|
| ENABLE_WIDE_RX | full 18MHz to 1300MHz RX (though front-end/PA not designed for full range)|
| ENABLE_TX_WHEN_AM | allow TX (always FM) when RX is set to AM|
| ENABLE_F_CAL_MENU | enable the radios hidden frequency calibration menu |
| ENABLE_CTCSS_TAIL_PHASE_SHIFT | standard CTCSS tail phase shift rather than QS's own 55Hz tone method|
| ENABLE_BOOT_BEEPS | gives user audio feedback on volume knob position at boot-up |
| ENABLE_SHOW_CHARGE_LEVEL | show the charge level when the radio is on charge |
| ENABLE_REVERSE_BAT_SYMBOL | mirror the battery symbol on the status bar (+ pole on the right) |
| ENABLE_NO_CODE_SCAN_TIMEOUT | disable 32-sec CTCSS/DCS scan timeout (press exit butt instead of time-out to end scan) |
| ENABLE_AM_FIX | dynamically adjust the front end gains when in AM mode to help prevent AM demodulator saturation, ignore the on-screen RSSI level (for now) |
| ENABLE_AM_FIX_SHOW_DATA | show debug data for the AM fix |
| ENABLE_SQUELCH_MORE_SENSITIVE | make squelch levels a little bit more sensitive - I plan to let user adjust the values themselves |
| ENABLE_FASTER_CHANNEL_SCAN | increases the channel scan speed, but the squelch is also made more twitchy |
| ENABLE_RSSI_BAR | enable a dBm/Sn RSSI bar graph level in place of the little antenna symbols |
| ENABLE_AUDIO_BAR | experimental, display an audio bar level when TX'ing |
| ENABLE_COPY_CHAN_TO_VFO | copy current channel settings into frequency mode. Long press `1 BAND` when in channel mode |
| ENABLE_SPECTRUM | fagci spectrum analyzer, activated with `F` + `5 NOAA`|
| ENABLE_REDUCE_LOW_MID_TX_POWER | makes medium and low power settings even lower |
| ENABLE_BYP_RAW_DEMODULATORS | additional BYP (bypass?) and RAW demodulation options, proved not to be very useful, but it is there if you want to experiment |
| ENABLE_BLMIN_TMP_OFF | additional function for configurable buttons that toggles `BLMin` on and off wihout saving it to the EEPROM |
| ENABLE_SCAN_RANGES | scan range mode for frequency scanning, see wiki for instructions (radio operation -> frequency scanning) |
| ENABLE_FOXHUNT_TX | enable the built in fox-hunt CW beacon |
|🧰 **DEBUGGING** ||
| ENABLE_AM_FIX_SHOW_DATA| displays settings used by  AM-fix when AM transmission is received |
| ENABLE_AGC_SHOW_DATA | displays AGC settings |
| ENABLE_UART_RW_BK_REGS | adds 2 extra commands that allow to read and write BK4819 registers |
|🧰 **COMPILER/LINKER OPTIONS**||
| ENABLE_CLANG | **experimental, builds with clang instead of gcc (LTO will be disabled if you enable this) |
| ENABLE_SWD | only needed if using CPU's SWD port (debugging/programming) |
| ENABLE_OVERLAY | cpu FLASH stuff, not needed |
| ENABLE_LTO | reduces size of compiled firmware but might break EEPROM reads (OVERLAY will be disabled if you enable this) |

## Compiler

arm-none-eabi GCC version 10.3.1 is recommended, which is the current version on Ubuntu 22.04.03 LTS.
Other versions may generate a flash file that is too big.
You can get an appropriate version from: https://developer.arm.com/downloads/-/gnu-rm

clang may be used but isn't fully supported. Resulting binaries may also be bigger.
You can get it from: https://releases.llvm.org/download.html

## Building

### Github Codespace build method

This is the least demanding option as you don't have to install enything on your computer. All you need is Github account.

1. Go to https://github.com/egzumer/uv-k5-firmware-custom
1. Click green `Code` button
1. Change tab from `Local` to `Codespace`
1. Click green `Create codespace on main` button

<img src="images/codespace1.png" width=700 />

5. Open `Makefile`
1. Edit build options, save `Makefile` changes
1. Run `./compile-with-docker.sh` in terminal window
1. Open folder `compiled-firmware`
1. Right click `firmware.packed.bin`
1. Click `Download`, now you should have a firmware on your computer that you can proceed to flash on your radio. You can use [online flasher](https://egzumer.github.io/uvtools)

<img src="images/codespace2.png" width=700 />

### Docker build method

If you have docker installed you can use [compile-with-docker.bat](./compile-with-docker.bat) (Windows) or [compile-with-docker.sh](./compile-with-docker.sh) (Linux/Mac), the output files are created in `compiled-firmware` folder. This method gives significantly smaller binaries, I've seen differences up to 1kb, so it can fit more functionalities this way. The challenge can be (or not) installing docker itself.

### Windows environment build method

1. Open windows command line and run:
    ```
    winget install -e -h git.git Python.Python.3.8 GnuWin32.Make
    winget install -e -h Arm.GnuArmEmbeddedToolchain -v "10 2021.10"
    ```
2. Close command line, open a new one and run:
    ```
    pip install --user --upgrade pip
    pip install crcmod
    mkdir c:\projects & cd /D c:/projects
    git clone https://github.com/egzumer/uv-k5-firmware-custom.git
    ```
3. From now on you can build the firmware by going to `c:\projects\uv-k5-firmware-custom` and running `win_make.bat` or by running a command line:
    ```
    cd /D c:\projects\uv-k5-firmware-custom
    win_make.bat
    ```
4. To reset the repository and pull new changes run (!!! it will delete all your changes !!!):
    ```
    cd /D c:\projects\uv-k5-firmware-custom
    git reset --hard & git clean -fd & git pull
    ```

I've left some notes in the win_make.bat file to maybe help with stuff.

## Credits

Many thanks to various people on Telegram for putting up with me during this effort and helping:

* [OneOfEleven](https://github.com/OneOfEleven)
* [DualTachyon](https://github.com/DualTachyon)
* [Mikhail](https://github.com/fagci)
* [Andrej](https://github.com/Tunas1337)
* [Manuel](https://github.com/manujedi)
* @wagner
* @Lohtse Shar
* [@Matoz](https://github.com/spm81)
* @Davide
* @Ismo OH2FTG
* [OneOfEleven](https://github.com/OneOfEleven)
* @d1ced95
* and others I forget

## Other sources of information

[ludwich66 - Quansheng UV-K5 Wiki](https://github.com/ludwich66/Quansheng_UV-K5_Wiki/wiki)<br>
[amnemonic - tools and sources of information](https://github.com/amnemonic/Quansheng_UV-K5_Firmware)

## Fox hunt transmitter

> **Ready-to-flash firmware:** grab `*.packed.bin` from the
> [Releases page](../../releases) and flash it with the
> [online flasher](https://egzumer.github.io/uvtools) - no build needed.

This firmware can turn the radio into a **fox** - a hidden transmitter for
amateur radio direction finding (foxhunts / T-hunts). Once armed, the radio
transmits on a schedule all by itself: a period of easy-to-track hi-lo hunt
tones, followed by your callsign in morse code, then silence until the next
cycle - just like a dedicated fox controller, except it's also still your
radio.

### Quick start - hiding your first fox

1. Press `MENU`, scroll to **FoxHnt**, press `MENU`. You are now in the fox menu.
2. Set **Msg** to your callsign (see *entering the message* below).
3. Set **Freq** to your hunt frequency (default is 146.565 MHz, the common
   North American foxhunt frequency) - type 6 digits, e.g. `146565`.
4. Set **Enable** to ON, then back out of the menu with `EXIT` `EXIT`.
5. The screen switches to the dedicated fox display: the transmit
   frequency, a countdown to the next transmission ("ON AIR" while it
   transmits), your message, and the key hints. You have 10 seconds before the
   first transmission - or set **StrDly** first and you could have up to
   2 hours to hike out and hide it.
6. Hide the radio. It will now transmit 15 seconds of tones + your callsign,
   pause 15 seconds, and repeat - out of the box, no other settings needed.

### While the fox is armed

| You press | What happens |
|---|---|
| `EXIT` (main screen) | Beacon stops, even mid-transmission |
| `PTT` | Beacon aborts instantly and you get your radio back |
| `F` | Toggles **found** mode - the fox keeps transmitting but now sends `FOUND <your message>` at the FndTon pitch so it sounds different. After FndRpt announcements it stands down by itself. Press `F` again to undo. |

The fox is polite: it will not transmit while you are listening to a signal,
talking, scanning, sitting in the menu, or using the FM radio - it just waits
and tries again a couple of seconds later.

### The fox menu

| Item | What it does | Range (default) |
|---|---|---|
| **Enable** | Arm / disarm the beacon | OFF (needs a message first) |
| **Freq**   | Transmit frequency | 137-174 / 400-470 MHz (146.565) |
| **Msg**    | The morse message - your callsign (A-Z, 0-9, space) | up to 23 characters |
| **Speed**  | Morse speed | 5-30 WPM (20) |
| **TxPwr**  | Transmit power on a 1-10 scale: 10 = full power (~5W), 7 = the radio's MID, 4 = LOW, and 1-3 go below LOW into milliwatt territory for close-in hunts | 1-10 (4) |
| **PwrVar** | Each transmission randomly varies the power up/down by up to this many levels - makes signal-strength hunting trickier | OFF-5 (OFF) |
| **Tones**  | Seconds of hi-lo hunt tones before the morse ID | 0-60 s (15) |
| **Interv** | Silence between transmissions | 5-600 s (15) |
| **IntMax** | Longest silence, when Random is ON | up to 600 s |
| **Random** | Randomise each pause between Interv and IntMax - makes the fox harder to predict | OFF |
| **StrDly** | Wait this long after arming before the first transmission - time to hide the fox and get away | 0-120 min (OFF = 10 s) |
| **MaxRun** | The fox disarms itself after this long - no all-night transmitter if nobody finds it | 0-480 min (OFF = run forever) |
| **AutoTx** | Stay armed through a power cycle / battery swap. With this OFF (the default) the fox always powers up silent. | OFF |
| **RxOff**  | Keep the receiver asleep between transmissions - the fox is deaf while armed, but the battery lasts much longer. Works even with BatSav off. | OFF |
| **Pitch**  | Tone pitch | 400-1200 Hz (600) |
| **CTCSS**  | Sub-audible tone under the beacon, if you want it | OFF |
| **DCS**    | Digital squelch code instead (normal then inverted codes) - setting one of CTCSS/DCS clears the other | OFF |
| **FndTon** | Tone pitch used in found mode, so the "all over" message is audibly different | 400-1200 Hz (800) |
| **FndRpt** | How many FOUND announcements before the fox stops itself (CONT = keep going until disarmed) | CONT-20 (10) |
| **Found**  | Toggle found mode from the menu (same as the `F` key) | OFF |

Tips:

* A lower `TxPwr` makes a *harder* hunt and a longer battery life. The default
  (4) matches the radio's LOW setting; try 1-2 for a small park.
* 15s tones / 15s pause (the defaults) is the classic 50% duty cycle used by
  commercial fox controllers. For a lazier fox, raise **Interv**.
* Everything except *Enable* and *Found* is remembered across reboots.

### Battery life

Between transmissions the radio power-saves exactly as it normally does, so
the standard settings apply - recommended for an all-day fox:

* **RxOff** ON (fox menu) - the receiver stays fully asleep between beacons;
  or **BatSav** `1:4` (main menu) if you want the radio to keep listening
* The backlight handles itself: while the fox is armed it always goes fully
  dark 10 seconds after it last lit (ignoring BLTime/BLMin - a hidden radio
  must not glow). Pressing any key lights it back up.
* **BLTime** `1 sec`, or **BLTxRx** OFF - the backlight is the silent battery
  killer in a radio that transmits unattended
* **TxPwr** as low as the hunt allows, and a modest **Interv** - transmit
  time dominates consumption
* **MaxRun** set to the hunt length, so a lost fox isn't still transmitting
  at midnight

### Entering the message

Select **Msg**, press `MENU` to start editing, then:

* `UP` / `DOWN` - change the current character (cycles space, A-Z, 0-9)
* number keys - type digits directly
* `MENU` - next character &nbsp;&nbsp; `*` - go back a character &nbsp;&nbsp; `F` - space
* When you're done, press `MENU` until `SURE?` appears, then `MENU` once more
  to save. `EXIT` abandons the edit.

While editing, the message shows in large text with the character you are on
highlighted as an inverted block. The `_` characters mean "not typed yet" -
stepping past one with `MENU` turns it into a real space, and any left at the
end disappear when you save. **A message is required before the fox will
arm** (your licence requires the fox to identify, too).

### Build notes

The fox is included when `ENABLE_FOXHUNT_TX = 1` in the Makefile. It costs
about 5 KB of flash, so the spectrum analyser (`ENABLE_SPECTRUM`) is turned
off in this configuration - the two do not fit together in the radio's 60 KB
flash. Build with `ENABLE_FOXHUNT_TX=0 ENABLE_SPECTRUM=1` to get the standard
firmware back.

## License

Copyright 2023 Dual Tachyon
https://github.com/DualTachyon

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.

## Example changes/updates

<p float="left">
  <img src="/images/image1.png" width=300 />
  <img src="/images/image2.png" width=300 />
  <img src="/images/image3.png" width=300 />
</p>

Video showing the AM fix working ..

<video src="/images/AM_fix.mp4"></video>

<video src="https://github.com/OneOfEleven/uv-k5-firmware-custom/assets/51590168/2a3a9cdc-97da-4966-bf0d-1ce6ad09779c"></video>
