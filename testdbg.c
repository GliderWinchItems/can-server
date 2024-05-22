/******************************************************************************
* File Name          : testdbg.c
* Date First Issued  : 05/18/2024
* Board              : Linux PC
* Description        : Test debug: test CANid-hex-bin routines
*******************************************************************************/
/*
gcc testdbg.c CANid-hex-bin.c -o testdbg && ./testdbg
*/
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <stdlib.h>

#include "CANid-hex-bin.h"


/* Line buffer size */
#define LINESZ 512	// Longest CAN msg line length
char buf[LINESZ];

char bout[32];

/* ************************************************************************************************************ */
int main(int argc, char **argv)
{
	uint32_t id;
	bout[0] = '.';
	bout[1] = '.';
	while ( (fgets (&buf[0],LINESZ,stdin)) != NULL)	// Get a line from stdin
	{
		if (strlen(buf) < 15)
		{
			printf("SHORT LINE: %i: %s",(int)strlen(buf),buf);
			continue;
		}

		id = CANid_hex_bin(buf);
		if (id == 0)
		{
			printf("hex input invalid: %s",buf);
			continue;
		}
		printf("%08X :%s",id, buf);

		CANid_bin_hex(bout, id);
		bout[10] = 0;
		printf("bout:     %s\n",bout);
	}
}
