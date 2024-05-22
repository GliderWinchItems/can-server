/*******************************************************************************
* File Name          : can-bridge.c
* Date First Issued  : 07/23/2021
* Board              : Seeed CAN hat
* Description        : Bridge/gateway between CAN0 and CAN1
*******************************************************************************/
/* 05/18/2024
	Command line:
   can-bridge <can#1> <file#1>
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

#include "can-bridge-filter.h"

#define MAXLEN 4000
//#define PORT 29536

#define STATE_INIT 0
#define STATE_CONNECTED 1
#define STATE_SHUTDOWN 2

#define PRINT_INFO(...) printf(__VA_ARGS__);
#define PRINT_ERROR(...) fprintf(stderr, __VA_ARGS__);
#define PRINT_VERBOSE(...) printf(__VA_ARGS__);

#define XBUFSZ 128 // Number chars to read from RAW socket read
struct CANALL canall_r; // Our format: 'r' = read from CAN bus
struct CANALL canall_w; // Our format: 'w' = write to CAN bus
//static int ret1;
//static char xbuf[XBUFSZ]; // See socketcand.h for XBUFSZ
//static char *pret; // extract_line_get() return points to line

FILE* fp;

int verbose_flag=0;

void print_usage(void);
void sigint();
int receive_command(int socket, char *buf);

#define BUSNAMESZ 8
struct RAWSOCSTUF
{
	int raw_socket;
	struct ifreq ifr;
	struct sockaddr_can addr;
	struct msghdr msg;
	struct can_frame frame;
	struct iovec iov;	
	FILE *fp; // Gateway file for bus_name CAN interface
	char bus_name[BUSNAMESZ];
}rs[2];

void print_usage(void)
{
	printf("Command line must have 2 arguments, e.g.--\n\
		can-bridge --file <path/file>\n\
		can-bridge --file CANbridge2x2.txt\n");
	return;
}

int main(int argc, char **argv)
{
	int ret;
	struct sigaction sigint_action;
	fd_set readfds;
	
/* Command line:
   can-bridge -f <path/file>
*/
	if (argc < 3)
	{
		printf("Number of arguments err: %d\n",argc);
		print_usage();
		exit(1);
	}
	if (strcmp(argv[1],"--file") != 0)
	{ 
		printf("ERR cmdline: 1st arg expected is --file\n");
		print_usage();
		exit(1);
	}
	if ((fp=fopen(argv[2], "r")) == NULL)
	{
		printf("ERR cmdline: file %s did not open\n",argv[2]);
		exit(1);
	}
	if (can_bridge_filter_init(fp) == NULL)
	{
		printf("ERR: bridging file %s set up failed\n",argv[2]);
		exit(1);
	}

	strcpy(rs[0].bus_name,"can0");
	strcpy(rs[1].bus_name,"can1");

#if 0
	if (strcmp(argv[1], "can0") || strcmp(argv[1], "can1"))
	{
		strncpy(rs[0].bus_name,argv[1],BUSNAMESZ);
	}
	else
	{
		printf("First CAN name %s: not can0 or can1\n",argv[1]);
		print_usage();
		exit(1);
	}
	if (strcmp(argv[3], "can0") || strcmp(argv[3], "can1"))
	{
		strncpy(rs[1].bus_name,argv[3],BUSNAMESZ);
	}
	else
	{
		printf("Second CAN name %s: not can0 or can1\n",argv[3]);
		print_usage();
		exit(1);
	}
	if (strcmp(rs[0].bus_name, rs[1].bus_name) == 0)
	{
		printf("CAN bus names cannot be the same\n");
		print_usage();
		exit(1);
	}
#endif

	if ( (rs[0].fp = fopen("can0","r")) == NULL)
	{
		printf("file: can0 did not open\n");
		print_usage();
		exit(1);
	}
	if ( (rs[1].fp = fopen("can1","r")) == NULL)
	{
		printf("file: can1 did not open\n");
		print_usage();
		exit(1);
	}

	sigint_action.sa_handler = &sigint;
	sigemptyset(&sigint_action.sa_mask);
	sigint_action.sa_flags = 0;
	sigaction(SIGINT, &sigint_action, NULL);

	int i;
	for (i = 0; i < 2; i++)
	{
		if((rs[i].raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			printf("Error while creating RAW socket number %d %s\n", (i+1), strerror(errno));
			exit(1);
		}

		strcpy(rs[i].ifr.ifr_name, rs[i].bus_name);
		if(ioctl(rs[i].raw_socket, SIOCGIFINDEX, &rs[i].ifr) < 0) {
			PRINT_ERROR("Error while searching for bus #%d %s\n", (i+1),strerror(errno));
			exit(1);
		}

		rs[i].addr.can_family = AF_CAN;
		rs[i].addr.can_ifindex = rs[i].ifr.ifr_ifindex;


		if(bind(rs[i].raw_socket, (struct sockaddr *) &rs[i].addr, sizeof(struct sockaddr_can)) < 0) {
			PRINT_ERROR("Error while binding RAW socket #%d %s\n", (i+1), strerror(errno));
			exit(1);
		}
	
		rs[i].iov.iov_base = &rs[i].frame;
		rs[i].msg.msg_name = &rs[i].addr;
		rs[i].msg.msg_iov  = &rs[i].iov;
		rs[i].msg.msg_iovlen = 1;

		printf("%s ready with file %s\n",rs[i].bus_name,argv[2+2*i]);
	}

 while(1)
 {

	FD_ZERO(&readfds);
	FD_SET(rs[0].raw_socket, &readfds);
	FD_SET(rs[1].raw_socket, &readfds);
	
	ret = select((rs[0].raw_socket > rs[1].raw_socket)?rs[0].raw_socket+1:rs[1].raw_socket+1, &readfds, NULL, NULL, NULL);

	if(ret < 0) 
	{
		PRINT_ERROR("Error in select()\n")
		exit(1);
	}

	if(FD_ISSET(rs[0].raw_socket, &readfds)) 
	{
		rs[0].iov.iov_len = sizeof(struct can_frame);
		rs[0].msg.msg_namelen = sizeof(struct sockaddr_can);
		rs[0].msg.msg_flags = 0;

		ret = recvmsg(rs[0].raw_socket, &rs[0].msg, 0);
//static int ctr1 = 0;
//printf("1st: %3d ret: %d\n", ctr1++,ret)		;
		if(ret < sizeof(struct can_frame)) 
		{
			PRINT_ERROR("Error reading frame from RAW socket\n")
		}
		else 
		{ 
//printf("ID: %08X\n",rs[0].frame.can_id);
			send(rs[1].raw_socket, &rs[0].frame, sizeof(struct can_frame), 0);
		}
	}

	if(FD_ISSET(rs[1].raw_socket, &readfds)) 
	{
		rs[1].iov.iov_len = sizeof(struct can_frame);
		rs[1].msg.msg_namelen = sizeof(struct sockaddr_can);
		rs[1].msg.msg_flags = 0;

		ret = recvmsg(rs[1].raw_socket, &rs[1].msg, 0);
//static int ctr2 = 0;
//printf("2nd: %3d ret: %d\n", ctr2++,ret)		;
		if(ret < sizeof(struct can_frame)) 
		{
			PRINT_ERROR("Error reading frame from RAW socket\n")
		}
		else 
		{ 
			send(rs[0].raw_socket, &rs[1].frame, sizeof(struct can_frame), 0);
		}
	}
 }
	return 0;
}

void sigint()
{
	if(verbose_flag)
		printf("received SIGINT\n");

	for (int i = 0; i < 2; i++)
	{
		if(rs[i].raw_socket != -1) 
		{
			if(verbose_flag == 1) printf("closing can socket #%d\n",(i+1));
					
			close(rs[i].raw_socket);
		}
	}
	exit(0);
}
/* eof */
