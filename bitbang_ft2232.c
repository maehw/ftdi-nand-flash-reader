/* 
 * This file is part of the ftdi-nand-flash-reader distribution (https://github.com/maehw/ftdi-nand-flash-reader).
 * Copyright (c) 2018 maehw.
 * 
 * This program is free software: you can redistribute it and/or modify  
 * it under the terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 *
 * This program is distributed in the hope that it will be useful, but 
 * WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 *
 * \file bitbang_ft2232.c
 * \brief NAND flash reader based on FTDI FT2232 IC in bit-bang IO mode
 * Interfacing NAND flash devices with an x8 I/O interface for address and data.
 * Additionally the signals Chip Enable (nCE), Write Enable (nWE), Read Enable (nRE), 
 * Address Latch Enable (ALE), Command Latch Enable (CLE), Write Protect (nWP) 
 * and Ready/Busy (RDY) on the control bus are used.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ftdi.h>

/* FTDI FT2232H VID and PID */
#define FT2232H_VID 0x0403
#define FT2232H_PID 0x6010

/* Pins on ADBUS0..7 (I/O bus) */
#define PIN_DIO0 0x01
#define PIN_DIO1 0x02
#define PIN_DIO2 0x04
#define PIN_DIO3 0x08
#define PIN_DIO4 0x10
#define PIN_DIO5 0x20
#define PIN_DIO6 0x40
#define PIN_DIO7 0x80
#define IOBUS_BITMASK_WRITE 0xFF
#define IOBUS_BITMASK_READ  0x00

/* Pins on BDBUS0..7 (control bus) */
#define PIN_CLE  0x01
#define PIN_ALE  0x02
#define PIN_nCE  0x04
#define PIN_nWE  0x08
#define PIN_nRE  0x10
#define PIN_nWP  0x20
#define PIN_RDY  0x40 /* READY / nBUSY output signal */
#define PIN_LED  0x80
#define CONTROLBUS_BITMASK 0xBF /* 0b1011 1111 = 0xBF */

#define STATUSREG_IO0  0x01

#define REALWORLD_DELAY 10 /* 10 usec */

const unsigned char CMD_READID = 0x90; /* read ID register */
const unsigned char CMD_READ1[2] = { 0x00, 0x30 }; /* page read */
const unsigned char CMD_BLOCKERASE[2] = { 0x60, 0xD0 }; /* block erase */
const unsigned char CMD_READSTATUS = 0x70; /* read status */

typedef enum { OFF=0, ON=1 } onoff_t;
typedef enum { IOBUS_IN=0, IOBUS_OUT=1 } iobus_inout_t;

unsigned char iobus_value;
unsigned char controlbus_value;

struct ftdi_context *nandflash_iobus, *nandflash_controlbus;

void controlbus_reset_value()
{
    controlbus_value = 0x00;
}

void controlbus_pin_set(unsigned char pin, onoff_t val)
{
    if(val == ON)
        controlbus_value |= pin;
    else
        controlbus_value &= (unsigned char)0xFF ^ pin;
}

void controlbus_update_output()
{
    unsigned char buf[1]; /* buffer for FTDI function needs to be an array */
    buf[0] = controlbus_value;
    ftdi_write_data(nandflash_controlbus, buf, 1);
}

void test_controlbus()
{
    #define CONTROLBUS_TEST_DELAY 1000000 /* 1 sec */

    printf("  CLE on\n");
    controlbus_pin_set(PIN_CLE, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  ALE on\n");
    controlbus_pin_set(PIN_ALE, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nCE on\n");
    controlbus_pin_set(PIN_nCE, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nWE on\n");
    controlbus_pin_set(PIN_nWE, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);


    printf("  nRE on\n");
    controlbus_pin_set(PIN_nRE, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nWP on\n");
    controlbus_pin_set(PIN_nWP, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  LED on\n");
    controlbus_pin_set(PIN_LED, ON);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);


    printf("  CLE off\n");
    controlbus_pin_set(PIN_CLE, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  ALE off\n");
    controlbus_pin_set(PIN_ALE, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nCE off\n");
    controlbus_pin_set(PIN_nCE, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nWE off\n");
    controlbus_pin_set(PIN_nWE, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nRE off\n");
    controlbus_pin_set(PIN_nRE, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  nWP off\n");
    controlbus_pin_set(PIN_nWP, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);

    printf("  LED off\n");
    controlbus_pin_set(PIN_LED, OFF);
    controlbus_update_output(nandflash_controlbus);
    usleep(CONTROLBUS_TEST_DELAY);
}

void iobus_set_direction(iobus_inout_t inout)
{
    if( inout == IOBUS_OUT )
        ftdi_set_bitmode(nandflash_iobus, IOBUS_BITMASK_WRITE, BITMODE_BITBANG);
    else if( inout == IOBUS_IN )
        ftdi_set_bitmode(nandflash_iobus, IOBUS_BITMASK_READ, BITMODE_BITBANG);        
}

void iobus_reset_value()
{
    iobus_value = 0x00;
}

void iobus_pin_set(unsigned char pin, onoff_t val)
{
    if(val == ON)
        iobus_value |= pin;
    else
        iobus_value &= (unsigned char)0xFF ^ pin;
}

void iobus_set_value(unsigned char value)
{
    iobus_value = value;
}

void iobus_update_output()
{
    unsigned char buf[1]; /* buffer for FTDI function needs to be an array */
    buf[0] = iobus_value;
    ftdi_write_data(nandflash_iobus, buf, 1);
}

unsigned char iobus_read_input()
{
    unsigned char buf; 
    //ftdi_read_data(nandflash_iobus, buf, 1); /* buffer for FTDI function needed to be an array */
    ftdi_read_pins(nandflash_iobus, &buf);
    return buf;
}

unsigned char controlbus_read_input()
{
    unsigned char buf;
    //ftdi_read_data(nandflash_controlbus, buf, 1); /* buffer for FTDI function needed to be an array */
    ftdi_read_pins(nandflash_controlbus, &buf);
    return buf;
}


void test_iobus()
{
    #define IOBUS_TEST_DELAY 1000000 /* 1 sec */

    printf("  DIO0 on\n");
    iobus_pin_set(PIN_DIO0, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO1 on\n");
    iobus_pin_set(PIN_DIO1, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO2 on\n");
    iobus_pin_set(PIN_DIO2, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO3 on\n");
    iobus_pin_set(PIN_DIO3, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO4 on\n");
    iobus_pin_set(PIN_DIO4, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO5 on\n");
    iobus_pin_set(PIN_DIO5, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO6 on\n");
    iobus_pin_set(PIN_DIO6, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    printf("  DIO7 on\n");
    iobus_pin_set(PIN_DIO7, ON);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);


    iobus_pin_set(PIN_DIO0, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO1, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO2, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO3, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO4, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO5, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO6, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    iobus_pin_set(PIN_DIO7, OFF);
    iobus_update_output(nandflash_iobus);
    usleep(IOBUS_TEST_DELAY);

    usleep(5 * IOBUS_TEST_DELAY);
    iobus_set_value(0xFF);
    iobus_update_output();
    usleep(5 * IOBUS_TEST_DELAY);
    iobus_set_value(0xAA);
    iobus_update_output();
    usleep(5 * IOBUS_TEST_DELAY);
    iobus_set_value(0x55);
    iobus_update_output();
    usleep(5 * IOBUS_TEST_DELAY);
    iobus_set_value(0x00);
    iobus_update_output();

    
    iobus_pin_set(PIN_DIO0, ON);
    iobus_pin_set(PIN_DIO2, ON);
    iobus_pin_set(PIN_DIO4, ON);
    iobus_pin_set(PIN_DIO6, ON);
    iobus_update_output(nandflash_iobus);
    usleep(2* 100000);

}

/* "Command Input bus operation is used to give a command to the memory device. Command are accepted with Chip
Enable low, Command Latch Enable High, Address Latch Enable low and Read Enable High and latched on the rising
edge of Write Enable. Moreover for commands that starts a modify operation (write/erase) the Write Protect pin must be
high."" */
int latch_command(unsigned char command)
{
    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE )
    {
        fprintf(stderr, "latch_command requires nCE pin to be low\n");
        return EXIT_FAILURE;
    }
    else if( ~controlbus_value & PIN_nRE )
    {
        fprintf(stderr, "latch_command requires nRE pin to be high\n");
        return EXIT_FAILURE;
    }

    /* debug info */
    printf("latch_command(0x%02X)\n", command);

    /* toggle CLE high (activates the latching of the IO inputs inside the 
     * Command Register on the Rising edge of nWE) */
    printf("  setting CLE high\n");
    controlbus_pin_set(PIN_CLE, ON);
    controlbus_update_output();

    // toggle nWE low
    printf("  setting nWE low\n");
    controlbus_pin_set(PIN_nWE, OFF);
    controlbus_update_output();

    // change I/O pins
    printf("  setting I/O bus to command\n");
    iobus_set_value(command);
    iobus_update_output();

    // toggle nWE back high (acts as clock to latch the command!)
    printf("  setting nWE high\n");
    controlbus_pin_set(PIN_nWE, ON);
    controlbus_update_output();

    // toggle CLE low
    printf("  setting CLE low\n");
    controlbus_pin_set(PIN_CLE, OFF);
    controlbus_update_output();

    return 0;
}

/** 
 * "Address Input bus operation allows the insertion of the memory address. 
 * Five cycles are required to input the addresses for the 4Gbit devices. 
 * Addresses are accepted with Chip Enable low, Address Latch Enable High, Command Latch Enable low and 
 * Read Enable High and latched on the rising edge of Write Enable.
 * Moreover for commands that starts a modifying operation (write/erase) the Write Protect pin must be high. 
 * See Figure 5 and Table 13 for details of the timings requirements.
 * Addresses are always applied on IO7:0 regardless of the bus configuration (x8 or x16).""
 */
int latch_address(unsigned char address[], unsigned int addr_length)
{
    unsigned int addr_idx = 0;

    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE )
    {
        fprintf(stderr, "latch_address requires nCE pin to be low\n");
        return EXIT_FAILURE;
    }
    else if( controlbus_value & PIN_CLE )
    {
        fprintf(stderr, "latch_address requires CLE pin to be low\n");
        return EXIT_FAILURE;
    }
    else if( ~controlbus_value & PIN_nRE )
    {
        fprintf(stderr, "latch_address requires nRE pin to be high\n");
        return EXIT_FAILURE;
    }

    /* toggle ALE high (activates the latching of the IO inputs inside
     * the Address Register on the Rising edge of nWE. */
    controlbus_pin_set(PIN_ALE, ON);
    controlbus_update_output();

    for(addr_idx = 0; addr_idx < addr_length; addr_idx++)
    {
        // toggle nWE low
        controlbus_pin_set(PIN_nWE, OFF);
        controlbus_update_output();
        usleep(REALWORLD_DELAY);

        // change I/O pins
        iobus_set_value(address[addr_idx]);
        iobus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure setup delay */

        // toggle nWE back high (acts as clock to latch the current address byte!)
        controlbus_pin_set(PIN_nWE, ON);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure hold delay */
    }

    // toggle ALE low
    controlbus_pin_set(PIN_ALE, OFF);
    controlbus_update_output();

    // wait for ALE to nRE Delay tAR before nRE is taken low (nanoseconds!)

    return 0;
}

/* Data Output bus operation allows to read data from the memory array and to 
 * check the status register content, the EDC register content and the ID data.
 * Data can be serially shifted out by toggling the Read Enable pin with Chip 
 * Enable low, Write Enable High, Address Latch Enable low, and Command Latch 
 * Enable low. */
int latch_register(unsigned char reg[], unsigned int reg_length)
{
    unsigned int addr_idx = 0;

    /* check if ALE is low and nRE is high */
    if( controlbus_value & PIN_nCE )
    {
        fprintf(stderr, "latch_address requires nCE pin to be low\n");
        return EXIT_FAILURE;
    }
    else if( ~controlbus_value & PIN_nWE )
    {
        fprintf(stderr, "latch_address requires nWE pin to be high\n");
        return EXIT_FAILURE;
    }
    else if( controlbus_value & PIN_ALE )
    {
        fprintf(stderr, "latch_address requires ALE pin to be low\n");
        return EXIT_FAILURE;
    }

    iobus_set_direction(IOBUS_IN);

    for(addr_idx = 0; addr_idx < reg_length; addr_idx++)
    {
        /* toggle nRE low; acts like a clock to latch out the data;
         * data is valid tREA after the falling edge of nRE 
         * (also increments the internal column address counter by one) */
        controlbus_pin_set(PIN_nRE, OFF);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREA delay */

        // read I/O pins
        reg[addr_idx] = iobus_read_input();

        // toggle nRE back high
        controlbus_pin_set(PIN_nRE, ON);
        controlbus_update_output();
        usleep(REALWORLD_DELAY); /* TODO: assure tREH and tRHZ delays */
    }

    iobus_set_direction(IOBUS_OUT);

    return 0;
}

void check_ID_register(unsigned char* ID_register)
{
    unsigned char ID_register_exp[5] = { 0xAD, 0xDC, 0x10, 0x95, 0x54 };

    /* output the retrieved ID register content */
    printf("actual ID register:   0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        ID_register[0], ID_register[1], ID_register[2],
        ID_register[3], ID_register[4] ); 

    /* output the expected ID register content */
    printf("expected ID register: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
        ID_register_exp[0], ID_register_exp[1], ID_register_exp[2],
        ID_register_exp[3], ID_register_exp[4] ); 

    if( strncmp( (char *)ID_register_exp, (char *)ID_register, 5 ) == 0 )
    {
        printf("PASS: ID register did match\n");
    }
    else
    {
        printf("FAIL: ID register did not match\n");
    }
}

/* Address Cycle Map calculations */
void get_address_cycle_map_x8(uint32_t mem_address, unsigned char* addr_cylces)
{
    addr_cylces[0] = (unsigned char)(  mem_address & 0x000000FF);
    addr_cylces[1] = (unsigned char)( (mem_address & 0x00000F00) >> 8 );
    addr_cylces[2] = (unsigned char)( (mem_address & 0x000FF000) >> 12 );
    addr_cylces[3] = (unsigned char)( (mem_address & 0x0FF00000) >> 20 );
    addr_cylces[4] = (unsigned char)( (mem_address & 0x30000000) >> 28 );
}

void dump_memory(void)
{
    FILE *fp;
    unsigned int page_idx;
    unsigned int page_idx_max;
    unsigned char addr_cylces[5];
    uint32_t mem_address;
    unsigned char mem_large_block[2112]; /* page content */
    //    unsigned int byte_offset;
    //    unsigned int line_no;

    printf("Trying to open file for storing the binary dump...\n");
    /* Opens a text file for both reading and writing. It first truncates the file to zero length
     * if it exists, otherwise creates a file if it does not exist. */
    fp = fopen("flashdump.bin", "w+");

    if( fp == NULL )
        printf("  Error when opening the file...\n");
    else
        printf("  File opened successfully...\n");

    // Start reading the data
    mem_address = 0x00000000; // start address
    page_idx_max = 64 * 4096;
    for( page_idx = 0; page_idx < page_idx_max; /* blocks per page * overall blocks */ page_idx++)
    {

      printf("Reading data from page %d / %d (%.2f %%)\n", page_idx, page_idx_max, (float)page_idx/(float)page_idx_max * 100 );
      {
          printf("Reading data from memory address 0x%02X\n", mem_address);
          get_address_cycle_map_x8(mem_address, addr_cylces);
          printf("  Address cycles are: 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
              addr_cylces[0], addr_cylces[1], /* column address */
              addr_cylces[2], addr_cylces[3], addr_cylces[4] ); /* row address */

          printf("Latching first command byte to read a page...\n");
          latch_command(CMD_READ1[0]);

          printf("Latching address cycles...\n");
          latch_address(addr_cylces, 5);

          printf("Latching second command byte to read a page...\n");
          latch_command(CMD_READ1[1]);

          // busy-wait for high level at the busy line
          printf("Checking for busy line...\n");
          unsigned char controlbus_val;
          do
          {
              controlbus_val = controlbus_read_input();
          }
          while( !(controlbus_val & PIN_RDY) );

          printf("  done\n");

          printf("Latching out data block...\n");
          latch_register(mem_large_block, 2112);
      }

//      // Dumping memory to console and file
//      // 2112 bytes = 132 lines * 16 bytes/line
//      for( line_no = 0; line_no < 132; line_no++ )
//      {
//          byte_offset = line_no * 16;
//
//          // Dumping memory to console
//          // printf("%02X %02X %02X %02X %02X %02X %02X %02X  %02X %02X %02X %02X %02X %02X %02X %02X | ",
//          //     mem_large_block[byte_offset+0], mem_large_block[byte_offset+1],
//          //     mem_large_block[byte_offset+2], mem_large_block[byte_offset+3],
//          //     mem_large_block[byte_offset+4], mem_large_block[byte_offset+5],
//          //     mem_large_block[byte_offset+6], mem_large_block[byte_offset+7],
//          //     mem_large_block[byte_offset+8], mem_large_block[byte_offset+9],
//          //     mem_large_block[byte_offset+10], mem_large_block[byte_offset+11],
//          //     mem_large_block[byte_offset+12], mem_large_block[byte_offset+13],
//          //     mem_large_block[byte_offset+14], mem_large_block[byte_offset+15] );
//
//          // for( k = 0; k < 16; k++ )
//          // {
//          //   c = mem_large_block[byte_offset+k];
//          //   if( c >= 0x20u && c <= 0x7Fu)
//          //     printf("%c", c);
//          //   else
//          //     printf("_");
//          // }
//          // printf("\n");
//      }

      // Dumping memory to file
      //printf("Dumping binary data to file...\n");
      fwrite(&mem_large_block[0], 1, 2112, fp);

      mem_address += 2112; // add bytes per block (i.e. goto next block)
    }

    // Finished reading the data
    printf("Closing binary dump file...\n");

    fclose(fp);
}

/**
 * BlockErase
 *
 * "The Erase operation is done on a block basis.
 * Block address loading is accomplished in there cycles initiated by an Erase Setup command (60h).
 * Only address A18 to A29 is valid while A12 to A17 is ignored (x8).
 *
 * The Erase Confirm command (D0h) following the block address loading initiates the internal erasing process.
 * This two step sequence of setup followed by execution command ensures that memory contents are not
 * accidentally erased due to external noise conditions.
 *
 * At the rising edge of WE after the erase confirm command input,
 * the internal write controller handles erase and erase verify.
 *
 * Once the erase process starts, the Read Status Register command may be entered to read the status register.
 * The system controller can detect the completion of an erase by monitoring the R/B output,
 * or the Status bit (I/O 6) of the Status Register.
 * Only the Read Status command and Reset command are valid while erasing is in progress.
 * When the erase operation is completed, the Write Status Bit (I/O 0) may be checked."
 */
int erase_block(unsigned int nBlockId)
{
	uint32_t mem_address;
	unsigned char addr_cylces[5];

	/* calculate memory address */
	mem_address = 2048 * 64 * nBlockId; // (2K + 64) bytes x 64 pages per block

	/* remove write protection */
	controlbus_pin_set(PIN_nWP, ON);

	printf("Latching first command byte to erase a block...\n");
	latch_command(CMD_BLOCKERASE[0]); /* block erase setup command */

	printf("Erasing block of data from memory address 0x%02X\n", mem_address);
	get_address_cycle_map_x8(mem_address, addr_cylces);
	printf("  Address cycles are (but: will take only cycles 3..5) : 0x%02X 0x%02X 0x%02X 0x%02X 0x%02X\n",
		addr_cylces[0], addr_cylces[1], /* column address */
		addr_cylces[2], addr_cylces[3], addr_cylces[4] ); /* row address */

	printf("Latching page(row) address (3 bytes)...\n");
	unsigned char address[] = { addr_cylces[2], addr_cylces[3], addr_cylces[4] };
	latch_address(address, 3);

	printf("Latching second command byte to erase a block...\n");
	latch_command(CMD_BLOCKERASE[1]);

	/* tWB: WE High to Busy is 100 ns -> ignore it here as it takes some time for the next command to execute */

	// busy-wait for high level at the busy line
	printf("Checking for busy line...\n");
	unsigned char controlbus_val;
	do
	{
		controlbus_val = controlbus_read_input();
	}
	while( !(controlbus_val & PIN_RDY) );

	printf("  done\n");


	/* Read status */

	printf("Latching command byte to read status...\n");
	latch_command(CMD_READSTATUS);

	unsigned char status_register;
	latch_register(&status_register, 1); /* data output operation */

	/* output the retrieved status register content */
	printf("Status register content:   0x%02X\n", status_register);


	/* activate write protection again */
	controlbus_pin_set(PIN_nWP, OFF);


	if(status_register & STATUSREG_IO0)
	{
		fprintf(stderr, "Failed to erase block.\n");
		return 1;
	}
	else
	{
		printf("Successfully erased block.\n");
		return 0;
	}
}

/**
 * Page Program
 *
 */
void program_page(void)
{
}


int main(int argc, char **argv)
{
    struct ftdi_version_info version;
    unsigned char ID_register[5];
    int f;

    // show library version
    version = ftdi_get_library_version();
    printf("Initialized libftdi %s (major: %d, minor: %d, micro: %d,"
        " snapshot ver: %s)\n", version.version_str, version.major,
        version.minor, version.micro, version.snapshot_str);

    // Init 1. channel for databus
    if ((nandflash_iobus = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed for iobus\n");
        return EXIT_FAILURE;
    }

    ftdi_set_interface(nandflash_iobus, INTERFACE_A);
    f = ftdi_usb_open(nandflash_iobus, FT2232H_VID, FT2232H_PID);
    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f,
          ftdi_get_error_string(nandflash_iobus));
        ftdi_free(nandflash_iobus);
        exit(-1);
    }
    printf("ftdi open succeeded(channel 1): %d\n", f);

    printf("enabling bitbang mode(channel 1)\n");
    ftdi_set_bitmode(nandflash_iobus, IOBUS_BITMASK_WRITE, BITMODE_BITBANG);

    // Init 2. channel
    if ((nandflash_controlbus = ftdi_new()) == 0)
    {
        fprintf(stderr, "ftdi_new failed\n");
        return EXIT_FAILURE;
    }
    ftdi_set_interface(nandflash_controlbus, INTERFACE_B);
    f = ftdi_usb_open(nandflash_controlbus, FT2232H_VID, FT2232H_PID);
    if (f < 0 && f != -5)
    {
        fprintf(stderr, "unable to open ftdi device: %d (%s)\n", f,
          ftdi_get_error_string(nandflash_controlbus));
        ftdi_free(nandflash_controlbus);
        exit(-1);
    }
    printf("ftdi open succeeded(channel 2): %d\n",f);

    printf("enabling bitbang mode (channel 2)\n");
    ftdi_set_bitmode(nandflash_controlbus, CONTROLBUS_BITMASK, BITMODE_BITBANG);

    usleep(2* 1000000);

    controlbus_reset_value();
    controlbus_update_output();

    iobus_set_direction(IOBUS_OUT);
    iobus_reset_value();
    iobus_update_output();

    /*printf("testing control bus, check visually...\n");
    usleep(2* 1000000);
    test_controlbus();

    printf("testing I/O bus for output, check visually...\n");
    usleep(2* 1000000);
    test_iobus();*/

    printf("testing I/O and control bus for input read...\n");
    iobus_set_direction(IOBUS_IN);
    //while(1)
    {
        unsigned char iobus_val = iobus_read_input();
        unsigned char controlbus_val = controlbus_read_input();
        printf("data read back: iobus=0x%02x, controlbus=0x%02x\n",
                iobus_val, controlbus_val);
        usleep(1* 1000000);
    }
    iobus_set_direction(IOBUS_OUT);

    // set nRE high and nCE and nWP low
    controlbus_pin_set(PIN_nRE, ON);
    controlbus_pin_set(PIN_nCE, OFF);
    controlbus_pin_set(PIN_nWP, OFF); /* nWP low provides HW protection against undesired modify (program / erase) operations */
    controlbus_update_output();

    // Read the ID register
    {
        printf("Trying to read the ID register...\n");

        latch_command(CMD_READID); /* command input operation; command: READ ID */

        //unsigned char address[] = { 0x11, 0x22, 0x44, 0x88, 0xA5 };
        //latch_address(address, 5);
        unsigned char address[] = { 0x00 };
        latch_address(address, 1); /* address input operation */

        latch_register(ID_register, 5); /* data output operation */

        check_ID_register(ID_register);
    }

    dump_memory();

//	/* Erase all blocks */
//    for( unsigned int nBlockId = 0; nBlockId < 4096; nBlockId++ )
//    {
//    	erase_block(nBlockId);
//    }

//    program_page();


    // set nCE high
    controlbus_pin_set(PIN_nCE, ON);


    printf("done, 10 sec to go...\n");
    usleep(10* 1000000);

    printf("disabling bitbang mode(channel 1)\n");
    ftdi_disable_bitbang(nandflash_iobus);
    ftdi_usb_close(nandflash_iobus);
    ftdi_free(nandflash_iobus);

    printf("disabling bitbang mode(channel 2)\n");
    ftdi_disable_bitbang(nandflash_controlbus);
    ftdi_usb_close(nandflash_controlbus);
    ftdi_free(nandflash_controlbus);

    return 0;
}
