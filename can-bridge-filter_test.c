/*******************************************************************************
* File Name          : can-bridge-filter_test.c
* Date First Issued  : 05/25/2024
* Board              : 
* Description        : Test can-bridge-filter
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "common_can.h"
#include "../../GliderWinchCommons/embed/svn_common/trunk/db/gen_db.h"
#include "can-bridge-filter.h"
#include "can-bridge-filter-lookup.h"
#include "can-bridge-filter_test.h"
#include "CANid-hex-bin.h"

/* Hard code test input for test. */
struct INID
{
	uint32_t id;
	uint8_t in;
	uint8_t out;
};
struct INID inid[] = 
{
	{0x00400000, 1, 3},
	{0x47400000, 1, 3},
	{0xE3200000, 1, 2},
	{0x12345678, 1, 2},
	{0x12345678, 1, 3},
	{0x12345678, 2, 1},
	{0x12345678, 2, 3},
	{0x12345678, 3, 1},
	{0x12345678, 3, 3},
	{       0x0, 0, 0} /* END */
};

/*******************************************************************************
 * void can_bridge_filter_test(struct CBF_TABLES* pcbf);
 * @brief   : Run some tests
*******************************************************************************/
void can_bridge_filter_test(struct CBF_TABLES* pcbf)
{
	printtablesummary(pcbf);
	char cmsg[16];
	memset(cmsg,'0',10);
	cmsg[10] = 0;
	int ret;
	int ctr = 1;
	int i = 0;
	while (inid[i].id > 0)
	{
		CANid_bin_hex(cmsg, inid[i].id);
		ret=can_bridge_filter_lookup((uint8_t*)cmsg, pcbf, (inid[i].in-1), (inid[i].out-1));
		printf("%3i:  rc %i %i  copy %i %08X cmsg:%s",ctr++, inid[i].in, inid[i].out, ret, inid[i].id, &cmsg[2]);
		printf(" rev:%1c%1c%1c%1c%1c%1c%1c%1c\n",cmsg[8],cmsg[9],cmsg[6],cmsg[7],cmsg[4],cmsg[5],cmsg[2],cmsg[3]);
		i += 1;
	} 
	return;
}

