/*******************************************************************************
* File Name          : can-bridge-filter-lookup.h
* Date First Issued  : 05/23/2024
* Board              : 
* Description        : Bridge & filter table search
*******************************************************************************/

#ifndef __CAN_BRIDGE_FILTER_LOOKUP
#define __CAN_BRIDGE_FILTER_LOOKUP

#include "can-bridge-filter.h"

/*******************************************************************************/
int can_bridge_filter_lookup(uint8_t* pmsg, struct CBF_TABLES* pcbf, uint8_t in, uint8_t out);
/* @brief   : Do the bridge
 * @param   : pmsg = pointer to asc/hex CAN msg beginning (sequence byte)
 * @param   : pcbf = pointer to struct holding pointer to table array struct and size 'N'
 * @param   : in = input  connection (0 - (N-1)), (not 1 - N)!
 * @param   : in = output connection (0 - (N-1)), (not 1 - N)!
 * @return  : 0 = not copy; 1 = copy;
*******************************************************************************/

#endif