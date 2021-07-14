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

#include <linux/can.h>
#include "can-so.h"
#include "can-os.h"

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
	int i, ret, items;
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
	/*
	 * Check if there are more elements in the element buffer before calling select() and
	 * blocking for new packets.
	 */
	if(more_elements) {
		FD_CLR(raw_socket, &readfds);
	} else {
		ret = select((raw_socket > client_socket)?raw_socket+1:client_socket+1, &readfds, NULL, NULL, NULL);

		if(ret < 0) {
			PRINT_ERROR("Error in select()\n")
				state = STATE_SHUTDOWN;
			return;
		}
	}

	if(FD_ISSET(raw_socket, &readfds)) {
		iov.iov_len = sizeof(frame);
		msg.msg_namelen = sizeof(addr);
		msg.msg_flags = 0;
		msg.msg_controllen = sizeof(ctrlmsg);

		ret = recvmsg(raw_socket, &msg, 0);
		if(ret < sizeof(struct can_frame)) {
			PRINT_ERROR("Error reading frame from RAW socket\n")
				} else {
			/* read timestamp data */
			for (cmsg = CMSG_FIRSTHDR(&msg);
			     cmsg && (cmsg->cmsg_level == SOL_SOCKET);
			     cmsg = CMSG_NXTHDR(&msg,cmsg)) {
				if (cmsg->cmsg_type == SO_TIMESTAMP) {
					tv = *(struct timeval *)CMSG_DATA(cmsg);
				}
			}

			if(frame.can_id & CAN_ERR_FLAG) {
				canid_t class = frame.can_id  & CAN_EFF_MASK;
				ret = sprintf(buf, "< error %03X %ld.%06ld >", class, tv.tv_sec, tv.tv_usec);
				send(client_socket, buf, strlen(buf), 0);
			} else if(frame.can_id & CAN_RTR_FLAG) {
				/* TODO implement */
			} else {
				
//				if(frame.can_id & CAN_EFF_FLAG) {
//					ret = sprintf(buf, "%08X ", frame.can_id << 3);
//				} else {
//					ret = sprintf(buf, "%08X ", frame.can_id << 21);
//				}
//				ret += sprintf(buf+ret," %d ",frame.can_dlc);
//				for(i=0;i<frame.can_dlc;i++) {
//					ret += sprintf(buf+ret, "%02X", frame.data[i]);
//				}
				/* "so" = Convert from Socket/Seeed to Our/Old ascii format */
				if (can_so_cnvt(&canall_r,&frame) != 0){
					ret += sprintf(buf,"\tERROR %d: CAN-SO ", ret);
				}
				ret += sprintf(buf,"%s",canall_r.caa);
				send(client_socket, buf, strlen(buf), 0);
			}
		}
	}
#define XBUFSZ 256
char xbuf[XBUFSZ];
	if(FD_ISSET(client_socket, &readfds)) {
//		ret = receive_command(client_socket, (char *) &buf);
		ret = read(client_socket, xbuf, XBUFSZ);
		xbuf[ret+1]= 0;
		if (strlen(xbuf) > 1){
			printf("xbuf %s",xbuf);

		/* Convert from Our/Old to Seeed/Socket format */
		ret1 = can_os_cnvt(&frame,&canall_w,xbuf);
		if (ret1 < 0){
			switch(ret1){
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
			default: printf("ERROR: can_os: error not classified: %d\n");
				break;
			}
		}
		}
		ret = send(raw_socket, &frame, sizeof(struct can_frame), 0);
		if(ret==-1) {
printf("STATE_RAW: send(raw_socket...\n");
			state = STATE_SHUTDOWN;
			return;
		}
	} 

	ret = read(client_socket, &xbuf, 0);
	if(ret==-1) {
printf("STATE_RAW: read(client_sock...SHUTDOWN\n");
		state = STATE_SHUTDOWN;
		return;
	}
}
