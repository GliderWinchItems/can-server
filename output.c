/*******************************************************************************
* File Name          : output.c
* Date First Issued  : 06/21/2024
* Board              : Seeed CAN hat
* Description        : Output buffering and output threads 
*******************************************************************************/


#include <semaphore.h>
#include <sys/uio.h>
#include <threads.h>
#include <string.h>
#include "output.h"



struct LINEBUFF linebuff;
struct FRAMEBUFF framebuff;

pthread_t thread_lines;
pthread_t thread_frames;

/* **************************************************************************************
 * int output_init_tcp(int socket);
 * @brief   : Initialize output threads and semaphores
 * @param   : socket = network socket
 * @return	:  0 = OK; 
 * ************************************************************************************** */
int output_init_tcp(int socket)
{
	int ret;
	linebuff.padd      = &linebuff.lbuf[0];
	linebuff.ptake     = linebuff.padd;
	linebuff.pend      = &linebuff.lbuf[LINEBUFFSIZE];
	linebuff.socket    = socket;

	ret = sem_init(&linebuff.sem, 0, 0); 
	if (ret != 0)
	{
		printf("output_init: semaphore init fail\n");
		return -1;
	}

    linebuff.tret = pthread_create( &thread_lines, NULL, output_thread_lines, (struct LINEBUFF*)&linebuff);
    if(linebuff.tret)
     {
         fprintf(stderr,"Error - pthread_create() thread_lines return code: %d\n",linebuff.tret);
         exit(EXIT_FAILURE);
     }
	return 0;
}
/* **************************************************************************************
 * int output_init_can(int socket);
 * @brief   : Initialize output threads and semaphores
 * @param   : socket = CAN socket
 * @return	:  0 = OK; 
 * ************************************************************************************** */
int output_init_can(int socket)
{
	int ret;
	framebuff.padd   = &framebuff.frame[0];
	framebuff.ptake  = framebuff.padd;
	framebuff.pend   = &framebuff.frame[FRAMEBUFFSIZE];
	framebuff.socket = socket;

	ret = sem_init(&framebuff.sem, 0, 0); 
	if (ret != 0)
	{
		printf("output_init: semaphore init fail\n");
		return -1;
	}
    framebuff.tret = pthread_create( &thread_frames, NULL, output_thread_frames, (struct FRAMEBUFF*)&framebuff);
    if(framebuff.tret)
     {
         fprintf(stderr,"Error - pthread_create() thread-frames return code: %d\n",framebuff.tret);
         exit(EXIT_FAILURE);
     }
	return 0;
}

/* **************************************************************************************
 * int output_add_lines(char* pc, int n);
 * @brief   : Add a CAN msg line to the output buffer (limited to 33 chars)
 * @param   : pc = pointer to input that ends with a '\n'
 * @param   : n = number of chars to transfer (n = 15 - 33)
 * @return	:  0 = OK; 
 * ************************************************************************************** */
int output_add_lines(char* pc, int n)
{
	struct LBUFF* plb = linebuff.padd;
	char* pb = &plb->lbuf.buf[0];
	int i;

/* Since 'n' is limited to: 14 < n < 34	why not inline this copy with uint_32_t, or uint64_t? */
	memcpy(pb, pc, n);

	plb->len = n; // Save length so we don't have to do another str(len)

	plb->padd += 1; // Step to next line buffer. Check for wraparound
	if (plb->padd >= plb->pend) plb->padd = &linebuff.lbuf[0];

	sem_post(&linebuff.sem); // Increments semaphore
	return 0;
}
/* **************************************************************************************
 * int output_add_frames(struct FRAMEBUFF* pfr);
 * @brief   : Add CAN frame to the output buffer of frames
 * @param   : pfr = pointer to input frame
 * @return	:  0 = OK; 
 * ************************************************************************************** */
int output_add_frames(struct FRAMEBUFF* pfr)
{
	struct FRAMEBUFF* pfb = &framebuff;
	*pfb->padd = *pfr; // Add frame to buffer
	pfb->padd += 1;
	if (pfb->padd >= framebuff.pend) pfb->padd = &frambuff.fbuf[0];
	sem_post(&framebuff.sem); // Increments semaphore
	return 0;	
}
/* **************************************************************************************
 * void output_thread_lines(struct LINEBUFF* plb;);
 * @brief   : Output buffered lines to TCP socket
 * @param   : plb = pointer to struct with buffer and pointers
 * ************************************************************************************** */
void output_thread_lines(struct LINEBUFF* plb;)
{
	while(1==1)
	{
		sem_wait(&plb->sem); // Decrements sem
		send(server_socket,&plb->ptake->lbuf.buf[0], plb->ptake->lbuf.len, 0);
		plb->ptake += 1;
		if (plb->ptake >= plb->pend) plb->ptake = &linebuff.lbuf[0];
	}
}
/* **************************************************************************************
 * void output_thread_frames(struct FRAMEBUFF* plf);
 * @brief   : Output buffered frames to CAN socket
 * @param   : plf = pointer to struct with buffer and pointers
 * ************************************************************************************** */
void output_thread_frames(struct FRAMEBUFF* plf)
{
	while(1==1)
	{
		sem_wait(&plf->sem);
		send(raw_socket, plf->ptake, sizeof(struct can_frame), 0);
		plf->take += 1;
		if (plf->take >= plf->pend) plf->take = &frambuff.frame[0];
	}

}