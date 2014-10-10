/*
  stmflasher - Open Source ST MCU flash program for *nix
  Copyright (C) 2010 Geoffrey McRae <geoff@spacevs.com>
  Copyright (C) 2011 Steve Markgraf <steve@steve-m.de>
  Copyright (C) 2012 Tormod Volden
  Copyright (C) 2012-2013 Alatar <alatar_@list.ru>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#ifndef _SERIAL_H
#define _SERIAL_H

#include <stdint.h>

#ifdef __WIN32__
  #define SERIAL_DEFAULT_PORTNAME			("COM1")
#else
  #define SERIAL_DEFAULT_PORTNAME			("/dev/ttyS0")
#endif

typedef struct serial serial_t;

typedef enum {
	SERIAL_PARITY_NONE,
	SERIAL_PARITY_EVEN,
	SERIAL_PARITY_ODD
} serial_parity_t;

typedef enum {
	SERIAL_BITS_5,
	SERIAL_BITS_6,
	SERIAL_BITS_7,
	SERIAL_BITS_8
} serial_bits_t;

typedef enum {
	SERIAL_BAUD_50,
	SERIAL_BAUD_75,
	SERIAL_BAUD_110,
	SERIAL_BAUD_134,
	SERIAL_BAUD_150,
	SERIAL_BAUD_200,
	SERIAL_BAUD_300,
	SERIAL_BAUD_600,
	SERIAL_BAUD_1200,
	SERIAL_BAUD_1800,
	SERIAL_BAUD_2400,
	SERIAL_BAUD_4800,
	SERIAL_BAUD_7200,
	SERIAL_BAUD_9600,
	SERIAL_BAUD_14400,
	SERIAL_BAUD_19200,
	SERIAL_BAUD_28800,
	SERIAL_BAUD_38400,
	SERIAL_BAUD_56000,
	SERIAL_BAUD_57600,
	SERIAL_BAUD_76800,
	SERIAL_BAUD_115200,
	SERIAL_BAUD_128000,
	SERIAL_BAUD_230400,
	SERIAL_BAUD_256000,
	SERIAL_BAUD_460800,
	SERIAL_BAUD_500000,
	SERIAL_BAUD_576000,
	SERIAL_BAUD_921600,
	SERIAL_BAUD_1000000,
	SERIAL_BAUD_1152000,
	SERIAL_BAUD_1500000,
	SERIAL_BAUD_2000000,
	SERIAL_BAUD_2500000,
	SERIAL_BAUD_3000000,
	SERIAL_BAUD_3500000,
	SERIAL_BAUD_4000000,

	SERIAL_BAUD_INVALID
} serial_baud_t;

typedef enum {
	SERIAL_STOPBIT_1,
	SERIAL_STOPBIT_2,
} serial_stopbit_t;

typedef enum {
	SERIAL_ERR_OK = 0,

	SERIAL_ERR_WRONG_ARG,
	SERIAL_ERR_NOT_CONFIGURED,
	SERIAL_ERR_SYSTEM,
	SERIAL_ERR_UNKNOWN,
	SERIAL_ERR_INVALID_BAUD,
	SERIAL_ERR_INVALID_BITS,
	SERIAL_ERR_INVALID_PARITY,
	SERIAL_ERR_INVALID_STOPBIT,
	SERIAL_ERR_NODATA
} serial_err_t;

#ifdef __cplusplus
extern "C" {
#endif

serial_t*    serial_open (const char *device);
void         serial_close(serial_t *h);
void         serial_flush(const serial_t *h);
serial_err_t serial_setup(serial_t *h, const serial_baud_t baud, const serial_bits_t bits, const serial_parity_t parity, const serial_stopbit_t stopbit);
serial_err_t serial_write(const serial_t *h, const void *buffer, unsigned int len);
serial_err_t serial_read (const serial_t *h, const void *buffer, unsigned int len, unsigned int *readed);
const char*  serial_get_setup_str(const serial_t *h);

/* common helper functions */
serial_baud_t serial_get_baud            (const unsigned int baud);
const unsigned int serial_get_baud_int   (const serial_baud_t baud);
const unsigned int serial_get_bits_int   (const serial_bits_t bits);
const char         serial_get_parity_str (const serial_parity_t parity);
const unsigned int serial_get_stopbit_int(const serial_stopbit_t stopbit);

#ifdef __cplusplus
}
#endif

#endif
