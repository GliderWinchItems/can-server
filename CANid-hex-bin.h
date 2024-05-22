/*******************************************************************************
* File Name          : CANid-hex-bin.h
* Date First Issued  : 05/17/2024
* Board              : 
* Description        : CAN ID to<->from ascii/hex format and binary
*******************************************************************************/
#ifndef __CANID_HEX_BIN
#define __CANID_HEX_BIN

/* ****************************************************************************/
uint32_t CANid_hex_bin(char* pc);
/* @brief   : Convert CAN id from ascii/hex to unint32_t
 * @param   : pc = pointer to ascii/hex CAN msg
 * @return  : CAN id as uint32_t; 0 = invalid input
 * ************************************************************************** */
void CANid_bin_hex(char* pc, uint32_t id);
/* @brief   : Convert CAN id uint32_t to ascii/hex
 * @param   : pc = pointer to ascii/hex CAN msg
 * @param	: id = CAN id to be hexed
 * ************************************************************************************** */

#endif
