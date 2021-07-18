/*******************************************************************************
* File Name          : extract-line.c
* Date First Issued  : 07/14/2021
* Board              : 
* Description        : Add incoming chars to buffer, build output line
*******************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "socketcand.h"

#define BUFBIGSZ (XBUFSZ+64)  // Coordinate buffering with socket read size
static char bufbig[BUFBIGSZ]; // Stream buffer

#define MAXOUTSZ 64
#define BUFOUTSZ MAXOUTSZ
static char bufout[BUFOUTSZ]; // Line under construction

static char *pb1 = &bufbig[0]; // Pointer next to be removed
static char *pb2 = &bufbig[0]; // Pointer next to be added
static char *po  = &bufout[0]; // Pointer next to be added

uint32_t maxctr = 0;
uint32_t ovrrunctr = 0;

/* **************************************************************************************
 * void extract_line_add(char *pin, int n);
 * @brief	: Add chars from 'read()' to big buffer and build output line
 * @param	: piin = pointer to input chars from 'read'()' (not used if n = 0)
 * @param	: n = number of chars in input
  * ************************************************************************************** */
void extract_line_add(char* pin, int n)
{
    char *ptmp;
    /* If no chars, then see if another line can be built from current buf big. */
    if (n > 0)
    { // Here, new chars to be added to big buffer
        while ((pb2 != &bufbig[BUFBIGSZ]) && (n > 0))
       {
            n -= 1;
            *pb2 = *pin++; // Copy char
            ptmp = pb2++;  // Save for overrun reset

            // pb2 wraparound check
            if (pb2 == &bufbig[BUFBIGSZ]) pb2 = &bufbig[0];

            // Overrun prevention
            if (pb2 == pb1)
            { // Here, next char will overrun big buffer
                pb2 = ptmp; // Reset pb2 to one less than pb1
                ovrrunctr += 1;
                break; // Screwed. Throw away input
            }
        }
    }
    return;
}
/* **************************************************************************************
 * char *extract_line_get(void);
 * @brief	: Attempt to extract a line from the buffer 
 * @return	:  NULL = no line available. Waiting for more chars (for a valid line)
 *			:  pointer to '\0' terminated string
 * Note: output line (if available) must be "consumed" before next call to this routine
 * Note: Input with no newline longer than MAXOUTSZ are discarded
 * ************************************************************************************** */
char *extract_line_get(void)
{
    // Here see if we can complete a line.
    while ((po < &bufout[BUFOUTSZ-1]) && (pb1 != pb2))
    {
        *po = *pb1++; // Copy buf big to output line

        // Check buf big pointer for wraparound
        if (pb1 == &bufbig[BUFBIGSZ])  pb1 = &bufbig[0];

        // Check if line is complete
        if (*po++ == '\n')
        { // Here, a line is complete
            *po = '\0';
            po = &bufout[0]; // Reset for next line
            return po;
        }

        // Don't overrun our output buffer.
        if (po == &bufout[BUFOUTSZ])
        { // Line is getting too long to be a valid CAN msg
            po = &bufout[0];
            maxctr += 1;
        }
    }
    return NULL;
}
/* **************************************************************************************
 * void extract_line_printerr(int ret);
 * @brief	: printf for return value of above code
 * ************************************************************************************** */
void extract_line_printerr(int ret)
{
    if (ret >= 0) return;
				switch(ret){
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
			default: printf("ERROR: can_os: error not classified: %d\n",ret);
				break;
				}
    return;
}