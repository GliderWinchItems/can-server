/*******************************************************************************
* File Name          : can-os.c
* Date First Issued  : 06/27/2021
* Board              : Seeed CAN hat
* Description        : Convert our Original format (ascii-hex) to Can Socket frame
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "linux/can.h"
#include "socketcand.h"

#include "common_can.h"
#include "can-os.h"
#include "can-so.h"

/* Lookup table to convert one hex char to binary (4 bits), no checking for illegal incoming hex */
#define E 255	// Error flag: non-hex char
const uint8_t hxbn[256] = {
/*          0   1   2   3   4   5   6   7   8   9  10  11  12  13  14  15   */
/*  0  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  1  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  2  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  3  */   0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  E,  E,  E,  E,  E,  E,
/*  4  */   E, 10, 11, 12, 13, 14, 15,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  5  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  6  */   0, 10, 11, 12, 13, 14, 15,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  7  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  8  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/*  9  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 10  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 11  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 12  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 13  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 14  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
/* 15  */   E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,  E,
};
 
//uint8_t unhex(char* p)
//{
//	uint8_t x = hxbn[(uint8_t)(*p++)] << 4;// Get hi nibble of byte
//	x |= hxbn[(uint8_t)(*p)];		// Or in low nibble
//	return x;
//}

/* **************************************************************************************
 * int can_os_cnvt(struct can_frame *pframe,struct CANALL *pall, char* p);
 * @brief	: Convert binary CAN msg in can socket to legacy format
 * @param	: pframe = points to can socket frame (see can.h)
 * @param	: pall = points to various forms of CAN msg (see can-so.h)
 * @param	: p = points to string with incoming ascii-hex
 * @return	:  0 = OK; 
 *			: -1 = Input string too long (>31)
 *			: -2 = Input string too short (<15)
 *			: -3 = Illegal hex char in input string
 *			: -4 = Illegal CAN id: 29b low ord bits present with 11b IDE flag off
 			: -5 = Illegal DLC: (low four bits greater than 8) [Time Stamping ignored]
 *			: -6 = Checksum error
 * ************************************************************************************** */
int can_os_cnvt(struct can_frame *pframe,struct CANALL *pall, char* p)
{
	uint32_t x = CHECKSUM_INITIAL; // (0xa5a5. See common_can.h)
	uint8_t *pend;
	uint8_t *pb; // Binary array, working ptr 
	uint8_t  w;  // hi-ord nibble
	uint8_t  y;  // lo-ord nibble

	int len = strlen(p);
	if (len > 31) return -1; // Too long
	if (len < 15) return -2; // Too short

	/* Convert incoming ascii to binary */
	pb = &pall->cba[0];    // Binary array, working ptr 
	pend = pb + (len-1)/2; // Ignore '\n' (odd) at end
	while (pb < pend){
		/* Look up to convert ascii char to binary nibble, and check for error. */
		if (((w=hxbn[(*(uint8_t*)p++)]) != E) && (y=hxbn[*(uint8_t*)(p++)]) != E){
			*pb = (w << 4) | y;	// Combine nibbles
			x +=  *pb++; // Build checksum, advance binary array ptr
		}
		else{
			return -3; // E = Illegal hex char (not E = excellence)
		}
	}

	/* Reposition binary bytes into Our Format */
	pb = &pall->cba[0];
	pall->seq = *pb;	// Save sequency number

	/* CAN id */
	pall->can.id = (
		(*(pb+1) <<  0) | 
		(*(pb+2) <<  8) |
		(*(pb+3) << 16) | 
		(*(pb+4) << 24) );

	// Illegal CAN id check: 11b addresses should not have 29b low order bits
	if (((pall->can.id & 0x0001FFFCU) != 0) && ((pall->can.id & 0x4) == 0)){
		return -4; // Programmer of the msg failed miserbly
	}

	/* DLC-data length */
	pall->can.dlc = *(pb+5) & 0xF; // Ignore time stamping
	if (pall->can.dlc > 8){
		return -5; // DLC too big
	}

	/* Copy binary array payload into Our traditional struct payload union. */
	for (int m = 0; m < pall->can.dlc; m++) pall->can.cd.uc[m] = *(pb+6+m);

	// Add DLC to CAN frame (since it already in the registers(?))
	pframe->can_dlc = pall->can.dlc;

	/* Checksum check */
	x -= *(pb + 6 + pall->can.dlc);

	// Complete checksum computation
    x += (x >> 16); // Add carries into high half word
    x += (x >> 16); // Add carry if previous add generated a carry
    x += (x >> 8);  // Add high byte of low half word
    x += (x >> 8);  // Add carry if previous add generated a carry

	if (*(pb + 6 + pall->can.dlc) != (uint8_t)x){
		return -6; // Checksum error
	}

	/* Populate CAN socket frame with their inefficient format. */
	// CAN id
	w = ((pall->can.id & 0x4) << 28); // Extended frame bit
	y = ((pall->can.id & 0x2) << 28); // RTR bit
	if (w != 0){
		pframe->can_id = (pall->can.id >> 3) | y | w;// 29b
	}
	else{
		pframe->can_id = (pall->can.id >> 21) | y;// 11b
	}

	// Payload (copy all possibilites in one gulp)
	*(uint64_t*)&pframe->data[0] = pall->can.cd.ull;

	return 0; // All Hail! Victory is ours!
}

/* **************************************************************************************
 * void can_os_printerr(int ret);
 * @brief	: printf for return value of above code
 * ************************************************************************************** */
void can_os_printerr(int ret)
{
    if (ret >= 0) return;
				switch(ret)
				{					
	case  -1: PRINT_ERROR("Error: can_os: Input string too long (>31)\n");
		break;
	case  -2: PRINT_ERROR("Error: can_os: Input string too short (<15)\n");
		break;
	case  -3: PRINT_ERROR("Error: can_os: Illegal hex char in input string");
		break;
	case  -4: PRINT_ERROR("Error: can_os: Illegal CAN id: 29b low ord bits present with 11b IDE flag off\n");
		break;
	case  -5: PRINT_ERROR("Error: can_os: Illegal DLC\n");
		break;
	case  -6: PRINT_ERROR("Error: can_os: Checksum error\n");
		break;
	default:  PRINT_ERROR("Error: can_os: error not classified\n");
		break;
				}
    return;
}