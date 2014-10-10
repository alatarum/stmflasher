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

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "utils.h"
#include "serial.h"
#include "stm32.h"
#include "parsers/parser.h"

#include "parsers/binary.h"
#include "parsers/hex.h"

enum {
	MEM_TYPE_ANY,
	MEM_TYPE_FLASH,
	MEM_TYPE_RAM,
	MEM_TYPE_EEPROM
};
enum {
	EXEC_FLAG_NONE = 0,
	EXEC_FLAG_REL,
	EXEC_FLAG_ABS
};

/* device globals */
serial_t	*serial		= NULL;
stm32_t		*stm		= NULL;

void		*p_st		= NULL;
parser_t	*parser		= NULL;

/* settings */
char		*device		= NULL;
serial_baud_t	baudRate	= SERIAL_BAUD_57600;
char		rd	 	= 0; //read memory
char		wr		= 0; //write memory
char		wu		= 0; //write unprotect
char		rp		= 0; //read protect
char		ru		= 0; //read unprotect
char		eraseOnly	= 0; //erase memory
char		mem_type	= MEM_TYPE_FLASH; //target memory region
char		relative_addr	= 1; //use relative addresation for -S option
int		npages		= 0; //pages to erase
int		spage		= -1; //first page to erase
uint32_t	readwrite_len	= 0; //number of read/write bytes
uint32_t	start_addr	= 0; //addr for read/write
char		verify		= 0; //verify data after writing
int		retry		= 10;//number of write retries
char		reset_flag	= 1; //reset device after operation
char		exec_flag	= EXEC_FLAG_NONE; //execute code after operation
uint32_t	execute		= 0; //execution address
char		init_flag	= 1; //send INIT to device
char		force_binary	= 0; //force to use binary parser
char		show_info	= 0; //print device configuration
char		verbose		= 1; //output messages level
char		*filename;	     //name of file to read or write

/* functions */
int  parse_options(int argc, char *argv[]);
void show_help(char *name, char *ser_port);
int calc_workspace(FILE *diag, uint32_t *start, uint32_t *end);

int main(int argc, char* argv[]) {
	int ret = 1;
	parser_err_t perr;
	FILE *diag = stdout;

	if (parse_options(argc, argv) != 0)
		goto close;

	if (rd && filename[0] == '-') {
		diag = stderr;
	}

	if (wr) {
		/* first try hex */
		if (!force_binary) {
			parser = &PARSER_HEX;
			p_st = parser->init();
			if (!p_st) {
				fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
				goto close;
			}
		}

		if (force_binary || (perr = parser->open(p_st, filename, 0)) != PARSER_ERR_OK) {
			if (force_binary || perr == PARSER_ERR_INVALID_FILE) {
				if (!force_binary) {
					parser->close(p_st);
					p_st = NULL;
				}

				/* now try binary */
				parser = &PARSER_BINARY;
				p_st = parser->init();
				if (!p_st) {
					fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
					goto close;
				}
				perr = parser->open(p_st, filename, 0);
			}

			/* if still have an error, fail */
			if (perr != PARSER_ERR_OK) {
				fprintf(stderr, "%s ERROR: %s\n", parser->name, parser_errstr(perr));
				if (perr == PARSER_ERR_SYSTEM) perror(filename);
				goto close;
			}
		}

		if(verbose > 1) fprintf(diag, "Using Parser : %s\n", parser->name);
		/* Assume data from stdin is whole specified range */
		if (filename[0] != '-') {
			unsigned int tmp_size = parser->size(p_st);
			if(readwrite_len == 0)
				readwrite_len = tmp_size;
			if(verbose > 1) {
				fprintf(diag, "Input file size is %d (bytes to write: %d)\n", tmp_size, readwrite_len);
			}
		if(verbose > 1) fprintf(diag, "\n");
		}
	} else {
		parser = &PARSER_BINARY;
		p_st = parser->init();
		if (!p_st) {
			fprintf(stderr, "%s Parser failed to initialize\n", parser->name);
			goto close;
		}
	}

	if(verbose > 1) {
		fprintf(diag, "Openning Serial Port %s\n", device);
	}
	serial = serial_open(device);
	if (!serial) {
		fprintf(stderr, "Failed to open serial port: ");
		perror(device);
		goto close;
	}

	if (serial_setup(
		serial,
		baudRate,
		SERIAL_BITS_8,
		SERIAL_PARITY_EVEN,
		SERIAL_STOPBIT_1
	) != SERIAL_ERR_OK) {
		perror(device);
		goto close;
	}

	if(verbose > 1) {
		fprintf(diag, "Serial Config: %s\n", serial_get_setup_str(serial));
	}
	if (!(stm = stm32_init(serial, init_flag))) goto close;

	if(verbose > 1 || show_info) {
		fprintf(diag, "MCU info\n");
		fprintf(diag, "Device ID     : 0x%04x (%s)\n", stm->pid, stm->dev->name);
		fprintf(diag, "Bootloader Ver: 0x%02x\n", stm->bl_version);
		fprintf(diag, "Option 1      : 0x%02x\n", stm->option1);
		fprintf(diag, "Option 2      : 0x%02x\n", stm->option2);
		fprintf(diag, "- RAM up to   :%4dKiB at 0x%08x\n", (stm->dev->ram_end - stm->dev->ram_start) / 1024, stm->dev->ram_start);
		fprintf(diag, "              :  (%db to 0x%08x reserved by bootloader)\n", stm->dev->ram_bl_res - stm->dev->ram_start , stm->dev->ram_bl_res);
		fprintf(diag, "- System mem  :%4dKiB at 0x%08x\n", (stm->dev->mem_end - stm->dev->mem_start) / 1024, stm->dev->mem_start);
		fprintf(diag, "- Option mem  :  %4dB at 0x%08x\n", stm->dev->opt_end - stm->dev->opt_start + 1, stm->dev->opt_start);
		fprintf(diag, "- Flash up to :%4dKiB at 0x%08x\n", (stm->dev->fl_end - stm->dev->fl_start ) / 1024, stm->dev->fl_start);
		fprintf(diag, "- Flash org.  : %d sectors x %d pages x %d bytes\n", (stm->dev->fl_end - stm->dev->fl_start ) / (stm->dev->fl_ps * stm->dev->fl_pps), stm->dev->fl_pps, stm->dev->fl_ps);
		if(stm->dev->eep_end - stm->dev->eep_start)
			fprintf(diag, "- EEPROM      :%4dKiB at 0x%08x\n", (stm->dev->eep_end - stm->dev->eep_start ) / 1024, stm->dev->eep_start);
		fprintf(diag, "\n");
		fprintf(diag, "Note: specified RAM/Flash sizes are maximum for this chip type.\n");
		fprintf(diag, "      Your chip may have less memory amount!\n");
		fprintf(diag, "\n");
	}

	uint8_t		buffer[256];
	uint32_t	addr, start, end;
	unsigned int	len;
	int		failed = 0;

	if (!calc_workspace(diag, &start, &end)) {
		goto close;
	}

	if (rd) {
		if ((perr = parser->open(p_st, filename, 1)) != PARSER_ERR_OK) {
			fprintf(stderr, "%s ERROR: %s\n", parser->name, parser_errstr(perr));
			if (perr == PARSER_ERR_SYSTEM) perror(filename);
			goto close;
		}

		addr = start;

		fflush(diag);
		while(addr < end) {
			uint32_t left	= end - addr;
			len		= sizeof(buffer) > left ? left : sizeof(buffer);
			if (!stm32_read_memory(stm, addr, buffer, len)) {
				fprintf(stderr, "Failed to read memory at address 0x%08x, target write-protected?\n", addr);
				goto close;
			}
			if (parser->write(p_st, buffer, len) != PARSER_ERR_OK)
			{
				fprintf(stderr, "Failed to write data to file\n");
				goto close;
			}
			addr += len;

			if(verbose) {
				fprintf(diag,
					"\rRead address 0x%08x (%.2f%%) ",
					addr,
					(100.0f / (float)(end - start)) * (float)(addr - start)
				);
				fflush(diag);
			}
		}
		if(verbose) fprintf(diag,	"Done.\n");
		ret = 0;
		goto close;
	} else if (rp) {
		if(verbose) fprintf(diag, "Read-Protecting flash\n");
		/* the device automatically performs a reset after the sending the ACK */
		reset_flag = 0;
		stm32_rprot_memory(stm);
		if(verbose) fprintf(diag,	"Done.\n");
	} else if (ru) {
		if(verbose) fprintf(diag, "Read-UnProtecting flash\n");
		/* the device automatically performs a reset after the sending the ACK */
		reset_flag = 0;
		stm32_runprot_memory(stm);
		if(verbose) fprintf(diag,	"Done.\n");
	} else if (eraseOnly) {
		ret = 0;
		if(verbose) fprintf(diag, "Erasing flash\n");

		if (!stm32_erase_memory(stm, spage, npages)) {
			fprintf(stderr, "Failed to erase memory\n");
			ret = 1;
			goto close;
		}

	} else if (wu) {
		if(verbose) fprintf(diag, "Write-unprotecting flash\n");
		/* the device automatically performs a reset after the sending the ACK */
		reset_flag = 0;
		stm32_wunprot_memory(stm);
		if(verbose) fprintf(diag,	"Done.\n");

	} else if (wr) {
		off_t 	offset = 0;
		ssize_t r;
		unsigned int size;

		if (readwrite_len > (end - start)) {
			fprintf(stderr, "Input file too big\n");
			goto close;
		}
		size = readwrite_len;
		addr = start;

		// TODO: It is possible to write to non-page boundaries, by reading out flash
		//       from partial pages and combining with the input data
		// if ((start % stm->dev->fl_ps) != 0 || (end % stm->dev->fl_ps) != 0) {
		//	fprintf(stderr, "Specified start & length are invalid (must be page aligned)\n");
		//	goto close;
		// }

		// TODO: If writes are not page aligned, we should probably read out existing flash
		//       contents first, so it can be preserved and combined with new data
		if(mem_type == MEM_TYPE_FLASH) {
			if(verbose) {
				fprintf(diag, "Erasing flash... ");
				fflush(diag);
			}
			if (!stm32_erase_memory(stm, spage, npages)) {
				fprintf(stderr, "Failed to erase memory\n");
				goto close;
			}
			if(verbose) fprintf(diag, "Done.\n");
		}
		if(verbose) fflush(diag);

		while(addr < end && offset < size) {
			uint32_t left	= end - addr;
			len		= sizeof(buffer) > left ? left : sizeof(buffer);
			len		= len > size - offset ? size - offset : len;

			if (parser->read(p_st, buffer, &len) != PARSER_ERR_OK) {
				fprintf(stderr, "Failed to read data block from input file\n");
				goto close;
			}

			if (len == 0) {
				if (filename[0] == '-') {
					break;
				} else {
					fprintf(stderr, "Failed to read input file\n");
					goto close;
				}
			}

			do {
				r = len;
				if (!stm32_write_memory(stm, addr, buffer, len)) {
					fprintf(stderr, "Failed to write memory at address 0x%08x\n", addr);
					goto close;
				}

				if (verify) {
					uint8_t compare[len];
					if (!stm32_read_memory(stm, addr, compare, len)) {
						fprintf(stderr, "Failed to read memory at address 0x%08x\n", addr);
						goto close;
					}

					for(r = 0; r < len; ++r) {
						if (buffer[r] != compare[r]) {
							if (failed == retry) {
								fprintf(stderr, "Failed to verify at address 0x%08x, expected 0x%02x and found 0x%02x\n",
									(uint32_t)(addr + r), buffer [r], compare[r]
								);
								goto close;
							}
							++failed;
							break;
						}
					}
					failed = 0;
				}
			} while (r != len);

			addr	+= len;
			offset	+= len;

			if(verbose) {
				fprintf(diag,
					"\rWrote %saddress 0x%08x (%.2f%%) ",
					verify ? "and verified " : "",
					addr,
					(100.0f / size) * offset
				);
				fflush(diag);
			}
		}

		if(verbose) fprintf(diag,	"Done.\n");
		ret = 0;
		goto close;
	} else
		ret = 0;

close:
	if (stm && exec_flag && ret == 0) {
		if (execute == 0)
			execute = stm->dev->fl_start;

		if(verbose) {
			fprintf(diag, "\nStarting execution at address 0x%08x... ", execute);
			fflush(diag);
		}
		if (stm32_go(stm, execute)) {
			reset_flag = 0;
			if(verbose) fprintf(diag, "Done.\n");
		} else {
			if(verbose) fprintf(diag, "Failed.\n");
		}
	}

	if (stm && reset_flag) {
		if(verbose) {
			fprintf(diag, "\nResetting device... ");
			fflush(diag);
		}
		if (stm32_reset_device(stm)) {
			if(verbose) fprintf(diag, "Done.\n");
		} else {
			if(verbose) fprintf(diag, "Failed.\n");
		}
	}

	if (p_st  ) parser->close(p_st);
	if (stm   ) stm32_close  (stm);
	if (serial) serial_close (serial);

	if(verbose) fprintf(diag, "\n");
	return ret;
}

/*
 * Input data: Global variables
 * * stm - device specification
 * * mem_type - target memoty type
 * * start_addr, spage - start of working region
 * * readwrite_len, npages - size of working region
 * * relative_addr - use relative start address
 * * execute - execution address
 * * exec_flag - absolute or relative flag
 * Output data: Arguments
 * * start, end - absolute start and end addresses of working region
 * Output data: Global variables
 * * start_addr, spage - start of working region (start_addr always absolute)
 * * readwrite_len, npages - size of working region
 * * execute - absolute execution address
 * return value: 0 if error; 1 if OK
 */
int calc_workspace(FILE *diag, uint32_t *start, uint32_t *end)
{
	uint32_t tmp_start = 0, tmp_end = 0;
	uint32_t allowed_start;
	uint32_t allowed_end;

/*Step 0. check input*/
	if((mem_type != MEM_TYPE_FLASH) && ((spage >= 0)||(npages > 0)))
	{
		fprintf(stderr, "UNEXPECTED ERROR: Wrong memory type!\n");
		return 0;
	}

/*Step 1. init boundaries of allowed region*/
	switch (mem_type)
	{
	case MEM_TYPE_FLASH:
		allowed_start = stm->dev->fl_start;
		allowed_end  =  stm->dev->fl_end;
		if(verbose > 1)
			fprintf(diag, "Working with Flash\n");
		break;
	case MEM_TYPE_RAM:
		allowed_start = stm->dev->ram_bl_res; //exclude memory, reserved by bootloader
		allowed_end  =  stm->dev->ram_end;
		if(verbose > 1)
			fprintf(diag, "Working with RAM\n");
		break;
	case MEM_TYPE_EEPROM:
		allowed_start = stm->dev->eep_start;
		allowed_end  =  stm->dev->eep_end;
		if((allowed_end - allowed_start) == 0)
		{
			fprintf(stderr, "ERROR: This chip does not have EEPROM\n");
			return 0;
		}
		if(verbose > 1)
			fprintf(diag, "Working with EEPROM\n");
		break;
	case MEM_TYPE_ANY:
		allowed_start = 0;
		allowed_end  =  0xFFFFFFFF;
		if(verbose > 1)
			fprintf(diag, "Working in entire memory space\n");
		break;
	default:
		fprintf(stderr, "ERROR: Memory type not known\n");
		return 0;
	}

/*Step 2. claculate start_addr, spage and execution addr*/
	tmp_start = allowed_start;
	if (spage >= 0) {
		tmp_start = allowed_start + (spage * stm->dev->fl_ps);
	} else {
		if(relative_addr)
			tmp_start += start_addr;
		else
			tmp_start = start_addr;
		if(mem_type == MEM_TYPE_FLASH)
			spage = (tmp_start - stm->dev->fl_start) / stm->dev->fl_ps;
	}
	if(exec_flag == EXEC_FLAG_REL) {
		execute += allowed_start;
		exec_flag = EXEC_FLAG_ABS;
	}

/*Step 3. claculate readwrite_len and npages*/
	if (!readwrite_len && npages)
		readwrite_len = (npages == 0xFFFF)?(allowed_end - allowed_start):(npages * stm->dev->fl_ps);
	if (readwrite_len) {
		tmp_end = tmp_start + readwrite_len;
	} else {
		tmp_end = allowed_end;
		readwrite_len = tmp_end - tmp_start;
	}
	if (mem_type == MEM_TYPE_FLASH) {
		if (npages == 0) {
			npages = 1; //clear spage
			int spage_offset = tmp_start - ((spage * stm->dev->fl_ps) + stm->dev->fl_start);
			int len_from_spage = spage_offset + readwrite_len - 1;
			npages += (len_from_spage) / stm->dev->fl_ps;
		}
		if((spage == 0) && (npages*stm->dev->fl_ps) >= stm->dev->fl_end - stm->dev->fl_start)
		{
			npages = 0xFFFF;	//full memory
		}
	}

/*Step 4. validating*/
	if (tmp_start < allowed_start || tmp_end > allowed_end) {
		fprintf(stderr, "ERROR: Can't fit input to selected region or specified start/length are invalid\n");
		fprintf(stderr, "Start 0x%08x < 0x%08x OR end 0x%08x > 0x%08x\n", tmp_start, allowed_start, tmp_end, allowed_end);
		return 0;
	}
	if ((exec_flag == EXEC_FLAG_ABS) &&
	    (execute < stm->dev->fl_start   || execute >= stm->dev->fl_end) &&
	    (execute < stm->dev->ram_bl_res || execute >= stm->dev->ram_end)) {
		fprintf(stderr, "ERROR: Execution address (0x%08x) must be in flash or RAM\n", execute);
		return 0;
	}
	if(verbose > 1) {
		fprintf(diag, "Starting at 0x%08x stopping at 0x%08x, length is %d bytes\n", tmp_start, tmp_end, readwrite_len);
		if(mem_type == MEM_TYPE_FLASH)
		{
			if(npages == 0xFFFF)
				fprintf(diag, "Affected entire flash memory\n");
			else
				fprintf(diag, "Affected %d pages from page %d\n", npages, spage);
		}
	}

/*Step 5. copy calculated bounaries to output variables*/
	if(start)*start = tmp_start;
	if( end ) *end  = tmp_end;
	return 1;
}

int parse_options(int argc, char *argv[]) {
	int  c;
	char reset = 0;
	char disable_reset = 0;
	char full_erase = 0;
	char show_help_and_exit = 0;

	while((c = getopt(argc, argv, "p:b:r:w:vn:g:ujkeiM:REKfchs:S:V:")) != -1) {
		switch(c) {
			case 'p':
				device = optarg;
				break;
			case 'b':
				baudRate = serial_get_baud(strtoul(optarg, NULL, 0));
				if (baudRate == SERIAL_BAUD_INVALID) {
					fprintf(stderr,	"Invalid baud rate, valid options are:\n");
					for(baudRate = SERIAL_BAUD_1200; baudRate != SERIAL_BAUD_INVALID; ++baudRate)
						fprintf(stderr, " %d\n", serial_get_baud_int(baudRate));
					return 1;
				}
				break;

			case 'r':
			case 'w':
				rd = rd || c == 'r';
				wr = wr || c == 'w';
				if (rd && wr) {
					fprintf(stderr, "ERROR: Invalid options, can't read & write at the same time\n");
					return 1;
				}
				filename = optarg;
				if (filename[0] == '-') {
					force_binary = 1;
				}
				break;
			case 'u':
				wu = 1;
				if (rd || wr) {
					fprintf(stderr, "ERROR: Invalid options, can't write unprotect and read/write at the same time\n");
					return 1;
				}
				break;

			case 'j':
				rp = 1;
				if (rd || wr) {
					fprintf(stderr, "ERROR: Invalid options, can't read protect and read/write at the same time\n");
					return 1;
				}
				break;

			case 'k':
				ru = 1;
				if (rd || wr) {
					fprintf(stderr, "ERROR: Invalid options, can't read unprotect and read/write at the same time\n");
					return 1;
				}
				break;

			case 'e':
				eraseOnly = 1;
				if (rd || wr) {
					fprintf(stderr, "ERROR: Invalid options, can't erase-only and read/write at the same time\n");
					return 1;
				}
				break;

			case 'E':
				full_erase = 1;
				if ((spage > 0) || (npages && (npages < 0xFFFF))) {
					fprintf(stderr, "ERROR: You cannot to specify a page count and full erase at same time");
					return 1;
				}
				spage = 0;
				npages = 0xFFFF;
				break;

			case 'v':
				verify = 1;
				break;

			case 'n':
				retry = strtoul(optarg, NULL, 0);
				break;

			case 'g':
				if(optarg[0] == '+')
					exec_flag = EXEC_FLAG_REL;
				else
					exec_flag = EXEC_FLAG_ABS;
				execute   = strtoul(optarg, NULL, 0);
				if (execute % 4 != 0) {
					fprintf(stderr, "ERROR: Execution address must be word-aligned\n");
					return 1;
				}
				break;
			case 's':
				if (readwrite_len || start_addr) {
					fprintf(stderr, "ERROR: Invalid options, can't specify start page / num pages and start address/length\n");
					return 1;
				} else {
					char *pLen;
					spage = strtoul(optarg, &pLen, 0);
					if (*pLen == ':') {
						pLen++;
						npages = strtoul(pLen, NULL, 0);
					}
					if (npages > 0xFFFF || npages < 0) {
						fprintf(stderr, "ERROR: You need to specify a page count between 0 and 65535");
						return 1;
					}
				}
				if (full_erase && ((spage > 0) || (npages && (npages < 0xFFFF)))) {
					fprintf(stderr, "ERROR: You cannot to specify a page count and full erase at same time");
					return 1;
				}
				break;
			case 'S':
				if ((spage >= 0) || npages) {
					fprintf(stderr, "ERROR: Invalid options, can't specify start page / num pages and start address/length\n");
					return 1;
				} else {
					char *pLen;
					if((optarg[0] == '+') || (optarg[0] == ':'))
						relative_addr = 1;
					else
						relative_addr = 0;
					start_addr = strtoul(optarg, &pLen, 0);
					if (*pLen == ':') {
						pLen++;
						readwrite_len = strtoul(pLen, NULL, 0);
					}
				}
				break;
			case 'M':
				switch(optarg[0])
				{
				case 'f':
					mem_type = MEM_TYPE_FLASH;
					break;
				case 'r':
					mem_type = MEM_TYPE_RAM;
					break;
				case 'e':
					mem_type = MEM_TYPE_EEPROM;
					break;
				case 'a':
					mem_type = MEM_TYPE_ANY;
					fprintf(stderr, "WARNING: Using entire address space. You can damage bootloader's RAM in this mode!\n");
					break;
				default:
					fprintf(stderr, "ERROR: Memory type not known\n");
					return 1;
				}
				break;
			case 'R':
				reset = 1;
				break;
			case 'K':
				disable_reset = 1;
				break;
			case 'f':
				force_binary = 1;
				break;
			case 'i':
				show_info = 1;
				break;
			case 'c':
				init_flag = 0;
				break;
			case 'V':
				verbose = strtoul(optarg, NULL, 0);
				if (verbose > 3 || verbose < 0) {
					fprintf(stderr, "ERROR: wrong verbosity level\n");
					return 1;
				}
				break;
			case 'h':
				show_help_and_exit = 1;
			default:
				show_help_and_exit = 1;
		}
	}

	for (c = optind; c < argc; ++c) {
		if (device) {
			fprintf(stderr, "ERROR: Invalid parameter specified\n");
			show_help(argv[0], SERIAL_DEFAULT_PORTNAME);
			return 1;
		}
		device = argv[c];
	}

	if (device == NULL) {
		fprintf(stderr, "ERROR: Device not specified\n");
		show_help(argv[0], SERIAL_DEFAULT_PORTNAME);
		return 1;
	}

	if (show_help_and_exit) {
		show_help(argv[0], device);
		return 1;
	}

	if (!wr && verify) {
		fprintf(stderr, "ERROR: Invalid usage, -v is only valid when writing\n");
		show_help(argv[0], device);
		return 1;
	}
	if (((spage >= 0) || npages) && mem_type != MEM_TYPE_FLASH) {
		fprintf(stderr, "ERROR: Invalid usage, page-based addressation availeble only for flash\n");
		return 1;
	}
	if ((full_erase || eraseOnly) && (mem_type != MEM_TYPE_FLASH)) {
		fprintf(stderr, "ERROR: Invalid usage, Only flash can be erased with -e and -E\n");
		return 1;
	}
	if (exec_flag && ((mem_type != MEM_TYPE_FLASH)&&(mem_type != MEM_TYPE_RAM))) {
		fprintf(stderr, "ERROR: Invalid usage, Can execute code only from flash or RAM\n");
		return 1;
	}
	if (reset && (disable_reset || exec_flag)) {
		fprintf(stderr, "ERROR: Invalid usage, cannot use -K or -g with -R\n");
		return 1;
	} else if (disable_reset) {
		reset_flag = 0;
	}
	if (!(rd || wr || rp || ru || wu || eraseOnly || exec_flag || show_info || reset)) {
		fprintf(stderr, "ERROR: Nothing to do, use at least one of -rwujkegiR\n");
		return 1;
	}
	return 0;
}

void show_help(char *name, char *ser_port) {
	fprintf(stderr, "stmflasher v0.6.3 current - http://developer.berlios.de/projects/stmflasher/\n\n");
	fprintf(stderr,
		"Usage: %s -p ser_port [-b rate] [-EvKfc] [-S [+]address[:length]] [-s start_page[:n_pages]]\n"
		"	[-n count] [-r|w filename] [-M f|r|e|a] [-ujkeiR] [-g [+]address] [-V level] [-h]\n"
		"\n"
		"	-p ser_port	Serial port name\n"
		"	-b ser_port	Serial port baud rate (default 57600)\n"
		"\n"
		"	-r filename	Read flash to file (stdout if \"-\")\n"
		"	-w filename	Write flash from file (stdin if \"-\")\n"
		"	-u		Disable the flash write-protection\n"
		"	-j		Enable the flash read-protection\n"
		"	-k		Disable the flash read-protection\n"
		"	-e		Erase only\n"
		"	-g [+]address	Start execution at specified address (0 = flash start)\n"
		"	-i		Print information about target device and serial mode\n"
		"	-R 		Reset controller (default for read/write/erase/etc)\n"
		"\n"
		"	-E		Full erase\n"
		"	-v		Verify writes\n"
		"	-n count	Retry failed writes up to count times (default 10)\n"
		"	-S [+]address[:length]	Specify start address and optionally length for\n"
		"				read/write/erase operations\n"
		"	-s start_page[:n_pages]	Specify start address at page <start_page> (0 = flash start)\n"
		"				and optionally number of pages to erase\n"
		"	-M f|r|e|a	Work with specified memory type (read/write/erase operation)\n"
		"			f - Flash (default), r - RAM, e - EEPROM, a - entire address space\n"
		"	-K 		Don`t Reset controller after operation (keep in bootloader)\n"
		"	-f		Force binary parser\n"
		"	-c		Resume the connection (don't send initial INIT)\n"
		"			*Baud rate must be kept the same as the first init*\n"
		"			This is useful with -K or if the reset fails\n"
		"	-V level	Verbose output level (0 - silent, 1 - default, 2 - debug)\n"
		"\n"
		"	-h		Show this help\n"
		"\n"
		"Examples:\n"
		"	Get device information:\n"
		"		%s -p %s -i\n"
		"	Write with verify and then start execution:\n"
		"		%s -p %s -w filename -v -g +0\n"
		"	Show information and read flash to file:\n"
		"		%s -p %s -i -r filename\n"
		"	Read 100 bytes of RAM with offset 0x1000 to stdout:\n"
		"		%s -p %s -r - -Mr -S +0x1000:100\n"
		"	Read first page of flash to file in verbose mode:\n"
		"		%s -p %s -r readed.bin -S :1 -V2\n"
		"	Start execution:\n"
		"		%s -p %s -g 0x0\n",
		name,
		name, ser_port,
		name, ser_port,
		name, ser_port,
		name, ser_port,
		name, ser_port,
		name, ser_port
	);
}

