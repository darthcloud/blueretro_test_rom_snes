# BlueRetro test

[Download both ROM & FW here](https://github.com/darthcloud/blueretro_test_rom_snes/releases/latest)

Plug a SNES BlueRetro running FW >= v25.07 into either SNES ports.\
This program will search for a Devices on any of the two SNES ports. ($4016 d0, $4017 d0)\
At boot it will try once to enable BlueRetro 8bits mode.  ($4201 d6/d7)

![ROM demo](demo.gif)

8 bytes are read each frames per port in this ROM.\
(Could be reduced to 5bytes in 4bits mode or less if not all axes are needed for your application)

Polling format can be cycle by pressing Start + B.
L/R trigger can be used to activate rumble.

# Polling format

## Enabling extended format
Similar to the SNES rumble, a cmd frame must be sent using $4201 bits d6 (P1) or d7 (P2).\
By default BlueRetro will output SNES standard format (2 bytes).

The sentry value is ASCII 'b' or in hex 0x62.

```
Bytes/Bits
0               1               2               3
7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0

Data on IO line:                <--- Sentry --> <---- Data --->
X X X X X X X X X X X X X X X X 0 1 1 0 0 0 1 0 0 0 0 0 0 0 Y Y

(BlueRetro will also accept cmds that are shift by (at most) 1 bit to the right)

X = Don't care (typically 1s)

Y Y = BR format code:
0 0 = Standard SNES 2 bytes format
0 1 = BlueRetro 8 bytes format (8bits per axis)
1 0 = BlueRetro 5 bytes format (4bits per axis)
```

## BlueRetro 8 bytes format (8bits per axis)

Application can detect this format with the 4bit ID: 0x6

```
Bytes/Bits
0               1               2               3
7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0

Data on D0 line:
<- Buttons Bitfield --> <- ID-> <-- LX axis --> <-- LY axis -->
B Y S S U D L R A X L R 0 1 1 0 X X X X X X X X Y Y Y Y Y Y Y Y

Bytes/Bits
4               5               6               7
7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0

Data on D0 line:
<-- RX axis --> <-- RY axis --> <-- AL axis --> <-- AR axis -->
X X X X X X X X Y Y Y Y Y Y Y Y L L L L L L L L R R R R R R R R
```

## BlueRetro 5 bytes format (4bits per axis)

Application can detect this format with the 4bit ID: 0x7

```
Bytes/Bits
0               1               2               3               4
7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0 7 6 5 4 3 2 1 0

Data on D0 line:
<- Buttons Bitfield --> <- ID-> <- LX-> <- LY-> <- RX-> <- RY-> <- AL-> <- AR->
B Y S S U D L R A X L R 0 1 1 1 X X X X Y Y Y Y X X X X Y Y Y Y L L L L R R R R

```

## Example polling function

Input data and output cmds can sent full duplex as done below (see mset.s):
```asm
_input_poll:
	; strobe
	ldy #1
	sty $4016
	dey
	sty $4016
	ldx #0
@poll_byte:
	ldy #8
	:
		rol _output+0, X
		ror
		rol _output+8, X
		ror
		sta $4201
		lda $4016
		ror
		rol _input+0, X
		lda $4017
		ror
		rol _input+8, X
		dey
		bne :-
	inx
	cpx _byte_cnt
	bcc @poll_byte
	rts
```

# Building
Source code requires CC65.
https://cc65.github.io/

On Windows, place it in an adjacent cc65 folder, run `build.bat`.\
On Ubuntu, `sudo apt install cc65`, run `./build.sh`.

build_runtime.bat is used to rebuild runtime.lib (instructions are contained as comments)
preprocessor.bat is used to diagnose preprocessor usage

# Credits
Based on:\
SNES Mouse test, by Brad Smith 2019\
Updated for SNES: 2022\
http://rainwarrior.ca \
NES version:\
https://forums.nesdev.org/viewtopic.php?p=231608#p231608

sprite.chr is built with & based on https://nesrocks.itch.io/naw

SNES rumble documentation\
https://github.com/LimitedRunGames-Tech/snes-rumble \
by Randy Linden