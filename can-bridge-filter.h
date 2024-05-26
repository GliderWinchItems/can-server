/*******************************************************************************
* File Name          : can-bridge-filter.h
* Date First Issued  : 05/16/2024
* Board              : 
* Description        : Setup bridging data from input file
*******************************************************************************/

#ifndef __CAN_BRIDGE_FILTER
#define __CAN_BRIDGE_FILTER

#define CBFNxNMAX 5     // 5x5 max size of matrix tables
#define CBFSORTMIN 12   // ID arrays shorter than this are not 'bsearch'd
#define CBFARRAYMAX 512 // An ID array larger than this is suspect.

/* Translation are pairs of CAN IDs*/
struct CBF2C
{
	uint32_t in;  // Incoming CAN id to match upon
	uint32_t out; // Outgoing CAN id translated; 0 = no translation
};

/* Pointers and codes for accessing tables.
   (Use an array of size N*N of these structs.) */
struct CBFNxN
{
	struct CBF2C* p2c; // Ptr translate table (id pairs, 2 columns)
	uint32_t*     p1c; // Ptr block:pass table (single id, 1 column)
	uint16_t  size_2c; // Size of 2 column struct array; 0 = empty table
	uint16_t  size_1c; // Size of 1 column uint32_t array; 0 = empty table
	uint8_t      type; // -1 = self; 0 = pass on match; 1 = block on match
};

/* Starting place for tables. */
struct CBF_TABLES
{
	struct CBFNxN* pnxn; // Pointer to first CBFNxN struct array
	uint8_t n;  // Matrix size 'n' (1 - CBFNxNMAX)
};

/* **************************************************************************************/
struct CBF_TABLES* can_bridge_filter_init(FILE *fp);
/* @brief   : Read file, extract IDs, and build filter table
 * @param   : fp = file pointer to filter table
 * @return  : pointer to base table struct; NULL = failed
 * ************************************************************************************** */
void printtablesummary(struct CBF_TABLES* pcbf);
/* @brief   : print table summary
 * @param   : pcbf = pointer to beginning struct for tables
*******************************************************************************/


#endif
