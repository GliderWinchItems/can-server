/*******************************************************************************
* File Name          : output.h
* Date First Issued  : 06/21/2024
* Board              : Seeed CAN hat
* Description        : Output buffering and output threads 
*******************************************************************************/
#ifndef __OUTPUT
#define __OUTPUT

#include <stdio.h>
#include "can.h"

#define LINEBUFFSIZE 320 // Lines for 2048 flash block, plus some
#define LBUFSZ 34 // Length of longest ascii/hex CAN msg+1

struct LBUF
{
	char buf[LBUFSZ]; // One CAN msg line
	uint8_t len;	// Length including '\n'
};
struct LINEBUF
{
	struct LBUFF lbuf[LINEBUFFSIZE];
	struct LBUFF* padd;
	struct LBUFF* ptake;
	struct LBUFF* pend;
	sem_t sem;
	int tret;
	int socket;
};

#define FRAMEBUFFSIZE 320
struct FRAMEBUFF
{
	struct frame fbuf[FRAMEBUFFSIZE];
	struct frame* padd;
	struct frame* ptake;
	struct frame* pend;
	sem_t sem;
	int tret;
	int socket;
};

/* **************************************************************************************/
 int output_init_tcp(int socket);
/* @brief   : Initialize output threads and semaphores
 * @param   : socket = network socket
 * @return	:  0 = OK; 
 * ************************************************************************************** */
 int output_init_can(int socket);
/* @brief   : Initialize output threads and semaphores
 * @param   : socket = CAN socket
 * @return	:  0 = OK; 
 * ************************************************************************************** */
 int output_add_lines(char* pc, int n);
/* @brief   : Add a CAN msg line to the output buffer (limited to 33 chars)
 * @param   : pc = pointer to input that ends with a '\n'
 * @param   : n = number of chars to transfer (n = 15 - 33)
 * @return	:  0 = OK; 
 * ************************************************************************************** */
 int output_add_frames(struct* FRAMEBUFF* pfr);
/* @brief   : Add CAN frame to the output buffer of frames
 * @param   : pfr = pointer to input frame
 * @return	:  0 = OK; 
 * ************************************************************************************** */
 void output_thread_lines(struct LINEBUFF* plb;);
/* @brief   : Output buffered lines to TCP socket
 * @param   : plb = pointer to struct with buffer and pointers
 * ************************************************************************************** */
 void output_thread_frames(struct FRAMEBUFF* plf);
/* @brief   : Output buffered frames to CAN socket
 * @param   : plf = pointer to struct with buffer and pointers
 * ************************************************************************************** */

#endif