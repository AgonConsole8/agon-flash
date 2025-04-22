/*
 * Title:			Agon firmware upgrade utility
 * Author:			Jeroen Venema
 * Created:			17/12/2022
 * Last Updated:	13/04/2025
 * 
 * Modinfo:
 * 17/12/2022:		Initial version
 * 05/04/2022:		Changed timer to 5sec at reset.
 *                  Sends cls just before reset
 * 07/06/2023:		Included faster crc32, by Leigh Brown
 * 14/10/2023:		VDP update code, MOS update rewritten for simplicity
 * 02/11/2023:		Batched mode, rewrite of UI
 * 13/04/2025:      Ported to agondev
 * 22/04/2025:      echoVDP now asks for screen dimensions specifically
 *                  Added DEBUG options to debug updating VDP
 */

// DEBUG if set to 1:
// - PortC bit position 0 upon entry to vdp_update
// - PortC bit position 1 will flash during echoVDP, to show activity while the VDP isn't responding
#define DEBUG 0

#include "ez80f92.h"
#include <stdio.h>
#include <errno.h>
#include <ctype.h>
#include <mos_api.h>
#include "getsysvars.h"
#include "flash.h"
#include "agontimer.h"
#include "crc32.h"
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "filesize.h"

#define UNLOCKMATCHLENGTH 9
#define EXIT_FILENOTFOUND	4
#define EXIT_INVALIDPARAMETER	19
#define DEFAULT_MOSFIRMWARE	"MOS.bin"
#define DEFAULT_VDPFIRMWARE	"firmware.bin"

#define CMDUNKNOWN	0
#define CMDALL		1
#define CMDMOS		2
#define CMDVDP		3
#define CMDFORCE	4
#define CMDBATCH	5

int errno; // needed by standard library

bool		flashmos = false;
char		mosfilename[256];
FILE*       mosfilehandle;
uint32_t	moscrc;
bool		flashvdp = false;
char		vdpfilename[256];
FILE*       vdpfilehandle;
uint32_t	vdpcrc;
bool		optbatch = false;
bool		optforce = false;		// No y/n user input required

char        message[256];

// separate putch function that doesn't rely on a running MOS firmware
// UART0 initialization done by MOS firmware previously
// This utility doesn't run without MOS to load it anyway
int putch(int c) {
	while((IO(UART0_LSR) & 0x40) == 0);
	IO(UART0_THR) = c;
	return c;
}

void outstring(const char *str) {
    while(*str) {
        putch(*str);
        str++;
    }
}

void beep(unsigned int number) {
	while(number--) {
		putch(7);
		delayms(250);
	}
}

uint8_t getCharAt(uint16_t x, uint16_t y) {
	delayms(20);
	putch(23);
	putch(0);
	putch(131);
	putch(x & 0xFF);
	putch((x >> 8) & 0xFF);
	putch(y & 0xFF);
	putch((y >> 8) & 0xFF);
	delayms(100);
    return getsysvar_scrchar();
}

bool vdp_ota_present(void) {
	char test[UNLOCKMATCHLENGTH];
	uint16_t n;

	putch(23);
	putch(0);
	putch(0xA1);
	putch(0);
	outstring("unlock");

	for(n = 0; n < UNLOCKMATCHLENGTH+1; n++) test[n] = getCharAt(n+8, 3);
	// 3 - line on-screen
	if(memcmp(test, "unlocked!",UNLOCKMATCHLENGTH) == 0) return true;
	else return false;
}

uint8_t mos_magicnumbers[] = {0xF3, 0xED, 0x7D, 0x5B, 0xC3};
#define MOS_MAGICLENGTH 5
bool containsMosHeader(uint8_t *filestart) {
	uint8_t n;
	bool match = true;

	for(n = 0; n < MOS_MAGICLENGTH; n++) if(mos_magicnumbers[n] != filestart[n]) match = false;
	return match;
}

uint8_t esp32_magicnumbers[] = {0x32, 0x54, 0xCD, 0xAB};
#define ESP32_MAGICLENGTH 4
#define ESP32_MAGICSTART 0x20
bool containsESP32Header(uint8_t *filestart) {
	uint8_t n;
	bool match = true;

	filestart += ESP32_MAGICSTART; // start of ESP32 magic header
	for(n = 0; n < ESP32_MAGICLENGTH; n++) {
		if(esp32_magicnumbers[n] != filestart[n]) match = false;
	}
	return match;
}

void print_version(void) {
	outstring("Agon firmware update utility v1.9\n\r\n\r");
}

void usage(void) {
	print_version();
	outstring("Usage: FLASH [all | [mos <filename>] [vdp <filename>] | batch] <-f>\n\r");
}

bool getResponse(void) {
	uint8_t response = 0;

	outstring("Flash firmware (y/n)?");
	while((response != 'y') && (response != 'n')) response = tolower(getch());
	if(response == 'n') outstring("\r\nUser abort\n\r\n\r");
	else outstring("\r\n\r\n");
	return response == 'y';
}

void askEscapeToContinue(void) {
	uint8_t response = 0;

	outstring("Press ESC to continue");
	while(response != 0x1B) response = tolower(getch());
	outstring("\r\n");
}

bool update_vdp(void) {
	uint24_t filesize;

	putch(12); // cls
	print_version();	
	outstring("Unlocking VDP updater...\r\n");

	if(!vdp_ota_present()) {
		outstring(" failed - OTA not present in current VDP\r\n\r\n");
		outstring("Program the VDP using Arduino / PlatformIO / esptool\r\n\r\n");
		return false;
	}
	// Do actual work here
	outstring("Updating VDP firmware\r\n");
	filesize = getFileSize(vdpfilehandle->fhandle);	
	startVDPupdate(vdpfilehandle->fhandle, filesize);
    return true;
}

bool update_mos(char *filename) {
	uint32_t crcresult;
	uint24_t bytesread;
	char* ptr = (char*)BUFFER1;
	uint24_t counter, pagemax, lastpagebytes;
	uint24_t addressto,addressfrom;
	uint24_t filesize;
	int attempt;
	bool success = false;

	putch(12); // cls
	print_version();	
	
	outstring("Programming MOS firmware to ez80 flash...\r\n\r\n");
	outstring("Reading MOS firmware");
	filesize = getFileSize(mosfilehandle->fhandle);
	// Read file to memory
	crc32_initialize();
	while((bytesread = fread(ptr, 1, BLOCKSIZE, mosfilehandle)) > 0) {
		crc32(ptr, bytesread);
		ptr += bytesread;
		putch('.');
	}
	crcresult = crc32_finalize();
	outstring("\r\n");
	// Final memory check to given crc32
	if(crcresult != moscrc) {
		outstring("Error reading file to memory\r\n");
		return false;
	}
	outstring("\r\n");	
	// Actual work here	
    asm volatile("di"); // prohibit any access to the old MOS firmware
	attempt = 0;
	while((!success) && (attempt < 3)) {
		// start address in flash
		addressto = FLASHSTART;
		addressfrom = BUFFER1;
		// Write attempt#
		if(attempt > 0) {
            sprintf(message,"Retry attempt #%d\r\n", attempt);
            outstring(message);
        }
		// Unprotect and erase flash
		outstring("Erasing flash... ");

		enableFlashKeyRegister();	// unlock Flash Key Register, so we can write to the Flash Write/Erase protection registers
		IO(FLASH_PROT) = 0;				// disable protection on all 8x16KB blocks in the flash
		enableFlashKeyRegister();	// will need to unlock again after previous write to the flash protection register
		IO(FLASH_FDIV) = 0x5F;			// Ceiling(18Mhz * 5,1us) = 95, or 0x5F
	
		for(counter = 0; counter < FLASHPAGES; counter++) {
			IO(FLASH_PAGE) = counter;
			IO(FLASH_PGCTL) = 0x02;			// Page erase bit enable, start erase
            while(IO(FLASH_PGCTL) & 0x02);  // wait for completion of erase
		}
        outstring("\r\n");
				
		// determine number of pages to write
		pagemax = filesize/PAGESIZE;
		if(filesize%PAGESIZE) {// last page has less than PAGESIZE bytes 
			pagemax += 1;
			lastpagebytes = filesize%PAGESIZE;			
		}
		else lastpagebytes = PAGESIZE; // normal last page
		
		// write out each page to flash
		for(counter = 0; counter < pagemax; counter++) {
			sprintf(message,"\rWriting flash page %03d/%03d", counter+1, pagemax);
            outstring(message);

			if(counter == (pagemax - 1)) // last page to write - might need to write less than PAGESIZE
				fastmemcpy(addressto,addressfrom,lastpagebytes);				
			else 
				fastmemcpy(addressto,addressfrom,PAGESIZE);
		
			addressto += PAGESIZE;
			addressfrom += PAGESIZE;
		}
		// lock the flash before WARM reset
		enableFlashKeyRegister();	// unlock Flash Key Register, so we can write to the Flash Write/Erase protection registers
		IO(FLASH_PROT) = 0xff;			// enable protection on all 8x16KB blocks in the flash
		
		outstring("\r\nChecking CRC... ");

		crc32_initialize();
		crc32(FLASHSTART, filesize);
		crcresult = crc32_finalize();
		if(crcresult == moscrc) {
			outstring("OK\r\n");
			success = true;
		}
		else {
			outstring("ERROR\r\n");
		}
		attempt++;
	}
	outstring("\r\n");
	return success;
}

void echoVDP(uint8_t value) {
    // Disable flowcontrol
    putch(23);
    putch(0);
    putch(0xF9);
    putch(0x01);
    putch(0x01);
    // Request general Poll
	putch(23);
	putch(0);
	putch(0x80);
	putch(value);

    #if defined(DEBUG) && (DEBUG == 1)
        IO(PC_DR) = IO(PC_DR) | 0x02; // set bit position 1
        delayms(150);
        IO(PC_DR) = IO(PC_DR) & (0x01); // everything off, except bit 0
    #endif

    // Get screen dimensions
    putch(23);
    putch(0);
    putch(0x86);
    // Wait a while before sending the next echo
    delayms(150);
}

int getCommand(const char *command) {
	if(memcmp(command, "all\0", 4) == 0) return CMDALL;
	if(memcmp(command, "mos\0", 4) == 0) return CMDMOS;
	if(memcmp(command, "vdp\0", 4) == 0) return CMDVDP;
	if(memcmp(command, "batch\0", 6) == 0) return CMDBATCH;
	if(memcmp(command, "-f\0", 3) == 0) return CMDFORCE;
	if(memcmp(command, "force\0", 6) == 0) return CMDFORCE;
	if(memcmp(command, "-force\0", 7) == 0) return CMDFORCE;
	return CMDUNKNOWN;
}

bool parseCommands(int argc, char *argv[]) {
	int argcounter;
	int command;

	argcounter = 1;
	while(argcounter < argc) {
		command = getCommand(argv[argcounter]);
		switch(command) {
			case CMDUNKNOWN:
				return false;
				break;
			case CMDALL:
				if(flashmos || flashvdp) return false;
				strcpy(mosfilename, DEFAULT_MOSFIRMWARE);
				strcpy(vdpfilename, DEFAULT_VDPFIRMWARE);
				flashmos = true;
				flashvdp = true;
				break;
			case CMDMOS:
				if(flashmos) return false;
				if((argc > (argcounter+1)) && (getCommand(argv[argcounter + 1]) == CMDUNKNOWN)) {
					strcpy(mosfilename, argv[argcounter + 1]);
					argcounter++;
				}
				else {
					strcpy(mosfilename, DEFAULT_MOSFIRMWARE);
				}
				flashmos = true;
				break;
			case CMDVDP:
				if(flashvdp) return false;
				if((argc > (argcounter+1)) && (getCommand(argv[argcounter + 1]) == CMDUNKNOWN)) {
					strcpy(vdpfilename, argv[argcounter + 1]);
					argcounter++;
				}
				else {
					strcpy(vdpfilename, DEFAULT_VDPFIRMWARE);
				}
				flashvdp = true;
				break;
			case CMDBATCH:
				if(optbatch) return false;
				optbatch = true;
				optforce = true;
				strcpy(mosfilename, DEFAULT_MOSFIRMWARE);
				strcpy(vdpfilename, DEFAULT_VDPFIRMWARE);
				flashmos = true;
				flashvdp = true;
				break;
			case CMDFORCE:
				if(optforce && !optbatch) return false;
				optforce = true;
				break;
		}
		argcounter++;
	}
	return (flashvdp || flashmos);
}

bool openFiles(void) {
	bool filesexist = true;

    mosfilehandle = NULL;
    vdpfilehandle = NULL;
    
	if(flashmos) {
		mosfilehandle = fopen(mosfilename, "rb");
		if(!mosfilehandle) {
			sprintf(message,"Error opening MOS firmware \"%s\"\n\r",mosfilename);
            outstring(message);
			filesexist = false;
		}
	}
	if(flashvdp) {
		vdpfilehandle = fopen(vdpfilename, "rb");
		if(!vdpfilehandle) {
			sprintf(message,"Error opening VDP firmware \"%s\"\n\r",vdpfilename);
            outstring(message);
			filesexist = false;
            if(mosfilehandle) fclose(mosfilehandle);
		}
	}
	return filesexist;
}

bool validFirmwareFiles(void) {
	FILE* file;
	uint24_t filesize;
	uint8_t buffer[ESP32_MAGICLENGTH + ESP32_MAGICSTART];
	bool validfirmware = true;

	if(flashmos) {
        fseek(mosfilehandle, 0, SEEK_SET);
		fread((char *)BUFFER1, 1, MOS_MAGICLENGTH, mosfilehandle);
		if(!containsMosHeader((uint8_t *)BUFFER1)) {
			sprintf(message,"\"%s\" does not contain valid MOS ez80 startup code\r\n", mosfilename);
            outstring(message);
			validfirmware = false;
		}
		filesize = getFileSize(mosfilehandle->fhandle);
		if(filesize > FLASHSIZE) {
			sprintf(message,"\"%s\" too large for 128KB embedded flash\r\n", mosfilename);
            outstring(message);
			validfirmware = false;
		}
        fseek(mosfilehandle, 0, SEEK_SET);
	}
	if(flashvdp) {
        fseek(vdpfilehandle, 0, SEEK_SET);
		fread((char *)buffer, 1, ESP32_MAGICLENGTH + ESP32_MAGICSTART, vdpfilehandle);
		if(!containsESP32Header(buffer)) {
			sprintf(message,"\"%s\" does not contain valid ESP32 code\r\n", vdpfilename);
            outstring(message);
			validfirmware = false;
		}
        fseek(vdpfilehandle, 0, SEEK_SET);
	}
	return validfirmware;
}

void showCRC32(void) {
	if(flashmos) {sprintf(message,"MOS CRC 0x%08lX\r\n", moscrc); outstring(message);}
	if(flashvdp) {sprintf(message,"VDP CRC 0x%08lX\r\n", vdpcrc); outstring(message);}
	outstring("\r\n");
}

void calculateCRC32(void) {
	uint24_t bytesread;
	char* ptr;

	moscrc = 0;
	vdpcrc = 0;

	outstring("Calculating CRC");

	if(flashmos) {
        fseek(mosfilehandle, 0, SEEK_SET);
		ptr = (char*)BUFFER1;
		crc32_initialize();
		
		// Read file to memory
		while((bytesread = fread(ptr, 1, BLOCKSIZE, mosfilehandle)) > 0) {
			crc32(ptr, bytesread);
			ptr += bytesread;
			putch('.');
		}		
		moscrc = crc32_finalize();
        fseek(mosfilehandle, 0, SEEK_SET);
	}
	if(flashvdp) {
        fseek(vdpfilehandle, 0, SEEK_SET);
		crc32_initialize();
        while((bytesread = fread((char *)BUFFER1, 1, BLOCKSIZE, vdpfilehandle)) > 0) {
            crc32((char *)BUFFER1, bytesread);
            putch('.');
        }
		vdpcrc = crc32_finalize();
        fseek(vdpfilehandle, 0, SEEK_SET);
	}
	outstring("\r\n\r\n");
}

int main(int argc, char * argv[]) {	
    SYSVAR *sysvars = getsysvars();
	uint16_t tmp;

    // DEBUG PortC pin option
    #if defined(DEBUG) && (DEBUG == 1) // Set all PortC pins to output && to 0
        IO(PC_DDR) = 0;
        IO(PC_DR) = 0;
    #endif

	// All checks
	if(argc == 1) {
		usage();
		return 0;
	}
	if(!parseCommands(argc, argv)) {
		usage();
		return EXIT_INVALIDPARAMETER;
	}

	if(!openFiles()) return EXIT_FILENOTFOUND;
	if(!validFirmwareFiles()) {
		return EXIT_INVALIDPARAMETER;
	}

	putch(12);
	print_version();
	calculateCRC32();
	// Skip showing CRC32 and user input when 'silent' is requested
	if(!optforce) {
		putch(12);
		print_version();
		showCRC32();
		if(!getResponse()) return 0;
	}
	if(optbatch) beep(1);

	if(flashvdp) {
		while(sysvars->scrHeight == 0); // wait for 1st feedback from VDP
		tmp = sysvars->scrHeight;
		sysvars->scrHeight = 0;
        
        #if defined(DEBUG) && (DEBUG == 1) // Start update indicator, set PortC bit 0 to 1
            IO(PC_DR) = 1;
        #endif

		if(update_vdp()) {
			while(sysvars->scrHeight == 0) {
                echoVDP(1);
            };
			if(optbatch) beep(2);
		}
		else {
			if(!optforce && flashmos) {
				askEscapeToContinue();
				sysvars->scrHeight = tmp;
			}
		}
	    fclose(vdpfilehandle);

        #if defined(DEBUG) && (DEBUG == 1) // Stop update indicator (VDP is responsive), set PortC bit 0 to 0
            IO(PC_DR) = 0;
        #endif
	}

	if(flashmos) {
		if(update_mos(mosfilename)) {
			outstring("\r\nDone\r\n\r\n");
			if(optbatch) {
				outstring("Press reset button");
				beep(3);
				while(1); // don't repeatedly run this command batched (autoexec.txt)
			}
			else {
				outstring("System reset in ");
				for(int n = 3; n > 0; n--) {
					sprintf(message,"%d...", n);
                    outstring(message);
					delayms(1000);
				}
				reset();
			}
		}
		else {
			outstring("\r\nMultiple errors occured during flash write.\r\n");
			outstring("Bare-metal recovery required.\r\n");
			while(1); // No live MOS to return to
		}
	}
	return 0;
}

