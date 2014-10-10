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


#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <stdio.h>
#include <assert.h>

#include "serial.h"

struct serial {
	int			fd;
	struct termios		oldtio;
	struct termios		newtio;

	char			configured;
	serial_baud_t		baud;
	serial_bits_t		bits;
	serial_parity_t		parity;
	serial_stopbit_t	stopbit;
};

serial_t* serial_open(const char *device) {
	serial_t *h = calloc(sizeof(serial_t), 1);

	h->fd = open(device, O_RDWR | O_NOCTTY | O_NDELAY);
	if (h->fd < 0) {
		free(h);
		return NULL;
	}
	fcntl(h->fd, F_SETFL, 0);

	tcgetattr(h->fd, &h->oldtio);
	tcgetattr(h->fd, &h->newtio);

	return h;
}

void serial_close(serial_t *h) {
	if(!h || (h->fd <= -1))
		return;

	serial_flush(h);
	tcsetattr(h->fd, TCSANOW, &h->oldtio);
	close(h->fd);
	free(h);
}

void serial_flush(const serial_t *h) {
	if(!h || (h->fd <= -1))
		return;
	tcflush(h->fd, TCIFLUSH);
}

serial_err_t serial_setup(serial_t *h, const serial_baud_t baud, const serial_bits_t bits, const serial_parity_t parity, const serial_stopbit_t stopbit) {
	if(!h || (h->fd <= -1))
		return SERIAL_ERR_NOT_CONFIGURED;

	speed_t		port_baud;
	tcflag_t	port_bits;
	tcflag_t	port_parity;
	tcflag_t	port_stop;

	switch(baud) {
		case SERIAL_BAUD_50     : port_baud = B50     ; break;
		case SERIAL_BAUD_75     : port_baud = B75     ; break;
		case SERIAL_BAUD_110    : port_baud = B110    ; break;
		case SERIAL_BAUD_134    : port_baud = B134    ; break;
		case SERIAL_BAUD_150    : port_baud = B150    ; break;
		case SERIAL_BAUD_200    : port_baud = B200    ; break;
		case SERIAL_BAUD_300    : port_baud = B300    ; break;
		case SERIAL_BAUD_600    : port_baud = B600    ; break;
		case SERIAL_BAUD_1200   : port_baud = B1200   ; break;
		case SERIAL_BAUD_1800   : port_baud = B1800   ; break;
		case SERIAL_BAUD_2400   : port_baud = B2400   ; break;
		case SERIAL_BAUD_4800   : port_baud = B4800   ; break;
		case SERIAL_BAUD_9600   : port_baud = B9600   ; break;
		case SERIAL_BAUD_19200  : port_baud = B19200  ; break;
		case SERIAL_BAUD_38400  : port_baud = B38400  ; break;
#ifdef B7200
		case SERIAL_BAUD_7200   : port_baud = B7200   ; break;
#endif
#ifdef B14400
		case SERIAL_BAUD_14400  : port_baud = B14400  ; break;
#endif
#ifdef B28800
		case SERIAL_BAUD_28800  : port_baud = B28800  ; break;
#endif
#ifdef B57600
		case SERIAL_BAUD_57600  : port_baud = B57600  ; break;
#endif
#ifdef B76800
		case SERIAL_BAUD_76800  : port_baud = B76800  ; break;
#endif
#ifdef B115200
		case SERIAL_BAUD_115200 : port_baud = B115200 ; break;
#endif
#ifdef B230400
		case SERIAL_BAUD_230400 : port_baud = B230400 ; break;
#endif
#ifdef B460800
 		case SERIAL_BAUD_460800 : port_baud = B460800 ; break;
#endif
#ifdef B500000
		case SERIAL_BAUD_500000 : port_baud = B500000 ; break;
#endif
#ifdef B576000
		case SERIAL_BAUD_576000 : port_baud = B576000 ; break;
#endif
#ifdef B921600
		case SERIAL_BAUD_921600 : port_baud = B921600 ; break;
#endif
#ifdef B1000000
		case SERIAL_BAUD_1000000: port_baud = B1000000; break;
#endif
#ifdef B1152000
		case SERIAL_BAUD_1152000: port_baud = B1152000; break;
#endif
#ifdef B1500000
		case SERIAL_BAUD_1500000: port_baud = B1500000; break;
#endif
#ifdef B2000000
		case SERIAL_BAUD_2000000: port_baud = B2000000; break;
#endif
#ifdef B2500000
		case SERIAL_BAUD_2500000: port_baud = B2500000; break;
#endif
#ifdef B3000000
		case SERIAL_BAUD_3000000: port_baud = B3000000; break;
#endif
#ifdef B3500000
		case SERIAL_BAUD_3500000: port_baud = B3500000; break;
#endif
#ifdef B4000000
		case SERIAL_BAUD_4000000: port_baud = B4000000; break;
#endif
		case SERIAL_BAUD_INVALID:
		default:
			return SERIAL_ERR_INVALID_BAUD;
	}

	switch(bits) {
		case SERIAL_BITS_5: port_bits = CS5; break;
		case SERIAL_BITS_6: port_bits = CS6; break;
		case SERIAL_BITS_7: port_bits = CS7; break;
		case SERIAL_BITS_8: port_bits = CS8; break;

		default:
			return SERIAL_ERR_INVALID_BITS;
	}

	switch(parity) {
		case SERIAL_PARITY_NONE: port_parity = 0;               break;
		case SERIAL_PARITY_EVEN: port_parity = PARENB;          break;
		case SERIAL_PARITY_ODD : port_parity = PARENB | PARODD; break;

		default:
			return SERIAL_ERR_INVALID_PARITY;
	}

	switch(stopbit) {
		case SERIAL_STOPBIT_1: port_stop = 0;	   break;
		case SERIAL_STOPBIT_2: port_stop = CSTOPB; break;

		default:
			return SERIAL_ERR_INVALID_STOPBIT;
	}

	/* if the port is already configured, no need to do anything */
	if (
		h->configured        &&
		h->baud	   == baud   &&
		h->bits	   == bits   &&
		h->parity  == parity &&
		h->stopbit == stopbit
	) return SERIAL_ERR_OK;

	/* reset the settings */
	cfmakeraw(&h->newtio);
	h->newtio.c_cflag &= ~(CSIZE | CRTSCTS);
	h->newtio.c_iflag &= ~(IXON | IXOFF | IXANY | IGNPAR);
	h->newtio.c_lflag &= ~(ECHOK | ECHOCTL | ECHOKE);
	h->newtio.c_oflag &= ~(OPOST | ONLCR);

	/* setup the new settings */
	cfsetispeed(&h->newtio, port_baud);
	cfsetospeed(&h->newtio, port_baud);
	h->newtio.c_cflag |=
		port_parity	|
		port_bits	|
		port_stop	|
		CLOCAL		|
		CREAD;
	if(parity != SERIAL_PARITY_NONE) h->newtio.c_iflag |= INPCK;

	h->newtio.c_cc[VMIN ] = 0;
	h->newtio.c_cc[VTIME] = 30; //read timeout 3sec

	/* set the settings */
	serial_flush(h);
	if (tcsetattr(h->fd, TCSANOW, &h->newtio) != 0)
		return SERIAL_ERR_SYSTEM;

	/* confirm they were set */
	struct termios settings;
	tcgetattr(h->fd, &settings);
	if (
		settings.c_iflag != h->newtio.c_iflag ||
		settings.c_oflag != h->newtio.c_oflag ||
		settings.c_cflag != h->newtio.c_cflag ||
		settings.c_lflag != h->newtio.c_lflag
	)	return SERIAL_ERR_UNKNOWN;

	h->configured = 1;
	h->baud	      = baud;
	h->bits	      = bits;
	h->parity     = parity;
	h->stopbit    = stopbit;
	return SERIAL_ERR_OK;
}

serial_err_t serial_write(const serial_t *h, const void *buffer, unsigned int len) {
	if(!h || (h->fd <= -1) || !h->configured)
		return SERIAL_ERR_NOT_CONFIGURED;

	ssize_t r;
	uint8_t *pos = (uint8_t*)buffer;

	while(len > 0) {
		r = write(h->fd, pos, len);
		if (r < 1) return SERIAL_ERR_SYSTEM;

		len -= r;
		pos += r;
	}

	return SERIAL_ERR_OK;
}

serial_err_t serial_read(const serial_t *h, const void *buffer, unsigned int len, unsigned int *readed) {
	if(!h || (h->fd <= -1) || !h->configured)
		return SERIAL_ERR_NOT_CONFIGURED;

	ssize_t r;
	uint8_t *pos = (uint8_t*)buffer;

	while(len > 0) {
		r = read(h->fd, pos, len);
		      if (r == 0) return SERIAL_ERR_NODATA;
		else  if (r <  0) return SERIAL_ERR_SYSTEM;

		len -= r;
		pos += r;
		if(readed) *readed += r;
	}

	return SERIAL_ERR_OK;
}

const char* serial_get_setup_str(const serial_t *h) {
	static char str[11];
	if (!h || !h->configured)
		snprintf(str, sizeof(str), "INVALID");
	else
		snprintf(str, sizeof(str), "%u %d%c%d",
			serial_get_baud_int   (h->baud   ),
			serial_get_bits_int   (h->bits   ),
			serial_get_parity_str (h->parity ),
			serial_get_stopbit_int(h->stopbit)
		);

	return str;
}

