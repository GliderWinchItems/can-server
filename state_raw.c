/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-3-Clause) */

#include "config.h"
#include "socketcand.h"

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

int raw_socket;
struct ifreq ifr;
struct sockaddr_can addr;
struct msghdr msg;
struct can_frame frame;
struct iovec iov;


struct CANALL canall_r; // Our format: 'r' = read from CAN bus
struct CANALL canall_w; // Our format: 'w' = write to CAN bus

static char xbuf[XBUFSZ]; // See socketcand.h for XBUFSZ
static char *pret; // extract_line_get() return points to line


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

		previous_state = STATE_RAW;
	}
/* Do endless loop here, rather than exiting routine to socketcand and back */
while(1==1)
 {
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
				send(client_socket,buf, strlen(buf), 0);
				if (verbose_flag == 1) { printf("%s",buf); }
			}
			else
			{
				send(client_socket, canall_r.caa, canall_r.caalen, 0);
			}
		}
	}

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
