/*******************************************************************************
* File Name          : output.h
* Date First Issued  : 06/21/2024
* Board              : Seeed CAN hat
* Description        : Output buffering and output threads 
*******************************************************************************/
#ifndef __OUTPUT
#define __OUTPUT

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <semaphore.h>
#include "include/linux/can.h"

#define LINEBUFFSIZE 320 // Lines for 2048 flash block, plus some
#define LBUFSZ 34 // Length of longest ascii/hex CAN msg+1

struct LBUFF
{
	char buf[LBUFSZ]; // One CAN msg line
	uint8_t len;	// Length including '\n'
};
struct LINEBUFF
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
	struct can_frame fbuf[FRAMEBUFFSIZE];
	struct can_frame* padd;
	struct can_frame* ptake;
	struct can_frame* pend;
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
 int output_add_frames(struct can_frame* pfr);
/* @brief   : Add CAN frame to the output buffer of frames
 * @param   : pfr = pointer to input frame
 * @return	:  0 = OK; 
 * ************************************************************************************** */


#endif