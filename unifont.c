/* 
 * Unicode/UTF-8 usage in SDL
 * Andreas Textor 2006-09-16
 * textor.andreas@googlemail.com
 *
 * This code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "SDL.h"

// prototypes
static void quit(int rc);
unsigned getbit(unsigned x, int k);
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel);
void drawcharacter(SDL_Surface *surface, int x, int y, Uint32 color,
	char *ch);
void drawtext(SDL_Surface *surface, int x, int y, Uint32 color, char *ch,
	char **font);
static void PrintKey(SDL_keysym *sym, char **font);
int openfile(char *filename, char **font);
#define hex2dec(a) strtol(a, NULL, 16)

// maximal length of font array
#define FONTLEN 0xFFFF

// global screen surface
SDL_Surface *screen;

/* Call this function to exit a SDL program cleanly */
static void quit(int rc)
{
	SDL_Quit();
	exit(rc);
}

/* Cut a single bit out of an integer value
 *  @x: The number from which you want to extract a bit
 *  @i: Which bit do you want to know
 */
inline unsigned getbit(unsigned x, int i){ return (x>>i) & 1; }

/* Draw a single pixel on a SDL surface at position x, y in
 * a given color
 *  @surface: The surface you want to draw on
 *  @x, @y: The coordinates of the pixel
 *  @pixel: The color value, use SDL_MapRGB or SDL_MapRGBA to
 *   create this value
 */
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	int bpp = surface->format->BytesPerPixel;
	// Here p is the address to the pixel we want to set
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

	switch(bpp) {
		case 1:
			*p = pixel;
			break;
		case 2:
			*(Uint16 *)p = pixel;
			break;
		case 3:
			if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
				p[0] = (pixel >> 16) & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = pixel & 0xff;
				} else {
				p[0] = pixel & 0xff;
				p[1] = (pixel >> 8) & 0xff;
				p[2] = (pixel >> 16) & 0xff;
			}
			break;
		case 4:
			*(Uint32 *)p = pixel;
			break;
	}
}

/* Draw a single character from the Unifont at a given position,
 * see the description of the font format here:
 * http://czyborra.com/unifont/HEADER.html
 * @surface: The surface the character should be drawn on
 * @x, @y: The top left coordinates of the character
 * @color: The color value, use SDL_MapRGB or SDL_MapRGBA to
 *  create this value
 * @ch: The string describing the look of the character, see the
 *  above URL for a format description
 */
void drawcharacter(SDL_Surface *surface, int x, int y, Uint32 color,
	char *ch) {

	char line[5];
	int i, j, l;
	int len = strlen(ch);

	if(len != 32 && len != 64) {
		fprintf(stderr, "Error: drawcharacter: Invalid character length\n");
		return;
	}

	for(i = 0; i < len; i += (len/16)) {
		sprintf(line, "%c%c%c%c", ch[i], ch[i+1],
			(len==64?ch[i+2]:'\0'), (len==64?ch[i+3]:'\0'));
		l = hex2dec(line);
		for(j = (len/4)-1; j >= 0; j--)
			if(getbit(l, j))
				putpixel(surface, x+((len/4)-j), y+(i/(len/16)), color);
	}
}

/* Draw a complete UTF-8 encoded string at a given position in a given
 * color to a given surface. Wide characters are recognized, but every
 * more sophisticated Unicode rendering capability such as bidirectional
 * text or correct use of combining characters (diacritics) is not supported. 
 * @surface: The surface the text should be drawn on
 * @x, @y: Top left coordinates of the string
 * @color: The color value, use SDL_MapRGB or SDL_MapRGBA to
 *  create this value
 * @ch: The text to be drawn, this must be null-terminated UTF-8 encoded
 *  string
 * @font: Pointer to the font information array, where each index corresponds
 *  an unicode codepoint and holds the character describing string for
 *  drawcharacter (see function above)
 */
void drawtext(SDL_Surface *surface, int x, int y, Uint32 color, char *ch,
	char **font) {

	int chlen = strlen(ch);
	int i = 0, k;
	int c;					// unicode codepoint
	int followbytes = 0;	// number of following bytes
	int drawx = x;
	unsigned char byte;
	unsigned char chati;

	// UTF-8-decoder
	while(i < chlen) {
		c = 0;
		chati = (unsigned char)ch[i];
		// byte is 0xxxxxxx - no following bytes
		if(chati>>7 == 0)
			c = chati;
		else {
			// check for overlong UTF-8 sequences
			if((chati>>1 == 0x60) ||
				((chati>>2 == 0xE0) && ((i+1 < chlen) &&
					(unsigned char)ch[i+1]>>5 == 0x04)) ||
				((chati>>3 == 0xF0) && ((i+1 < chlen) &&
					(unsigned char)ch[i+1]>>4 == 0x80)) ||
				((chati>>4 == 0xF8) && ((i+1 < chlen) &&
					(unsigned char)ch[i+1]>>3 == 0x10)) ||
				((chati>>5 == 0xFC) && ((i+1 < chlen) &&
					(unsigned char)ch[i+1]>>2 == 0x20))) {
				fprintf(stderr, "Error: Text is no valid UTF-8 "
					"(overlong UTF-8 sequence at byte 0x%02X\n", i);
				c = 0x003F;
				i++;
				continue;
			}

			// get number of following bytes
			if((chati>>1) == 126) { followbytes = 5; c = (chati & 0x01); }
			else if((chati>>2) == 62) { followbytes = 4; c = (chati & 0x03); }
			else if((chati>>3) == 30) { followbytes = 3; c = (chati & 0x07); }
			else if((chati>>4) == 14) { followbytes = 2; c = (chati & 0x0F); }
			else if((chati>>5) == 6) { followbytes = 1; c = (chati & 0x1F); }
			else if((chati>>6) == 2) {
				followbytes = 0;
				fprintf(stderr, "Error: Text is no valid UTF-8 (lone following "
					"byte at 0x%02X\n", i);
			}

			// process following bytes
			for(k = 1; k <= followbytes; k++) {
				if(k > strlen(ch)) {
					fprintf(stderr, "Error: Text is no valid UTF-8 (EOL "
						"reached, follow byte expected)\n");
				} else {
					byte = (unsigned char)ch[k+i];
					// byte is really a following byte?
					if((byte>>6) == 2) {
						c = c << 6;
						c += (byte & 0x3F);
					} else {
						c = 0x003F;
						fprintf(stderr, "Error: Text is no valid UTF-8 "
							"(byte 0x%02X is no valid following byte)", byte);
					}
				}
			}
			i += followbytes;
		}
		drawcharacter(surface, drawx, y, color, font[c]);
		i++;
		drawx += 8;
	}
	SDL_UpdateRect(screen, 0, 0, 640, 480);
}

/* Prints the character corresponding the last pressed key
 * @sym: SDL keysym structure containing data about the keypress
 * @font: Pointer to the font information array, where each index corresponds
 *  an unicode codepoint and holds the character describing string for
 *  drawcharacter (see function above)
 */
static void PrintKey(SDL_keysym *sym, char **font)
{
	static int x = 5;
	int symbol;

	if(sym->unicode > 0) {
		if (sym->sym && strlen(font[sym->unicode]) >= 32 )
			symbol = sym->unicode;
		else
			symbol = 0x003F;	// questionmark
		drawcharacter(screen, x, 200, SDL_MapRGB(screen->format, 0xff, 0xff, 0x00), font[symbol]);
		SDL_UpdateRect(screen, x, 200, 50, 50);
		x += (strlen(font[symbol]) == 32)?8:16;

		printf("key: %lc (0x%04X)\n", sym->unicode, sym->unicode);
	}
}

/* Opens a .hex font description file and stores the raw hex data
 * in the font array (see drawcharacter description above for more
 * information on the font format)
 * @filename: The filename to open
 * @font: The font array to store the font information in
 */
int openfile(char *filename, char **font) {
	FILE *fp;
	char line[70];
	char number[5] = "";
	char ch[65];
	int intnumber = 0;
	fp = fopen(filename, "r");

	if(fp == NULL) {
		printf("Could not open file: %s\n", filename);
		return 1;
	}

	while(!feof(fp)) {
		fscanf(fp, "%[^\n]\n", line);
		strncpy(number, line, 4);
		strncpy(ch, line+5, 65);
		intnumber = hex2dec(number);
		if(intnumber < FONTLEN)
			strcpy(font[intnumber], ch);
	}

	fclose(fp);
	return 0;
}

/* The main function sets up locale (for correct console character output),
 * creates a SDL window, reads the font file, displays some sample UTF-8
 * text (some characters that are used to draw boxes, original by Markus Kuhn,
 * found on a w3.org UTF-8 test site), then enters a loop
 * to wait for keypresses. Each keypress in SDL contains information about
 * the unicode codepoint of the character entered, which is used to directly
 * render that character.
 */
int main(int argc, char *argv[])
{
	// set up locales for utf-8 output
	if (!setlocale(LC_CTYPE, "")) {
		fprintf(stderr, "Can't set the specified locale! "
			"Check LANG, LC_CTYPE, LC_ALL.\n");
		return 1;
	}

	SDL_Event event;
	int done;
	Uint32 videoflags;

	// initialize sdl
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
		return(1);
	}

	videoflags = SDL_SWSURFACE;
	while(argc > 1) {
		--argc;
		if (argv[argc] && !strcmp(argv[argc], "-fullscreen")) {
			videoflags |= SDL_FULLSCREEN;
		} else {
			fprintf(stderr, "Usage: %s [-fullscreen]\n", argv[0]);
			quit(1);
		}
	}

	// set 640x480 video mode
	screen = SDL_SetVideoMode(640, 480, 0, videoflags);
	if (screen == NULL) {
		fprintf(stderr, "Couldn't set 640x480 video mode: %s\n",
			SDL_GetError());
		quit(2);
	}
	int i;
	char *font[FONTLEN];
	for(i = 0; i <= FONTLEN; i++) {
		font[i] = (char*)malloc(sizeof(char) * 65);
		if(font[i] == NULL) {
			fprintf(stderr, "Error: Couldn't allocate font memory\n");
			return 1;
		}
	}
	if(openfile("unifont.hex", font) != 0)
		quit(1);

	// source of this UTF-8-art:
	// http://www.w3.org/2001/06/utf-8-test/UTF-8-demo.html
	char *a[8] = { " ╔══╦══╗  ┌──┬──┐  ╭──┬──╮  ╭──┬──╮  ┏━━┳━━┓  ┎┒┏┑   ╷  ╻ ┏┯┓ ┌┰┐    ▊ ╱╲╱╲╳╳╳",
				   " ║┌─╨─┐║  │╔═╧═╗│  │╒═╪═╕│  │╓─╁─╖│  ┃┌─╂─┐┃  ┗╃╄┙  ╶┼╴╺╋╸┠┼┨ ┝╋┥    ▋ ╲╱╲╱╳╳╳",
				   " ║│╲ ╱│║  │║   ║│  ││ │ ││  │║ ┃ ║│  ┃│ ╿ │┃  ┍╅╆┓   ╵  ╹ ┗┷┛ └┸┘    ▌ ╱╲╱╲╳╳╳",
				   " ╠╡ ╳ ╞╣  ├╢   ╟┤  ├┼─┼─┼┤  ├╫─╂─╫┤  ┣┿╾┼╼┿┫  ┕┛┖┚     ┌┄┄┐ ╎ ┏┅┅┓ ┋ ▍ ╲╱╲╱╳╳╳",
				   " ║│╱ ╲│║  │║   ║│  ││ │ ││  │║ ┃ ║│  ┃│ ╽ │┃  ░░▒▒▓▓██ ┊  ┆ ╎ ╏  ┇ ┋ ▎        ",
				   " ║└─╥─┘║  │╚═╤═╝│  │╘═╪═╛│  │╙─╀─╜│  ┃└─╂─┘┃  ░░▒▒▓▓██ ┊  ┆ ╎ ╏  ┇ ┋ ▏        ",
				   " ╚══╩══╝  └──┴──┘  ╰──┴──╯  ╰──┴──╯  ┗━━┻━━┛           └╌╌┘ ╎ ┗╍╍┛ ┋  ▁▂▃▄▅▆▇█",
				   " Press any key to display corresponding Unicode-character (click to quit)     " };
	int x;
	for(x = 0; x < 8; x++)
		drawtext(screen, 0, 10+(16*x), SDL_MapRGB(screen->format, 0xff, 0xff, 0x00), a[x], font);

	// enable unicode translation for keyboard input
	SDL_EnableUNICODE(1);

	// enable auto repeat
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
	                    SDL_DEFAULT_REPEAT_INTERVAL);

	done = 0;
	while (!done) {
		SDL_WaitEvent(&event);
		switch (event.type) {
			case SDL_KEYDOWN:
				PrintKey(&event.key.keysym, font);
				break;
			case SDL_MOUSEBUTTONDOWN:
			case SDL_QUIT:
				done = 1;
				break;
			default:
				break;
		}
	}

	quit(0);
	return(0);
}

