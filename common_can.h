/******************************************************************************
* File Name          : common_can.h
* Date First Issued  : 12/27/2012
* Board              : STM32F103VxT6_pod_mm
* Description        : includes "common" for CAN for all sensor programs
*******************************************************************************/
/*
10/16/2013 Copy of svn_sensor/common_all/trunk/common_can.h (svn_sensor rev 259)
*/

/*
Note: '#define CANOPEN' below separates two sections.  The first is the original 
 bit assignment scheme.  The 2nd is for CANOpen compatibility.  The first section
 may no longer compile/work correctly.  When the changes for CANOpen compatibility
 were there was no testing the reverting to the original scheme compiles and works
 correctly.
*/

#ifndef __SVN_COMMON_CAN
#define __SVN_COMMON_CAN

#include <stdint.h>

/* The solution to different paths is to include common.h in the caller 
to common_can.h
#ifdef STM32F3
//  #include "common.h"
  #include "../../../git_discoveryf3/lib/libopencm3/cm3/common.h"
#else
  #include "../../svn_discoveryf4/sw_discoveryf4/trunk/lib/libopencm3/cm3/common.h"
#endif
*/

#ifndef NULL 
#define NULL	0
#endif
/* This is the same UNION as used in svn_pod (see common.h) */
union LL_L_S	
{
	unsigned long long	  ull;		// 64 bit version unsigned
	signed	 long long         ll;		// 64 bit version signed
	unsigned int		ui[2];		// Two 32 bit'ers unsigned
	int			 n[2];		// Two 32 bit'ers signed
	unsigned long		ul[2];		// Two 32 bit'ers unsigned
	signed   long           sl[2];		// Two 32 bit'ers signed
	unsigned short		us[4];		// Four 16 bit unsigned
	signed   short		 s[4];		// Four 16 bit signed
	unsigned char		uc[8];		// Last but not least, 8b unsigned
	signed   char		 c[8];		// And just in case, 8b signed
};

/* Buffering incoming CAN messages */
union CANDATA	// Unionize for easier cooperation amongst types
{
   uint64_t  ull;
   int64_t   sll;
   double    dbl;
	float   f[2];
	uint32_t 	   ui[2];
	uint16_t 	   us[4];
	uint8_t 	   uc[8];
	uint8_t	       u8[8];
	int32_t        si[2];
	int16_t        ss[4];
	int8_t         sc[8];
};
struct CANRCVBUF		// Combine CAN msg ID and data fields
{ //                               offset  name:     verbose desciption
	uint32_t id;			// 0x00 CAN_TIxR: mailbox receive register ID p 662
	uint32_t dlc;		// 0x04 CAN_TDTxR: time & length p 660
	union CANDATA cd;	// 0x08,0x0C CAN_TDLxR,CAN_TDLxR: Data payload (low, high)
};
struct CANRCVTIMBUF		// CAN data plus timer ticks
{
	union LL_L_S	U;	// Linux time, offset, in 1/64th ticks
	struct CANRCVBUF R;	// CAN data
};
struct CANRCVSTAMPEDBUF
{
	union LL_L_S	U;	// Linux time, offset, in 1/64th ticks
	uint32_t id;			// 0x00 CAN_TIxR: mailbox receive register ID p 662
	uint32_t dlc;		// 0x04 CAN_TDTxR: time & length p 660
	union CANDATA cd;	// 0x08,0x0C CAN_TDLxR,CAN_TDLxR: Data payload (low, high)	
};

struct PP
{
	char 	*p;
	uint32_t	ct;
};

#define GPSSAVESIZE	80	// Max size of saved GPS line (less than 256)
struct GPSPACKETHDR
{
	union LL_L_S	U;	// Linux time, offset, in 1/64th ticks
	uint32_t 		id;	// Fake msg ID for non-CAN messages, such as GPS
	uint8_t c[GPSSAVESIZE];	// 1st byte is count; remaining bytes are data
};

union CANPC
{
	struct CANRCVBUF	can;		// Binary msg
	uint8_t c[sizeof(struct CANRCVBUF)+2];	// Allow for chksum w longest msg
};

struct CANPCWRAP	// Used with gateway (obsolete except for old code)
{
	union CANPC can;
	uint32_t	chk;
	uint8_t	*p;
	uint8_t	prev;
	uint8_t	c1;
	int16_t 	ct;
};



/* The following are used in the PC program and USART1_PC_gateway.  (Easier to find them here.) */

#define PC_TO_GATEWAY_ID	(' ')	// Msg to/from PC is for the gateway unit
#define PC_TO_CAN_ID		0x0	// Msg to/from PC is for the CAN bus

#define CAN_PC_FRAMEBOUNDARY	'\n'	// Should be a value not common in the data
#define CAN_PC_ESCAPE		0X7D	// Should be a value not common in the data

#define CHECKSUM_INITIAL	0xa5a5	// Initial value for computing checksum

#define ASCIIMSGTERMINATOR	'\n'	// Separator for ASCII/HEX msgs

#define PCTOGATEWAYSIZE	48	// (Note keep align 4 to allow casts to ints)
/* Compressed msg  */
struct PCTOGATECOMPRESSED
{
	uint8_t 	cm[PCTOGATEWAYSIZE/2];	// seq + id + dlc + payload bytes + jic spares
	uint8_t*	p;			// Pointer into cm[]
	int16_t	ct;			// Byte count of compressed result (not including checksum)
	uint8_t	seq;			// Message count
	uint8_t	chk;			// Checksum
};


struct PCTOGATEWAY	// Used in PC<->gateway asc-binary conversion & checking
{
	char asc[PCTOGATEWAYSIZE];	// ASCII "line" is built here
//	uint8_t	*p;			// Ptr into buffer
	char	*pasc;			// Ptr into buffer
	uint32_t	chk;			// Checksum
	int16_t	ct;			// Byte ct (0 = msg not complete; + = byte ct)
	int16_t	ctasc;			// ASC char ct
	uint8_t	prev;			// Used for binary byte stuffing	uint8_t x = hxbn[(uint8_t)(*p++)] << 4;// Get hi nibble of byte
//	x |= hxbn[(uint8_t)(*p)];		// Or in low nibble

	uint8_t	seq;			// Sequence number (CAN msg counter)
	uint8_t	mode_link;		// PC<->gateway mode (binary, ascii, ...)
	struct PCTOGATECOMPRESSED cmprs; // Easy way to make call to 'send'
};

/* ------------ PC<->gateway link MODE selection -------------------------------------- */
#define MODE_LINK	2	// PC<->gateway mode: 0 = binary, 1 = ascii, 2 = ascii-gonzaga


 struct FLASHP_SE4
 {
 	uint32_t crc;			// crc-32 placed by loader
	uint32_t version;		// version number
 	uint32_t numblkseg;		// Number of black segments
 	float ctr_to_feet;		// Shaft counts to feet conversion
 	uint32_t adc3_htr_initial;	// High threshold registor setting, ADC3
 	uint32_t adc3_ltr_initial;	// Low  threshold register setting, ADC3
 	uint32_t adc2_htr_initial;	// High threshold registor setting, ADC2
 	uint32_t adc2_ltr_initial;	// Low  threshold register setting, ADC2
	char c[4*8];			// ASCII identifier
 	double dtest;			// Test double
 	uint64_t ulltest;		// Test long long
 	uint32_t testidx;		// Test index
	uint32_t canid_error2;		// [0]encode_state er ct [1]adctick_diff<2000 ct
 	uint32_t canid_error1;		// [2]elapsed_ticks_no_adcticks<2000 ct  [3]cic not in sync
	uint32_t canid_adc3_histogramA;	// ADC3 HistogramA tx: request count, switch buffers. rx: send count
	uint32_t canid_adc3_histogramB;	// ADC3 HistogramB tx: bin number, rx: send bin count
	uint32_t canid_adc2_histogramA;	// ADC2 HistogramA tx: request count, switch buffers; rx send count
	uint32_t canid_adc2_histogramB;	// ADC2 HistogramB tx: bin number, rx: send bin count
	uint32_t canid_adc3_adc2_readout;	// ADC3 ADC2 readings readout
	uint32_t canid_se4h_counternspeed;	// Shaft counter and speed
	uint32_t canid_se4h_cmd;	// Command CAN: send commands to subsystem
 };


/* Error counts for monitoring. */
struct CANWINCHPODCOMMONERRORS
{
	uint32_t can_txerr; 		// Count: total number of msgs returning a TERR flags (including retries)
	uint32_t can_tx_bombed;	// Count: number of times msgs failed due to too many TXERR
	uint32_t can_tx_alst0_err; 	// Count: arbitration failure total
	uint32_t can_tx_alst0_nart_err;// Count: arbitration failure when NART is on
	uint32_t can_msgovrflow;	// Count: Buffer overflow when adding a msg
	uint32_t can_spurious_int;	// Count: TSR had no RQCPx bits on (spurious interrupt)
	uint32_t can_no_flagged;	// Count: 
	uint32_t can_pfor_bk_one;	// Count: Instances that pfor was adjusted in TX interrupt
	uint32_t can_pxprv_fwd_one;	// Count: Instances that pxprv was adjusted in 'for' loop
	uint32_t can_rx0err;		// Count: FIFO 0 overrun
	uint32_t can_rx1err;		// Count: FIFO 1 overrun
	uint32_t can_cp1cp2;		// Count: (RQCP1 | RQCP2) unexpectedly ON
	uint32_t error_fifo1ctr;	// Count: 'systickphasing' unexpected 'default' in switch
	uint32_t nosyncmsgctr;	// Count: 'systickphasing',lost sync msgs 
	uint32_t txint_emptylist;	// Count: TX interrupt with pending list empty
	uint32_t disable_ints_ct;	// Count: while in disable ints looped
};

struct PARAMIDPTR {
	uint16_t id;
	void*	ptr;
};

#endif 

