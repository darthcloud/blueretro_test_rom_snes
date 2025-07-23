//
// mset.c
// SNES Mouse test, by Brad Smith 2019
// adapted for SNES: 2022
// http://rainwarrior.ca
//
// rework into BlueRetro Test, by Jacques Gagnon 2025

// for debugging performance
//#define PROFILE() ppu_profile(0x41);
#define PROFILE() {}

typedef unsigned char      uint8;
typedef   signed char      sint8;
typedef unsigned short int uint16;
typedef   signed short int sint16;
typedef unsigned long  int uint32;
typedef   signed long  int sint32;

extern uint8* ptr;
extern uint8 i, j, k, l;
extern uint16 ix, jx, kx, lx;
extern uint32 eix, ejx, ekx, elx;
#pragma zpsym("ptr")
#pragma zpsym("i")
#pragma zpsym("j")
#pragma zpsym("k")
#pragma zpsym("l")
#pragma zpsym("ix")
#pragma zpsym("jx")
#pragma zpsym("kx")
#pragma zpsym("lx")
#pragma zpsym("eix")
#pragma zpsym("ejx")
#pragma zpsym("ekx")
#pragma zpsym("elx")

extern uint8 byte_cnt;
extern uint8 input[16];
extern uint8 output[16];
#pragma zpsym("byte_cnt")
#pragma zpsym("input")
#pragma zpsym("output")

extern uint8 sprite_chr[];
#define SPRITE_CHR_SIZE (16 * 16 * 16)

extern uint8 prng(); // 8-bit random value
extern void input_poll();

extern void ppu_latch(uint16 addr); // write address to PPU latch
extern void ppu_direction(uint8 vertical); // set write increment direction
extern void ppu_write(uint8 value); // write value to $2007
extern void ppu_load(uint16 count); // uploads bytes from ptr to $2007 (clobbers ptr)
extern void ppu_fill(uint8 value, uint16 count); // uploads single value to $2007
extern void ppu_ctrl(uint8 v); // $2000, only bits 4-6 count (tile pages, sprite height), applies at next post
extern void ppu_mask(uint8 v); // $2001, applies at next post
extern void ppu_scroll_x(uint16 x);
extern void ppu_scroll_y(uint16 y);
extern void ppu_post(uint8 mode); // waits for next frame and posts PPU update
extern void ppu_profile(uint8 emphasis); // immediate $2001 write, OR with current mask (use bit 0 for greyscale)

// POST_OFF     turn off rendering
// POST_NONE    turn on, no other updates
// POST_UPDATE  turn on, palette, send
// POST_DOUBLE  turn on, palette, send 64 bytes across 2 nametables
#define POST_OFF    1
#define POST_NONE   2
#define POST_UPDATE 3
#define POST_DOUBLE 4

#define PAD_A       0x80
#define PAD_B       0x40
#define PAD_SELECT  0x20
#define PAD_START   0x10
#define PAD_UP      0x08
#define PAD_DOWN    0x04
#define PAD_LEFT    0x02
#define PAD_RIGHT   0x01
#define MOUSE_L     0x40
#define MOUSE_R     0x80

extern uint16 ppu_send_addr;
extern uint8 ppu_send_count;
extern uint8 ppu_send[64];

extern uint8 palette[32];
extern uint8 oam[256];

void cls() // erase nametables
{
	ppu_latch(0x2000);
	ppu_fill(0x20,0x1000);
}

uint8 line = 0;
uint8 oam_pos = 0;
sint8 *axes;
sint8 tmp[4];
uint8 *in, *out;
uint8 type[2];
uint8 cur_port = 0;
uint8 mode[2] = {0, 0};
uint8 wait_release[2] = {0, 0};
uint8 line_offset = 0;
uint8 px_offset = 0;
uint8 io_offset = 0;

void add_sprite(uint8 x, uint8 y, uint8 tile)
{
	oam[oam_pos+0] = y-1;
	oam[oam_pos+1] = tile;
	oam[oam_pos+2] = 0;
	oam[oam_pos+3] = x;
	oam_pos += 4;
}

void add_hex_sprite(uint8 x, uint8 y, uint8 value)
{
	add_sprite(x,y,value>>4);
	add_sprite(x+8,y,value&0xF);
}

void add_dec_sprite(uint8 x, uint8 y, uint8 value)
{
	i = value;
	j = 0;
	if (i >= 200) {
		j = 2;
		i -= 200;
	}
	else if (i >= 100){
		j = 1;
		i -= 100;
	}
	add_sprite(x, y, i % 10);
	x -= 8;
	if (i >= 10 || j > 0) {
		i /= 10;
		add_sprite(x, y, i);
		x -= 8;
	}
	if (j > 0) {
		add_sprite(x, y, j);
		x -= 8;
	}
}

void add_dec_sprite_sign(uint8 x, uint8 y, sint8 value, uint8 sign_ext)
{
	uint8 sign;
	if (sign_ext && value & 0x8) {
		value |= 0xF0;
	}
	sign = (value < 0) ? 1 : 0;
	if (sign) value = 0 - value;

	/*
	// this was too slow
	add_sprite(x,y,value%10);
	value /= 10;
	x -= 8;
	while (value > 0)
	{
		add_sprite(x,y,value%10);
		value /= 10;
		x -= 8;
	}
	*/

	// this was fast enough
	i = (uint8)value;
	j = 0;
	if (i >= 100)
	{
		j=1;
		i-=100;
	}
	add_sprite(x,y,i%10); x -= 8;
	if (i >= 10 || j > 0)
	{
		i /= 10;
		add_sprite(x,y,i); x -= 8;
	}
	if (j > 0)
	{
		add_sprite(x,y,j); x -= 8;
	}
	
	if (sign) add_sprite(x, y, '-' + 0x10);
}

void print_bg(uint8 col, uint8 line, uint8 grey, char *str, uint8 len)
{
	uint8 offset = (grey) ? 0x80 : 0x10;
	ppu_send_count = len;
	for (k = 0; k < len; ++k) {
		ppu_send[k] = str[k] + offset;
	}
	ppu_send_addr = 0x2000 + col + ((2 + line) * 32);
}

void test()
{
	cls();
	
	for (i=0; i<32; i+=4)
	{
		palette[i+0] = 0x0F;
		palette[i+1] = 0x00;
		palette[i+2] = 0x10;
		palette[i+3] = 0x30;
	}
	palette[12+3] = 0x00; // grey background text

	// Title
	print_bg(1, 0, 0, "BlueRetro Test", 14);
	ppu_post(POST_UPDATE);

	// Port 0
	print_bg(2, 2, 0, "P1:", 3);
	ppu_post(POST_UPDATE);

	// Port 1
	print_bg(2, 14, 0, "P2:", 3);
	ppu_post(POST_UPDATE);

	// Origin P1 L
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 26 + ((2 + 4) * 32);
	ppu_post(POST_UPDATE);

	// Origin P1 R
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 26 + ((2 + 10) * 32);
	ppu_post(POST_UPDATE);

	// Origin P1 AL
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 3 + ((2 + 11) * 32);
	ppu_post(POST_UPDATE);

	// Origin P1 AR
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 12 + ((2 + 11) * 32);
	ppu_post(POST_UPDATE);

	// Origin P2 L
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 26 + ((2 + 16) * 32);
	ppu_post(POST_UPDATE);

	// Origin P2 R
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 26 + ((2 + 22) * 32);
	ppu_post(POST_UPDATE);

	// Origin P2 AL
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 3 + ((2 + 23) * 32);
	ppu_post(POST_UPDATE);

	// Origin P2 AR
	ppu_send_count = 1;
	ppu_send[0] = 0x9B;
	ppu_send_addr = 0x2000 + 12 + ((2 + 23) * 32);
	ppu_post(POST_UPDATE);

	// Buttons
	ppu_send_count = 12;
	ppu_send[0] = 0xC2; // B
	ppu_send[1] = 0xD9; // Y
	ppu_send[2] = 0xD3; // Select
	ppu_send[3] = 0xD3; // Start
	ppu_send[4] = 0x9F; // Up
	ppu_send[5] = 0x9C; // Down
	ppu_send[6] = 0x9E; // Left
	ppu_send[7] = 0x9D; // Right
	ppu_send[8] = 0xC1; // A
	ppu_send[9] = 0xD8; // X
	ppu_send[10] = 0xCC; // L
	ppu_send[11] = 0xD2; // R
	ppu_send_addr = 0x2000 + 3 + ((2 + 4) * 32);
	ppu_post(POST_UPDATE);
	ppu_send_count = 12;
	ppu_send_addr = 0x2000 + 3 + ((2 + 16) * 32);
	ppu_post(POST_UPDATE);

	// Axes
	print_bg(3, 6, 0, "LX:      LY:", 12);
	ppu_post(POST_UPDATE);
	print_bg(3, 18, 0, "LX:      LY:", 12);
	ppu_post(POST_UPDATE);
	print_bg(3, 8, 0, "RX:      RY:", 12);
	ppu_post(POST_UPDATE);
	print_bg(3, 20, 0, "RX:      RY:", 12);
	ppu_post(POST_UPDATE);
	print_bg(3, 10, 0, "AL:      AR:", 12);
	ppu_post(POST_UPDATE);
	print_bg(3, 22, 0, "AL:      AR:", 12);
	ppu_post(POST_UPDATE);

	// Instructions
	print_bg(1, 25, 1, "L/R Rumble, St+B Cycle BR Mode", 30);
	ppu_post(POST_UPDATE);

	while (1)
	{
		oam_pos = 0;

		// Update devices type
		type[0] = input[1] & 0xF;
		type[1] = input[9] & 0xF;

		// Init rumble cmd
		for (i = 0; i < sizeof(output); ++i) {
			output[i] = 0xFF;
		}

		// Update displayed device name
		if (cur_port) {
			io_offset = 8;
			line_offset = 12;
		}
		else {
			io_offset = 0;
			line_offset = 0;
		}
		switch (type[cur_port]) {
			case 0x0:
				if (input[2 + io_offset]) {
					print_bg(6, 2 + line_offset, 0, "Controller      ", 16);
				}
				else {
					print_bg(6, 2 + line_offset, 0, "NONE            ", 16);
				}
				break;
			case 0x1:
				print_bg(6, 2 + line_offset, 0, "Mouse           ", 16);
				break;
			case 0x3:
				print_bg(6, 2 + line_offset, 0, "NTT Modem       ", 16);
				break;
			case 0x4:
				print_bg(6, 2 + line_offset, 0, "NTT Controller  ", 16);
				break;
			case 0x6:
				print_bg(6, 2 + line_offset, 0, "BlueRetro (8Bit)", 16);
				break;
			case 0x7:
				print_bg(6, 2 + line_offset, 0, "BlueRetro (4Bit)", 16);
				break;
			case 0xD:
				print_bg(6, 2 + line_offset, 0, "Voice Kun       ", 16);
				break;
			case 0xE:
				switch (input[2 + io_offset]) {
					default:
						print_bg(6, 2 + line_offset, 0, "3rd Party Device", 16);
						break;
				}
				break;
			case 0xF:
				print_bg(6, 2 + line_offset, 0, "Super Scope     ", 16);
				break;
			default:
				print_bg(6, 2 + line_offset, 0, "UNKNOWN         ", 16);
				break;
		}

		for (l = 0; l < 2; ++l) {
			if (l) {
				io_offset = 8;
				line_offset = 12;
				px_offset = 96;
			}
			else {
				io_offset = 0;
				line_offset = 0;
				px_offset = 0;
			}
			in = &input[io_offset];
			out = &output[io_offset];
			axes = (sint8 *)&input[2 + io_offset];

			// Reset rumble
			out[2] = 'r';
			out[3] = 0x00;

			// Buttons
			if (in[0] & 0x80) {
				add_sprite(24, 48 + px_offset, 0x52); // B
			}
			if (in[0] & 0x40) {
				add_sprite(32, 48 + px_offset, 0x69); // Y
			}
			if (in[0] & 0x20) {
				add_sprite(40, 48 + px_offset, 0x63); // Select
			}
			if (in[0] & 0x10) {
				add_sprite(48, 48 + px_offset, 0x63); // Start
			}
			if (in[0] & 0x08) {
				add_sprite(56, 48 + px_offset, 0x2F); // Up
			}
			if (in[0] & 0x04) {
				add_sprite(64, 48 + px_offset, 0x2C); // Down
			}
			if (in[0] & 0x02) {
				add_sprite(72, 48 + px_offset, 0x2E); // Left
			}
			if (in[0] & 0x01) {
				add_sprite(80, 48 + px_offset, 0x2D); // Right
			}
			if (in[1] & 0x80) {
				add_sprite(88, 48 + px_offset, 0x51); // A
			}
			if (in[1] & 0x40) {
				add_sprite(96, 48 + px_offset, 0x68); // X
			}
			if (in[1] & 0x20) {
				add_sprite(104, 48 + px_offset, 0x5C); // L
				out[3] |= 0x0F;
			}
			if (in[1] & 0x10) {
				add_sprite(112, 48 + px_offset, 0x62); // R
				out[3] |= 0xF0;
			}

			byte_cnt = 4;
			switch (type[l]) {
				case 0x0:
					mode[l] = 0;
					add_sprite(0, 0, 0x30);
					break;
				case 0x6:
					if (byte_cnt < 8) {
						byte_cnt = 8;
					}
					byte_cnt = 8;
					mode[l] = 1;
					// Set Rumble base on analog triggers
					out[2] = 'r';
					out[3] = (in[7] & 0xF0) | (in[6] >> 4);

					// Axes
					add_dec_sprite_sign(80, 64 + px_offset, axes[0], 0);
					add_dec_sprite_sign(152, 64 + px_offset, axes[1], 0);
					add_dec_sprite_sign(80, 80 + px_offset, axes[2], 0);
					add_dec_sprite_sign(152, 80 + px_offset, axes[3], 0);
					add_dec_sprite(80, 96 + px_offset, in[6]);
					add_dec_sprite(152, 96 + px_offset, in[7]);

					add_sprite(208 + axes[0]/6, 48 - axes[1]/6 + px_offset, 0x2B);
					add_sprite(208 + axes[2]/6, 96 - axes[3]/6 + px_offset, 0x2B);
					add_sprite(24 + (in[6] >> 2), 104 + px_offset, 0x2B);
					add_sprite(96 + (in[7] >> 2), 104 + px_offset, 0x2B);
					break;
				case 0x7:
					if (byte_cnt < 5) {
						byte_cnt = 5;
					}
					mode[l] = 2;
					// Set Rumble base on analog triggers
					out[2] = 'r';
					out[3] = (in[4] << 4) | (in[4] >> 4);

					// Axes
					add_dec_sprite_sign(80, 64 + px_offset, in[2] >> 4, 1);
					add_dec_sprite_sign(152, 64 + px_offset, in[2] & 0xF, 1);
					add_dec_sprite_sign(80, 80 + px_offset, in[3] >> 4, 1);
					add_dec_sprite_sign(152, 80 + px_offset, in[3] & 0xF, 1);
					add_dec_sprite(80, 96 + px_offset, in[4] >> 4);
					add_dec_sprite(152, 96 + px_offset, in[4] & 0xF);

					for (i = 0; i < 4; ++i) {
						j = i & 0x1;
						if (j) {
							tmp[i] = axes[i >> 1] & 0xF;
						}
						else {
							tmp[i] = axes[i >> 1] >> 4;
						}
						if (tmp[i] & 0x8) {
							tmp[i] |= 0xF0;
						}
					}

					add_sprite(208 + tmp[0]*2, 48 - tmp[1]*2 + px_offset, 0x2B);
					add_sprite(208 + tmp[2]*2, 96 - tmp[3]*2 + px_offset, 0x2B);
					add_sprite(24 + ((in[4] >> 4) << 2), 104 + px_offset, 0x2B);
					add_sprite(96 + ((in[4] & 0xF) << 2), 104 + px_offset, 0x2B);
					break;
			}

			// Toggle controller type (Start + B)
			if ((in[0] & 0x90) == 0x90) {
				wait_release[l] = 1;
			}
			else if (wait_release[l]) {
				++mode[l];
				if (mode[l] > 2) {
					mode[l] = 0;
				}
				out[2] = 'b';
				out[3] = mode[l];
				wait_release[l] = 0;
			}

			// Mute rumble on port 2 for wired controller
			if (l && out[2] == 'r' && !(type[1] == 0x6 || type[1] == 0x7)) {
				out[3] = 0x00;
			}
		}

		while (oam_pos != 0) {
			oam[oam_pos+0] = 0xF0;
			oam_pos += 4;
		}

		PROFILE();
		ppu_post(POST_UPDATE);
		input_poll();
		cur_port ^= 0x1;
	}
}

void main()
{
	// Set how much bytes are polled for each port.
	byte_cnt = 8;
	for (i = 0; i < sizeof(output); ++i) {
		output[i] = 0xFF;
	}
	// Try enable BlueRetro 8bit mode if present.
	output[2] = 'b';
	output[3] = 1;
	output[10] = 'b';
	output[11] = 1;
	input_poll();
	ppu_latch(0x0000);
	ppu_fill(0x00,8*1024);

	ptr = sprite_chr;
	ppu_latch(0x0000);
	ppu_load(SPRITE_CHR_SIZE);

	ppu_ctrl(0x00);

	ppu_scroll_x(0);
	ppu_scroll_y(0);
	test();

	return;
}
