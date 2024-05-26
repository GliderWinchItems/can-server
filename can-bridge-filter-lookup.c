/*******************************************************************************
* File Name          : can-bridge-filter-lookup.c
* Date First Issued  : 05/23/2024
* Board              : 
* Description        : Bridge & filter table search
*******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "common_can.h"
#include "can-bridge-filter.h"
#include "can-bridge-filter-lookup.h"
#include "CANid-hex-bin.h"

/*******************************************************************************
 * int can_bridge_filter_lookup(uint8_t* pmsg, struct CBF_TABLES* pcbf, uint8_t in, uint8_t out);
 * @brief   : Do the bridge
 * @param   : pmsg = pointer to asc/hex CAN msg beginning (sequence byte)
 * @param   : pcbf = pointer to struct holding pointer to table array struct and size 'N'
 * @param   : in = input  connection (0 - (N-1)), => (not 1 - N) <=
 * @param   : in = output connection (0 - (N-1)), => (not 1 - N) <=
 * @return  : 0 = not copy; 1 = copy;
*******************************************************************************/
int can_bridge_filter_lookup(uint8_t* pmsg, struct CBF_TABLES* pcbf, uint8_t in, uint8_t out)
{
	int i,j,k;

	uint32_t id;
	/* Computer address of CBFNxN array for this in:out pair. */
	struct CBFNxN* pbnn = pcbf->pnxn + ((in)*pcbf->n) + out;
	struct CBF2C* p2c;
	uint32_t* p1c;

	/* Tables are handled differently for pass and block types. */
	if (pbnn->type == 0)
	{ /* Type = Pass-on-match. 2 column table lookup w translation */
		if (pbnn->size_2c == 0)
		{ // Here, no lookup entries, (and no translations)
			return 0; // ==> block all <==
		}
		else
		{ //Here, tables has entries



			id = CANid_hex_bin((char*)(pmsg+2)); // Get CAN id in binary
			if (id == 0)
				return 0; // Non-hex in CAN id. Do not copy in->oout
			// binary search 2 column table
			p2c = pbnn->p2c;
		    i = 0; j = (pbnn->size_2c - 1);
		    while (i <= j) 
		    {
		        k = i + ((j - i) / 2);
//printf("TT %3i %3i %3i %08X %08X %08X\n",i,j,k,id,(p2c+k)->in,(p2c+k)->out);
		        if ((p2c+k)->in == id) 
		        { // Here, found
					if ((p2c+k)->out == 0)
					{ // Here, ID is not translated
						return 1; // Copy in->out, no ID change
					}
					else
					{ // Here, ID translation
						CANid_bin_hex((char*)pmsg, (p2c+k)->out);
						return 1; // Copy in->out w ID change
					}
		        }
		        else if ((p2c+k)->in < id) 
		        {
		            i = k+1;
		        }
		        else 
		        {
		            j = k-1;
		        }
		    }
//printf("POM: NO match: %08X %s\n",id, pmsg);
		    return 0; // No match; Do not copy in->out
		}
	}

	/* Type = Block-on-match */
	if (pbnn->size_1c == 0)
	{ // Here, lookup table has no entries (pass all)
		if (pbnn->size_2c == 0)
		{ // Here, translation table has no entries
			return 1; // Copy without ID change
		}
		else
		{ // Here, translation table needs a lookup
			id = CANid_hex_bin((char*)(pmsg+2)); // Get CAN id in binary
			if (id == 0)
				return 0; // Non-hex in CAN id. Do not copy in->out

//printf("BOM: size_2c %3i %s\n",pbnn->size_2c,pmsg);
p2c = pbnn->p2c;
//for(int kk=0; kk<pbnn->size_2c; kk++)			
//	printf("BZ %2i %08X %08X\n",kk,(p2c+kk)->in,(p2c+kk)->out);

			// binary search 2 column table
			p2c = pbnn->p2c;
		    i = 0; j = (pbnn->size_2c - 1);
		    while (i <= j) 
		    {
		        k = i + ((j - i) / 2);
		        if ((p2c+k)->in == id) 
		        { // Here, found
					if ((p2c+k)->out == 0)
					{ // Here, ID is not translated
						return 1; // Copy in->out, no ID change
					}
					else
					{ // Here, ID translation. Insert new ID
						CANid_bin_hex((char*)pmsg, (p2c+k)->out);
						return 1; // Copy in->out w ID change
					}
		        }
		        else if ((p2c+k)->in < id) 
		        {
		            i = k+1;
		        }
		        else 
		        {
		            j = k-1;
		        }
		    }
		    return 0; // No match; Do not copy in->out
		}
	}
	else
	{ // Here, lookup table has entries
		id = CANid_hex_bin((char*)(pmsg+2)); // Get CAN id in binary
		if (id == 0)
			return 0; // Non-hex in CAN id. Do not copy in->out

//printf("BOM: size_1c %3i %s\n",pbnn->size_1c,pmsg);
//p1c = pbnn->p1c;
//for(int kk=0; kk<pbnn->size_1c; kk++)			
//	printf("B1 %2i %08X\n",kk,*(p1c+kk));


		// binary search 2 column table
		p1c = pbnn->p1c;
	    i = 0; j = (pbnn->size_1c - 1);
	    while (i <= j) 
	    {
	        k = i + ((j - i) / 2);
	        if (*(p1c+k) == id) 
	        { // Here, found
	        	return 0; // Do not copy in->out
	        }
	        else if (*(p1c+k) < id) 
	        {
	            i = k+1;
	        }
	        else 
	        {
	            j = k-1;
	        }
	    }
	    // Here, not found, check translation table before passing
//printf("B2: size_2c %3i %s\n",pbnn->size_2c,pmsg);
//p2c = pbnn->p2c;
//for(int kk=0; kk<pbnn->size_2c; kk++)			
//	printf("DD %2i %08X %08X\n",kk,(p2c+kk)->in,(p2c+kk)->out);

		// binary search 2 column table
		p2c = pbnn->p2c;
	    i = 0; j = (pcbf->n - 1);
	    while (i <= j) 
	    {
	        k = i + ((j - i) / 2);
	        if ((p2c+k)->in == id) 
	        { // Here, found. Insert new ID
	        	CANid_bin_hex((char*)pmsg, (p2c+k)->out);
				return 1;
	        }
	        else if ((p2c+k)->in < id) 
	        {
	            i = k+1;
	        }
	        else 
	        {
	            j = k-1;
	        }
	    }
	    // Here, not found.
	    return 1; // Copy in->out, no ID change
	}
	/* Here--should not happen!. */
	exit (-2);
}

