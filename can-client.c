/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */
/*
 * Authors:
 * Andre Naujoks
 * Oliver Hartkopp
 * Jan-Niklas Meier
 * Felix Obenhuber
 *
 * Copyright (c) 2002-2012 Volkswagen Group Electronic Research
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of Volkswagen nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * Alternatively, provided that this notice is retained in full, this
 * software may be distributed under the terms of the GNU General
 * Public License ("GPL") version 2, in which case the provisions of the
 * GPL apply INSTEAD OF those given above.
 *
 * The provided data structures and external interfaces from this code
 * are not restricted to be used by modules with a GPL compatible license.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * Send feedback to <linux-can@vger.kernel.org>
 *
 */

/*
Example: can1 connects to hub-server with hub-server on port 32127
./can-client -s 192.168.2.139 -p 32127 -i can1 -v
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <linux/can.h>

#include "can-os.h"
#include "can-so.h"
#include "extract-line.h"
#include "output.h"

/* enable output buffering w output threads. */
#define OBUF

#define MAXLEN 8192 //4000
#define PORT 29536

#define STATE_INIT 0
#define STATE_CONNECTED 1
#define STATE_SHUTDOWN 2

#define PRINT_INFO(...) printf(__VA_ARGS__);
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__);
#define PRINT_VERBOSE(...) printf(__VA_ARGS__);

#define XBUFSZ 4096 // 128 // Number chars to read from RAW socket read
struct CANALL canall_r; // Our format: 'r' = read from CAN bus
struct CANALL canall_w; // Our format: 'w' = write to CAN bus
static int ret1;
static char xbuf[XBUFSZ]; // See socketcand.h for XBUFSZ
static char *pret; // extract_line_get() return points to line
int daemon_flag=0; // logfile flag (see socketcand.c)


void print_usage(void);
void sigint();
int receive_command(int socket, char *buf);
void state_connected();

int server_socket;
int raw_socket;
int port;
int verbose_flag=0;
int cmd_index=0;
int more_elements=0;
int state, previous_state;
char ldev[IFNAMSIZ];
char rdev[IFNAMSIZ];
char buf[MAXLEN];
char cmd_buffer[MAXLEN];

struct msghdr msg;


int main(int argc, char **argv)
{
	struct sockaddr_in serveraddr;
	struct hostent *server_ent;
	struct sigaction sigint_action;
	char* server_string;

	/* set default config settings */
	port = PORT;
	strcpy(ldev, "can0");
	strcpy(rdev, "can0");
	server_string = malloc(strlen("localhost"));

	/* Parse commandline arguments */
	for(;;) {
		/* getopt_long stores the option index here. */
		int c, option_index = 0;
		static struct option long_options[] = {
			{"verbose", no_argument, 0, 'v'},
			{"interfaces",  required_argument, 0, 'i'},
			{"server", required_argument, 0, 's'},
			{"port", required_argument, 0, 'p'},
			{"version", no_argument, 0, 'z'},
			{0, 0, 0, 0}
		};

		c = getopt_long(argc, argv, "vhi:p:l:s:", long_options, &option_index);

		if(c == -1)
			break;

		switch(c) {
		case 0:
			/* If this option set a flag, do nothing else now. */
			if(long_options[option_index].flag != 0)
				break;
			break;

		case 'v':
			puts("Verbose output activated\n");
			verbose_flag = 1;
			break;

		case 'p':
			port = atoi(optarg);
			break;

		case 's':
			server_string = realloc(server_string, strlen(optarg)+1);
			strcpy(server_string, optarg);
			break;

		case 'i':
	strcpy(rdev, optarg);
	strcpy(ldev, optarg);
//	printf("i OPTION: bus_name:%s\n",ldev);
			break;

		case 'h':
			print_usage();
			return 0;

		case 'z':
			printf("socketcandcl version '%s'\n", "0.1");
			return 0;

		case '?':
			print_usage();
			return 0;

		default:
			print_usage();
			return -1;
		}
	}

	sigint_action.sa_handler = &sigint;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);

	server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if(server_socket < 0) 
	{
		perror("socket");
		exit(1);
	}

	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	serveraddr.sin_port = htons(port);

	server_ent = gethostbyname(server_string);
	if(server_ent == 0) 
	{
		perror(server_string);
		exit(1);
	}

	memcpy(&(serveraddr.sin_addr.s_addr), server_ent->h_addr,
	      server_ent->h_length);


	if(connect(server_socket, (struct sockaddr*)&serveraddr,
		   sizeof(serveraddr)) != 0) 
	{
		perror("connect");
		exit(1);
	}

	for(;;) 
	{
		switch(state) 
		{
		case STATE_INIT:
				previous_state = STATE_INIT;
				state = STATE_CONNECTED;

		case STATE_CONNECTED:
			state_connected();
			break;
		case STATE_SHUTDOWN:
			PRINT_VERBOSE("Closing client connection.\n");
			close(server_socket);
			return 0;
		}
	}
	return 0;
}

inline void state_connected()
{

	int ret;
	static struct can_frame frame;
	static struct ifreq ifr;
	static struct sockaddr_can addr;
	fd_set readfds;
	struct iovec iov;

	if(previous_state != STATE_CONNECTED) 
	{
		if((raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			PRINT_ERROR("Error while creating RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		strcpy(ifr.ifr_name, ldev);
		if(ioctl(raw_socket, SIOCGIFINDEX, &ifr) < 0) {
			PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;

		/* turn on timestamp */
		const int timestamp_on = 0;
		if(setsockopt(raw_socket, SOL_SOCKET, SO_TIMESTAMP, &timestamp_on, sizeof(timestamp_on)) < 0) {
			PRINT_ERROR("Could not enable CAN timestamps\n");
			state = STATE_SHUTDOWN;
			return;
		}
		/* bind socket */
		if(bind(raw_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			PRINT_ERROR("Error while binding RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		iov.iov_base = &frame;
		msg.msg_name = &addr;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
//		msg.msg_control = &ctrlmsg;

		previous_state = STATE_CONNECTED;
	}
#ifdef OBUF	
	output_init_tcp(server_socket);
	output_init_can(raw_socket);
#endif

 for(;;) 
 {

	FD_ZERO(&readfds);
	FD_SET(raw_socket, &readfds);
	FD_SET(server_socket, &readfds);
	
	ret = select((raw_socket > server_socket)?raw_socket+1:server_socket+1, &readfds, NULL, NULL, NULL);

	if(ret < 0) 
	{
		PRINT_ERROR("Error in select()\n")
		state = STATE_SHUTDOWN;
		return;
	}

	if(FD_ISSET(raw_socket, &readfds)) 
	{
		iov.iov_len = sizeof(frame);
		msg.msg_namelen = sizeof(addr);
		msg.msg_flags = 0;

		ret = recvmsg(raw_socket, &msg, 0);
		if(ret < sizeof(struct can_frame)) 
		{
			PRINT_ERROR("Error reading frame from RAW socket\n")
		}
		else 
		{ 
			/* "so" = Convert from Socket/Seeed to Our/Old ascii format */
			if (can_so_cnvt(&canall_r,&frame) != 0)
			{
				sprintf(buf,"ERROR %d %08X: CAN-SO \n", ret, frame.can_id);
#ifdef OBUF				
 output_add_lines(buf,strlen(buf));
#else 
				send(server_socket,buf, strlen(buf), 0);
#endif				
				if (verbose_flag == 1) { printf("%s",buf); }
			}
			else
			{
#ifdef OBUF					
 output_add_lines(canall_r.caa, canall_r.caalen);			
#else 
				send(server_socket, canall_r.caa, canall_r.caalen, 0);
#endif				
			}
		}
	}

	if(FD_ISSET(server_socket, &readfds)) 
	{
		ret = read(server_socket, xbuf, XBUFSZ);
		if (ret > 0)
		{ // Here, some additional incoming chars from the stream 
			extract_line_add(xbuf,ret); // Add to a buffer

			do /* Extract:Convert:send lines until no lines in buffer. */
			{				
				pret = extract_line_get(); // Attempt to get line from buffer
				if (pret != NULL)
				{ // Here, pret points to a complete line
					ret1 = can_os_cnvt(&frame,&canall_w,pret);
					if (ret1 == 0)
					{ // Here, conversion to output frame good and ready to send
#ifdef OBUF							
	output_add_frames(&frame);
#else	
						send(raw_socket, &frame, sizeof(struct can_frame), 0);
#endif						
					}
					else
					{ // Here, some sort of error with the ascii line
						can_os_printerr(ret1); // Nice format error output
					}
				}
			} while (pret != NULL);
		}
		if (ret < 0)
		{
			PRINT_ERROR("Error reading frame from client socket\n")
		}
	}
 }
	return;
}

void print_usage(void)
{
	printf("Usage: socketcandcl [-v | --verbose] [-i interfaces | --interfaces interfaces]\n\t\t[-s server | --server server ]\n\t\t[-p port | --port port]\n");
	printf("Options:\n");
	printf("\t-v activates verbose output to STDOUT\n");
	printf("\t-s server hostname\n");
	printf("\t-i SocketCAN interfaces to use: device_server,device_client \n");
	printf("\t-p port changes the default port (%d) the client connects to\n", PORT);
	printf("\t-h prints this message\n");
}

void sigint()
{
	if(verbose_flag)
		PRINT_ERROR("received SIGINT\n")

			if(server_socket != -1) {
				if(verbose_flag)
					PRINT_INFO("closing server socket\n")
						close(server_socket);
			}

	if(raw_socket != -1) {
		if(verbose_flag)
			PRINT_INFO("closing can socket\n")
				close(raw_socket);
	}

	exit(0);
}

/* eof */
