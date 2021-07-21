/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include "config.h"
#include "socketcand.h"
#include "statistics.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/uio.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <syslog.h>

#include <errno.h>
#include <linux/can.h>
#include "can-so.h"
#include "can-os.h"
#include "extract-line.h"

int rctr;

int raw_socket;
struct ifreq ifr;
struct sockaddr_can addr;
struct msghdr msg;
struct can_frame frame;
struct iovec iov;
char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
struct timeval tv;
struct cmsghdr *cmsg;

struct CANALL canall_r; // Our format: 'r' = read from CAN bus
struct CANALL canall_w; // Our format: 'w' = write to CAN bus

void state_raw() {
	char buf[MAXLEN];
	int ret;
	int ret1;
	fd_set readfds;
	if(previous_state != STATE_RAW) {

		if((raw_socket = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
			PRINT_ERROR("Error while creating RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		strcpy(ifr.ifr_name, bus_name);
		if(ioctl(raw_socket, SIOCGIFINDEX, &ifr) < 0) {
			PRINT_ERROR("Error while searching for bus %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		addr.can_family = AF_CAN;
		addr.can_ifindex = ifr.ifr_ifindex;

		const int timestamp_on = 1;
		if(setsockopt( raw_socket, SOL_SOCKET, SO_TIMESTAMP, &timestamp_on, sizeof(timestamp_on)) < 0) {
			PRINT_ERROR("Could not enable CAN timestamps\n");
			state = STATE_SHUTDOWN;
			return;
		}

		if(bind(raw_socket, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
			PRINT_ERROR("Error while binding RAW socket %s\n", strerror(errno));
			state = STATE_SHUTDOWN;
			return;
		}

		iov.iov_base = &frame;
		msg.msg_name = &addr;
		msg.msg_iov = &iov;
		msg.msg_iovlen = 1;
		msg.msg_control = &ctrlmsg;

		previous_state = STATE_RAW;
		

	}

	FD_ZERO(&readfds);
	FD_SET(raw_socket, &readfds);
	FD_SET(client_socket, &readfds);	

	ret = select((raw_socket > client_socket)?raw_socket+1:client_socket+1, &readfds, NULL, NULL, NULL);

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
		msg.msg_controllen = sizeof(ctrlmsg);

		ret = recvmsg(raw_socket, &msg, 0);
		if(ret < sizeof(struct can_frame)) 
		{
			PRINT_ERROR("Error reading frame from RAW socket\n")
				} else {
			/* read timestamp data */
			for (cmsg = CMSG_FIRSTHDR(&msg);
			     cmsg && (cmsg->cmsg_level == SOL_SOCKET);
			     cmsg = CMSG_NXTHDR(&msg,cmsg)) 
			{
				if (cmsg->cmsg_type == SO_TIMESTAMP) 
				{
					tv = *(struct timeval *)CMSG_DATA(cmsg);
				}
			}

			if(frame.can_id & CAN_ERR_FLAG) 
			{
				canid_t class = frame.can_id  & CAN_EFF_MASK;
				ret = sprintf(buf, "< error %03X %ld.%06ld >", class, tv.tv_sec, tv.tv_usec);
				send(client_socket, buf, strlen(buf), 0);
			} 
			else if(frame.can_id & CAN_RTR_FLAG) 
			{
				/* TODO implement */
			} 
			else 
			{	
				/* "so" = Convert from Socket/Seeed to Our/Old ascii format */
				if (can_so_cnvt(&canall_r,&frame) != 0)
				{
					ret += sprintf(buf,"\tERROR %d: CAN-SO ", ret);
				}
				ret += sprintf(buf,"%s",canall_r.caa);
				send(client_socket, buf, strlen(buf), 0);
			}
		}
	}

static char xbuf[XBUFSZ]; // See socketcand.h for XBUFSZ
static char *pret; // extract_line_get() return points to line

static int32_t sctr = 0; // Debug counter

	if(FD_ISSET(client_socket, &readfds)) 
	{
		ret = read(client_socket, xbuf, XBUFSZ);
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
						send(raw_socket, &frame, sizeof(struct can_frame), 0);
sctr += 1;							
					}
					else
					{ // Here, some sort of error with the ascii line
printf("sctr: %d ",sctr)						;
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
	return;
}
