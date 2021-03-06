#include <avr/io.h>
#include <util/delay.h>
#include <avr/pgmspace.h>

#include "Screen.h"
#include "font.h"

#define PIN_SCE		PD4
#define PIN_RESET	PC6
#define PIN_DC		PD5
#define PIN_SDIN	PD6
#define PIN_SCLK	PD7

#define LCD_BYTES	504
#define LCD_ROWS	6
#define LCD_COLS	14

const char HEX_TABLE[] PROGMEM = "0123456789ABCDEF";

void spi_write(unsigned char data);
void write_command(unsigned char cmd);
void write_data(unsigned char data);
void set_pos(unsigned char x, unsigned char y);

char text_shadow[84];
unsigned char cursorx = 0;
unsigned char cursory = 0;

unsigned char invert = 0;

void screen_initialize() {
    //initialize pin directions
    DDRD |= (1<<PIN_SCE)|(1<<PIN_SCLK)|(1<<PIN_DC)|(1<<PIN_SDIN);
    DDRC |= (1<<PIN_RESET);

    //reset the LCD
    _delay_ms(200);
    PORTC |= _BV(PIN_RESET);

    //initialization sequence
    write_command(0x21); // switch to extended commands
    write_command(0x80 | 0x40); // set value of Vop (controls contrast) = 0x80 | 0x60 (arbitrary)
    write_command(0x04); // set temperature coefficient
    write_command(0x13); // set bias mode to 1:48.
    write_command(0x20); // switch back to regular commands
    write_command(0x0C); // enable normal display (dark on light), horizontal addressing

    cursorx = 0;
    cursory = 0;

    screen_clear_shadow();
}

void screen_clear_shadow(void) {
    for(unsigned char i=0; i<84; i++) {
        text_shadow[i] = ' ';
    }
}

void screen_clear(unsigned char color) {
    set_pos(0, 0);
    cursorx = 0;
    cursory = 0;

    if(color) {
        for(unsigned int i=0; i<LCD_BYTES; i++) {
            write_data(0xFF);
        }
    } else {
        for(unsigned int i=0; i<LCD_BYTES; i++) {
            write_data(0x00);
        }
    }
}

void screen_print_string(char* addr) {
    char c;
    while((c=*addr)) {
        screen_print_char(c);
        addr++;
    }
}

void screen_print_hex(unsigned char val) {
    screen_print_char(pgm_read_word(&HEX_TABLE[val>>4]));
    screen_print_char(pgm_read_word(&HEX_TABLE[val&0x0F]));
}

void screen_scrollup(void) {
    for(unsigned char i=0; i<70; i++) {
        text_shadow[i] = text_shadow[i+14];
    }

    for(unsigned char i=70; i<84; i++) {
        text_shadow[i] = ' ';
    }

    screen_clear(0x00);

    for(unsigned char i=0; i<70; i++) {
        screen_print_char(text_shadow[i]);
    }

    cursorx = 0;
    cursory = LCD_ROWS-1;
}

void screen_backspace(void) {
    cursorx--;

    if(cursorx<0&&cursory>0) {
        cursorx = LCD_COLS-1;
        cursory--;
    }
}

void screen_newline(void) {
    cursorx = 0;
    cursory++;

    if(cursory>=LCD_ROWS)
        screen_scrollup();
}

void screen_print_char(char c) {
    if(invert) {
        screen_draw_inverted_char(cursorx, cursory, c);
    } else {
        screen_draw_char(cursorx, cursory, c);
    }

    cursorx++;

    if(cursorx>=LCD_COLS) {
        cursorx = 0;
        cursory++;
    }

    if(cursory>=LCD_ROWS) {
        screen_scrollup();
    }
}

unsigned char screen_get_y() {
    return cursorx;
}

unsigned char screen_get_x() {
    return cursory;
}

void screen_set_pos(unsigned char x, unsigned char y) {
    cursorx = x;
    cursory = y;
}

void screen_draw_char(unsigned char x, unsigned char y, char c) {
    text_shadow[y*LCD_COLS+x] = c;

    set_pos(x*6, y);

    int index = (c-32)*6;
    write_data(pgm_read_byte(&font[index]));
    write_data(pgm_read_byte(&font[index+1]));
    write_data(pgm_read_byte(&font[index+2]));
    write_data(pgm_read_byte(&font[index+3]));
    write_data(pgm_read_byte(&font[index+4]));
    write_data(pgm_read_byte(&font[index+5]));
}

void screen_draw_inverted_char(unsigned char x, unsigned char y, char c) {
    text_shadow[y*LCD_COLS+x] = c;

    set_pos(x*6, y);

    int index = (c-32)*6;
    write_data(~pgm_read_byte(&font[index]));
    write_data(~pgm_read_byte(&font[index+1]));
    write_data(~pgm_read_byte(&font[index+2]));
    write_data(~pgm_read_byte(&font[index+3]));
    write_data(~pgm_read_byte(&font[index+4]));
    write_data(~pgm_read_byte(&font[index+5]));
}

void screen_draw_pos(unsigned char x, unsigned char y) {
    set_pos(x, y);
}

void screen_draw_slice(unsigned char slice) {
    write_data(slice);
}

/*void screen_draw_bitmap()
  {
  set_pos(0, 0);

  for(int i=0; i<84*48; i++)
  {
  write_data(pgm_read_byte(&logo[i]));
  }
  }*/

//NOKIA 3310 specific
void spi_write(unsigned char data) {
    PORTD &= ~_BV(PIN_SCLK);	//lower clock

    for(unsigned char bit=0; bit<8; bit++)
    {
        //output data bit
        if(data&(1<<(7-bit)))
        {
            PORTD |= _BV(PIN_SDIN);
        } else {
            PORTD &= ~_BV(PIN_SDIN);
        }

        //clock out data bit
        PORTD |= _BV(PIN_SCLK);
        PORTD &= ~_BV(PIN_SCLK);
    }
}

void write_command(unsigned char cmd) {
    PORTD &= ~_BV(PIN_DC);	//put in command mode
    PORTD &= ~_BV(PIN_SCE);	//enable LCD

    spi_write(cmd);

    PORTD |= _BV(PIN_SCE);	//disable LCD
}

void write_data(unsigned char data) {
    PORTD |= _BV(PIN_DC);	//put in data mode
    PORTD &= ~_BV(PIN_SCE);	//enable LCD

    spi_write(data);

    PORTD |= _BV(PIN_SCE);	//disable LCD
}

void set_pos(unsigned char x, unsigned char y) {
    write_command(0x80|x);
    write_command(0x40|(y&0x07));
}
