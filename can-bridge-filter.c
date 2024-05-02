/*******************************************************************************
* File Name          : can-bridge-filter.c
* Date First Issued  : 04/30/2024
* Board              : 
* Description        : File for bridging
*******************************************************************************/
/* Command line:
   can-bridge <can#1> <file#1> <can#2> <file#2>
   can-bridge can0 gate0-1 can1 gate1-0

file format
# = comment
% <bridge singles table mode>
0 pass in->out only on match
1 block in->out only on match
2 pass all in->out
3 block all in->out

<ascii hex CAN id name from CANID_INSERT/sql file> 
[Copy line from CANID_INSERT.sql; add #define and '//']
#define CANID_UNIT_BMS18 //'B2A00000','UNIT_BMS18', 1,1,'U8_U8_U8_X4','BMS ADBMS1818 #16');

*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


#include "common_can.h"
#include "../../GliderWinchCommons/embed/svn_common/trunk/db/gen_db.h"

#define LINESZ 128
char buf[LINESZ];

#define CANIDTBLSIZE 512 // Max number of CAN IDs 
struct CANIDTBL
{
   uint32_t idtbl[CANIDTBLSIZE];  // CAN ID
   uint16_t idsort[CANIDTBLSIZE]; // Index for sorted CAN ID
   uint16_t size;                 // Number of table entries
   uint8_t  code; // Filter rule: First char '%' next char 0-3    
      // 0 = table match: pass
      // 1 = table match: block
      // 2 = pass all;
      // 3 = block all;

}canidtbl[2];
/* **************************************************************************************
 * static int extract_id(uint32_t id, char* p);
 * @brief   : Extract CAN ID, given a copy of a line from CANID_INSERT.sql file
 * @brief   : id = CAN ID extracted
 * @param   : p = pointer to line
 * @return  : 0 = success; not 0 = something wrong
 * ************************************************************************************** */
static int extract_id(uint32_t id, char* p)
{
   char* pp = strchr(p,','); // First occurence of ','
   if (pp == NULL)
   {
      printf("Extract CAN ID fail: no comma.\n");
      return -1;
   }
   pp += 1;
   if (*pp != '\'')
   {
      printf("Extract CAN ID fail: no '' ahead of ID.\n");
      return -1;      
   }
   // Check for all hex in ID field
   pp += 1;
   *(pp+8) = 0; // ID must be exactly 8 chars and valid hex
   if (pp[strspn(pp, "0123456789abcdefABCDEF")] == 0)
   { // Here, valid hex
      sscanf(pp,"%X",&id); // Convert hex
   }
   else
   {
      printf("Extract CAN ID fail: CAN ID field not valid hex.\n");
      return -1;       
   }
   return 0; // Success
}
/* **************************************************************************************
 * int can_bridge_filter_init(FILE *fp, uint8_t tbl);
 * @brief   : Read file, extract IDs, and build filter table
 * @param   : fp = file pointer
 * @param   : tble index: 0 or 1
 * @return  : ?
 * ************************************************************************************** */
 int can_bridge_filter_init(FILE *fp, uint8_t tbl)
 {
   int i,j;
   uint16_t size = 0;
   uint8_t tmp;

   /* Read in table */
   canidtbl[tbl].code = 0;
   while ( (fgets (&buf[0],LINESZ,fp)) != NULL)  // Get a line
   {
      if (buf[0] == '#') continue; // Skip comment lines
      else if (buf[0] == ' ') continue;
      else if (buf[0] == '%')
      { // Here, code for filtering rule
         tmp = buf[1] - '0';
         if (tmp > 3)
         {
            printf("Table code %s is not 0-3\n",buf);
            exit (-1);
         }
         else
         {
            canidtbl[tbl].code = tmp;
            continue;
         }
      }
      if (extract_id(canidtbl[tbl].idtbl[size], buf) != 0)
      {
         printf("CAN ID extraction failed\n");
         exit(-1);
      }
      size += 1;
      if (size >= CANIDTBLSIZE)
      {
         printf("Table %i:Number of CAN IDs too large\n",tbl);
         exit (-1);
      }
   }
   canidtbl[tbl].size = size;

   /* DEBUG: Print table */
   for (i = 0; i < 2; i++)
   {
      printf("\nTable %i size %i\t",tbl,size);
      switch (canidtbl[i].code)
      {
      case 0:
         printf("PASS on MATCH\n");
         break;
      case 1:
         printf("BLOCK on MATCH\n");
         break;
      case 2:
         printf("PASS ALL\n");
         break;
      case 3:
         printf("BLOCK ALL\n");
      }
      if (canidtbl[i].code < 2)
      {
         for (j = 0; j < size; j++)
         {
            printf ("%3i %08X\n",j,canidtbl[tbl].idtbl[j]);
         }
      }
   }

   /* Sort can ids. */
   return 0;
 }