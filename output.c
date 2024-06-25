/*******************************************************************************
* File Name          : output.c
* Date First Issued  : 06/21/2024
* Board              : Seeed CAN hat
* Description        : Output buffering and output threads 
*******************************************************************************/

#include <stdio.h>
#include <sys/socket.h>
#include <semaphore.h>
#include <sys/uio.h>
#include <threads.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "output.h"

extern int server_socket;
extern int raw_socket;

struct LINEBUFF linebuff;
struct FRAMEBUFF framebuff;

pthread_t thread_lines;
pthread_t thread_frames;

void* output_thread_lines(void*);
void* output_thread_frames(void*);

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

    linebuff.tret = pthread_create( &thread_lines, NULL, output_thread_lines, NULL);
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
	framebuff.padd   = &framebuff.fbuf[0];
	framebuff.ptake  = framebuff.padd;
	framebuff.pend   = &framebuff.fbuf[FRAMEBUFFSIZE];
	framebuff.socket = socket;

	ret = sem_init(&framebuff.sem, 0, 0); 
	if (ret != 0)
	{
		printf("output_init: semaphore init fail\n");
		return -1;
	}
    framebuff.tret = pthread_create( &thread_frames, NULL, output_thread_frames, NULL);
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
static unsigned int CANtoTCPmax;
//static int bbb = 0;
int output_add_lines(char* pc, int n)
{
/* Since 'n' is limited to: 14 < n < 34	why not inline this copy with uint_32_t, or uint64_t? */
	memcpy(&linebuff.padd->buf[0], pc, n);

	linebuff.padd->len = n; // Save length so we don't have to do another costly str(len)

	linebuff.padd += 1; // Step to next line buffer. Check for wraparound
	if (linebuff.padd >= linebuff.pend) linebuff.padd = &linebuff.lbuf[0];

//printf("D %d %u\n",bbb++,(int)(linebuff.padd - linebuff.ptake));	
//printf("A %d %lu %lu\n",bbb++, *(unsigned long*)linebuff.ptake, *(unsigned long*)linebuff.padd);

	sem_post(&linebuff.sem); // Increments semaphore

	int tmp = (int)(linebuff.padd - linebuff.ptake);
	if (tmp < 0) tmp += LINEBUFFSIZE;
	if (tmp > CANtoTCPmax) CANtoTCPmax = tmp;

	return 0;
}
/* **************************************************************************************
 * int output_add_frames(struct FRAMEBUFF* pfr);
 * @brief   : Add CAN frame to the output buffer of frames
 * @param   : pfr = pointer to input frame
 * @return	:  0 = OK; 
 * ************************************************************************************** */
static unsigned int TCPtoCANmax;
int output_add_frames(struct can_frame* pfr)
{
	struct FRAMEBUFF* pfb = &framebuff;
	*pfb->padd = *pfr; // Add frame to buffer
	pfb->padd += 1;
	if (pfb->padd >= framebuff.pend) framebuff.padd = &framebuff.fbuf[0];
	sem_post(&framebuff.sem); // Increments semaphore

	/* Find max depth of buffering. */
	int tmp = (int)(framebuff.padd - framebuff.ptake);
	if (tmp < 0) tmp += FRAMEBUFFSIZE;
	if (tmp > TCPtoCANmax) TCPtoCANmax = tmp;

	return 0;	
}
/* **************************************************************************************
 * void* output_thread_lines(struct LINEBUFF* plb;);
 * @brief   : Output buffered lines to TCP socket
 * @param   : plb = pointer to struct with buffer and pointers
 * ************************************************************************************** */
void* output_thread_lines(void* p)
{
//int a = 0;	
printf("\nOUTPUT_THREAD_LINES: start\n");	

	while(1==1)
	{
		sem_wait(&linebuff.sem); // Decrements sem
		while (linebuff.ptake != linebuff.padd)
		{
			send(server_socket,&linebuff.ptake->buf[0], linebuff.ptake->len, 0);
			linebuff.ptake += 1;
			if (linebuff.ptake >= linebuff.pend) linebuff.ptake = &linebuff.lbuf[0];
//printf("T %d\n",a++);			
//usleep(100);
		}
	}
}
/* **************************************************************************************
 * void* output_thread_frames(struct FRAMEBUFF* plf);
 * @brief   : Output buffered frames to CAN socket
 * @param   : plf = pointer to struct with buffer and pointers
 * ************************************************************************************** */
void* output_thread_frames(void* p)
{
	printf("\nOUTPUT_THREAD_FRAMES: start\n");	
	while(1==1)
	{
		sem_wait(&framebuff.sem);
		send(raw_socket, framebuff.ptake, sizeof(struct can_frame), 0);
		framebuff.ptake += 1;
		if (framebuff.ptake >= framebuff.pend) framebuff.ptake = &framebuff.fbuf[0];
//		usleep(280);
	}
}