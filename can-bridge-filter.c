/*******************************************************************************
* File Name          : can-bridge-filter.c
* Date First Issued  : 05/16/2024
* Board              : 
* Description        : Setup bridging data from input file
*******************************************************************************/
/* 
struct CBF_TABLES* can_bridge_filter_init(FILE *fp)
- This routine reads lines from an input file and builds the bridging/filtering
tables.
- Input file format: see CANfilter3x3.txt comment lines
- Format checks are made on the file.
- Return is either:
  NULL - failure of some type
  pointer to "root" struct that contains
  - pointer to array of structs for in:out tables
  - size of matrix, "N"

Strategy:
For each in:out connection pair, two types of tables are created. One with a 
"single column", i.e. uint32_t array and another array with "two columns" with 
a struct that has "in" and "out" uint32_t elements.

These tables for an in:out pair are added to a temporarly arrays on the stack
while the input is being read. Upon completion of an in:out pair memory is
'calloc'd and the temporary array copied, then sorted.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "common_can.h"
#include "../../GliderWinchCommons/embed/svn_common/trunk/db/gen_db.h"
#include "can-bridge-filter.h"

//#define DBGINPT // Print IDs extracted from input file
//#define DBGPTBL // Old style print of tables
//#define DBGNSRT // Include not-sorted table print out

#define LINESZ 256 // Input line max size

/* Pointer and matrix size. */
//struct CBF_TABLES cbf_tables;

/* Row:Column */
struct ROWCOL
{
   uint8_t r; // Row
   uint8_t c; // Col
};
struct TMPTBL
{
   struct CBF2C id_2c[CBFARRAYMAX];
   struct ROWCOL rc_cur;
   struct ROWCOL rc_prev;
   struct ROWCOL rc_test;
   uint32_t id_1c[CBFARRAYMAX];
   struct CBFNxN* pbnn;
   uint8_t type;
   char buf[LINESZ];
   uint8_t otosw_n;
   uint16_t linectr;
   int8_t Ttsw;
   int8_t oto_at;
   int8_t otosw;

};
static void printatline(struct TMPTBL* pt);
static void printatlinebuf(struct TMPTBL* pt);
/*******************************************************************************
 * static int size_1c_inc(struct TMPTBL* pt);
 * @brief   : Increment and check size of 1 column array 
 * @return  : -1, 0, +1
*******************************************************************************/
static int size_1c_inc(struct TMPTBL* pt)
{
   pt->pbnn->size_1c += 1;
   if (pt->pbnn->size_1c >= CBFARRAYMAX)
   {
      printf("ERR: EGADS! Number of CAN IDs %i too large: 1 column table %i %i",\
         pt->pbnn->size_1c, pt->rc_test.r, pt->rc_test.c);printatline(pt);
      return -1;
   }
   return 0;
}
/*******************************************************************************
 * static int size_2c_inc(struct TMPTBL* pt);
 * @brief   : Increment and check size of 1 column array 
 * @return  : -1, 0, +1
*******************************************************************************/
static int size_2c_inc(struct TMPTBL* pt)
{
   pt->pbnn->size_2c += 1;
   if (pt->pbnn->size_2c >= CBFARRAYMAX)
   {
      printf("ERR: EGADS! Number of CAN IDs %i too large: 2 column table %i %i",\
         pt->pbnn->size_2c, pt->rc_test.r, pt->rc_test.c);printatline(pt);
      return -1;
   }
   return 0;
}
/*******************************************************************************
 * static int cmpfuncS (const void * a, const void * b);
 * @brief   : Compare function "struct" for qsort and bsearch (see man pages)--using direct access
 * @return  : -1, 0, +1
*******************************************************************************/
static int cmpfuncS (const void * a, const void * b)
{
   uint32_t aa =((struct CBF2C*)a)->in;
   uint32_t bb =((struct CBF2C*)b)->in;
   return (aa > bb) - (aa < bb);
}
/*******************************************************************************
 * static int cmpfunc(const void * a, const void * b);
 * @brief   : Compare function "uint32_t" for qsort and bsearch (see man pages)--using direct access
 * @return  : -1, 0, +1
*******************************************************************************/
static int cmpfunc(const void * a, const void * b)
{
   return (*(uint32_t*)a > *(uint32_t*)b) - (*(uint32_t*)a < *(uint32_t*)b);
}
/*******************************************************************************
 * static int oto_matrix(struct TMPTBL* pt);
 * @brief   : Check for matrix size 'n' encountered before any other data
 * @return  : exit if fail
*******************************************************************************/
static void oto_matrix(struct TMPTBL* pt)
{
   if (pt->otosw_n == 0)
   {
      printf("ERR: matrix size not yet set ");
      printatlinebuf(pt);
      exit (-1);
   }
}
/* **************************************************************************************
 * static int tmptbl_init(struct TMPTBL* pt);
 * @brief   : Initialize struct for temporary table
 * @param   : pt = pointer temporary table on stack
 * @return  : 0 = success; -1 = fail 
 * ************************************************************************************** */
static int tmptbl_init(struct TMPTBL* pt)
{
   struct ROWCOL rcx; rcx.r = 1; rcx.c = 0;
   pt->rc_cur = rcx;
   pt->rc_prev =rcx;
   pt->rc_test =rcx;
   pt->otosw_n = 0;
   pt->linectr = 1;
   pt->Ttsw = 0;
   pt->oto_at = 0;
   pt->otosw = 0;
   return 0;
}
/*******************************************************************************
 * static void printatline(struct TMPTBL* pt);
 * @brief   : Print line number (where ERR was found)
 * @param   : pt = pointer to struct on stack
*******************************************************************************/
static void printatline(struct TMPTBL* pt)
{
   printf(" at line %i\n",pt->linectr);
   return;
}
/*******************************************************************************
 * static void printatlinebuf(struct TMPTBL* pt);
 * @brief   : Print line number (where ERR was found) and input line
 * @param   : pt = pointer to struct on stack
*******************************************************************************/
static void printatlinebuf(struct TMPTBL* pt)
{
   printf(" at line %i\n%s",pt->linectr,pt->buf);
   return;
}
/*******************************************************************************
 * static void printtbl_1c(uint32t* p, uint16_t size);
 * @brief   : Print table
 * @param   : p = pointer to array[0]
 * @param   : size = size
*******************************************************************************/
#ifdef DBGPTBL
static void printtbl_1c(uint32_t* p, uint16_t size)
{
   for (int i=0; i<size; i++) printf("%3i %08X\n",(i+1),*p++);
   return;
}
#endif
/*******************************************************************************
 * static void printtbl_2c(struct CBF2C* p, uint16_t size);
 * @brief   : Print table
 * @param   : p = pointer to array[0]
 * @param   : size = size
*******************************************************************************/
#ifdef DBGPTBL
static void printtbl_2c(struct CBF2C* p, uint16_t size)
{
   for (int i=0; i<size; i++) 
   {
      printf("%3i %08X  %08X\n",(i+1),p->in,p->out);
      p += 1;
   }
   return;
}
#endif
/*******************************************************************************
 * static void printinput(char a, char* pbuf, uint32_t id);
 * @brief   : Print input name with extracted ID
 * @param   : Leading char
 * @param   : p = pointer to input line
 * @param   : id = extracted id
 * @param   : size = size
*******************************************************************************/
static void printinput(char a, char* pbuf, uint32_t id)
{
   #define ZZ 30
   printf("%c %08X ",a, id);
   /* Extract name */
   char* p = strchr(pbuf, '\'');
   char tmp[ZZ];
   char fill;
   if ((a == 't') || (a == 'I'))
      fill = ' ';
   else
      fill = '.';
   memset(tmp, fill,ZZ);

   char* pp = strchr((p+1),'\'');
   strncpy(tmp,p, (pp - p)+1);
   tmp[ZZ-1] = 0;
   printf("%s",tmp);
   return;
}
/*******************************************************************************
 * static void printpassonmatch( \
     struct CBF2C*   p2c, uint16_t size_2c, \
     uint8_t connection_in, \
     uint8_t connection_out);
 * @brief   : Print table
 * @param   : p1c = pointer to 2 column struct array[0]
 * @param   : size_1c = size of 2 column array
 * @param   : connection in  (1 - n)
 * @param   : connection out (1 - n)
********************************************************************************/
static void printpassonmatch( \
     struct CBF2C*   p2c, uint16_t size_2c, \
     uint8_t connection_in, \
     uint8_t connection_out)
{
   int i;
   if (size_2c == 0)
   {
      printf(" ### EMPTY PASS-ON-MATCH will ==> BLOCK ALL <==\n");
   }
   else
   {
     // Here, pass-on-match. Single two column table
      printf("##### PASS-ON-MATCH ###### size_1c %i\n",size_2c);
      printf("    PASS ON  TRANS (if not zero)\n");
      for (i = 0; i < size_2c; i++)
      {
         printf("%3i %08X %08X\n",(i+1), (p2c+i)->in, (p2c+i)->out);
      }
   }
   printf("##########################\n");
   return;
}
/*******************************************************************************
 * static void printblockonmatch( \
     uint32_t*       p1c, uint16_t size_1c, \
     struct CBF2C*   p2c, uint16_t size_2c, \
     uint8_t connection_in, \
     uint8_t connection_out);
 * @brief   : Print side-by-side tables for 2 col and 1 col
 * @param   : p1c = pointer to 1 column array[0]
 * @param   : size_1c = size of 1 column array
 * @param   : p1c = pointer to 2 column struct array[0]
 * @param   : size_1c = size of 2 column array
 * @param   : connection in  (1 - n)
 * @param   : connection out (1 - n)
********************************************************************************/
static void printblockonmatch( \
   uint32_t*      p1c, uint16_t size_1c, \
   struct CBF2C*  p2c, uint16_t size_2c, \
   uint8_t connection_in, \
   uint8_t connection_out)
{
   int i;
   int sz;
   if (size_1c > size_2c)
      sz = size_1c;
   else
      sz = size_2c;

  // Here, two tables side-by-side
   printf("##### BLOCK-ON-MATCH ###### size_2c %i, size_1c %i row %i col %i\n",
      size_2c, size_1c,
      connection_in,connection_out);
   printf("Trans   IN      OUT      BLOCK ON\n");
   for (i = 0; i < sz; i++)
   {
      if ( i < size_2c)
         printf("%3i %08X %08X",(i+1), (p2c+i)->in, (p2c+i)->out);
      else
         printf(".....................");
      if (i < size_1c)
      {
         printf("%3i %08X",(i+1), *(p1c+i));
      }
      else
         printf("  ..........");

      printf("\n");
   }
   printf("##########################\n");
   return;
}

/* **************************************************************************************
 * static void rc_inc(struct ROWCOL* prc);
 * @brief   : Increment row:col 
 * @param   : pt = pointer row|col pair
 * ************************************************************************************** */
static void rc_inc(struct ROWCOL* prc, uint8_t n)
{
   prc->c += 1; // Increment column
   if (prc->c > n)
   {
      prc->c  = 1; // Set column to beginning
      prc->r += 1; // Increment row
   }
   return;
}
/* **************************************************************************************
 * static int rc_cmp(struct ROWCOL* prc1,struct ROWCOL* prc2 );
 * @brief   : Compare two row:col 
 * @param   : prc1 = pointer row|col pair
 * @param   : prc1 = pointer row|col pair
 * @return  : 0 = equal; + = prc1 > prc2; - = prc1 < prc2;
 * ************************************************************************************** */
static int rc_cmp(struct ROWCOL* prc1,struct ROWCOL* prc2 )
{
   int16_t r1 = (prc1->r << 8) | (prc1->c);
   int16_t r2 = (prc2->r << 8) | (prc2->c);
   return (r1 - r2);
}

/* **************************************************************************************
 * static int close_table(struct TMPTBL* pt);
 * @brief   : New table line closes curent table being built
 * @param   : pt = pointer temporary table on stack
 * @return  : 0 = success; -1 = fail 
 * ************************************************************************************** */
static int close_table(struct TMPTBL* pt)
{  
   struct CBF2C* pel;
   struct CBFNxN* pbnn = pt->pbnn;
   uint32_t* pui;

   /* First time, there is no previous table to close. */
   if(pt->otosw == 0)
   {
      pt->otosw = 1;
      pt->rc_prev = pt->rc_cur;
      return 0;
   }

// Debugging print of tables
printf("##### prev %i %i size_1c: %i size_2c: %i type: %i\t",pt->rc_prev.r,pt->rc_prev.c,
   pt->pbnn->size_1c,pt->pbnn->size_2c,pt->pbnn->type);
printf("cur %i %i size_1c: %3i          \n",pt->rc_cur .r,pt->rc_cur.c,pt->pbnn->size_1c);

if (pbnn->type == 0)
{ // Here, pass-on-match

#ifdef DBGPTBL   
   printf("...... PASS-ON-MATCH ....... size: %i\n",pbnn->size_2c);
   printtbl_2c(&pt->id_2c[0], pbnn->size_2c);
   printf(".......................\n");
   printpassonmatch(&pt->id_2c[0], pbnn->size_2c, pt->rc_prev.r, pt->rc_prev.c);   
#endif   
}
else
{ // Here, block-on-match
#ifdef DBGNSRT   
   printf("...... BLOCK-ON-MATCH 1C lookup ....... size: %i\n",pbnn->size_1c);
   printf("...... BLOCK-ON-MATCH 2c translation... size: %i\n",pbnn->size_1c);
//printtbl_2c(&pt->id_2c[0], pbnn->size_2c);   
   printblockonmatch(&pt->id_1c[0],pbnn->size_1c,&pt->id_2c[0],pbnn->size_2c,pt->rc_prev.r,pt->rc_prev.c);
   printf(".......................\n");
#endif   
}
   /* Both pass-on-match and block-on-match have possible 2 column tables. */
   /* If size is zero, then no table is needed. */   
   if (pbnn->size_2c != 0)
   {  /* Allocate memory for table. */
      pel = calloc(pbnn->size_2c, sizeof(struct CBF2C));
      if (pel == NULL)
      {
         printf("ERR: calloc for translation table failed\n");
         return -1;
      }
      pbnn->p2c = pel; // Save ptr to beginning of table

#ifdef DBGPTBL
printf("...... INPUT 2 COLUMN ....... size: %i\n",pbnn->size_2c);
printtbl_2c(&pt->id_2c[0], pbnn->size_2c);
printf(".......................\n");
#endif

      /* Copy temporary list to allocated memory location. */
      memcpy(pbnn->p2c, &pt->id_2c[0], (pbnn->size_2c * sizeof(struct CBF2C)));      

      /* Sort incoming CAN ID column for later bsearch */
      if (pbnn->size_2c > 1)
      {
         qsort(pbnn->p2c, pbnn->size_2c, sizeof(struct CBF2C), cmpfuncS);// Sort for bsearch'ing
      }

   }
   else
   pbnn->p2c = NULL; // Empty table

   /* Block-on-match type has one 1 column and one 2 column table. 
      Pass-on-match does not need a 1 column list.
      The 2 column table was handled above, so do the 1 column table. */

   if (pbnn->type != 0)
   { // Here, type is block-on-match
      /* If size is zero, then no 1 column block-on-match lookup table is needed. */
      if (pbnn->size_1c != 0)
      {  /* Allocate memory for table. */
         pui = calloc(pbnn->size_1c, sizeof(uint32_t));
         if (pui == NULL)
         {
            printf("ERR: calloc for block:pass table failed\n");
            return -1;
         }
         pbnn->p1c = pui; // Save ptr to beginning of table
#ifdef DBGPTBL
   printf("****** INPUT 1 COLUMN ******* _size_1c: %i type: %i\n",pbnn->size_1c, pbnn->type);
   printtbl_1c(&pt->id_1c[0], pbnn->size_1c);
   printf("***********************\n");
#endif   

         /* Copy tenporary list to allocated memory location. */
         memcpy(pbnn->p1c, &pt->id_1c[0], (pbnn->size_1c * sizeof(uint32_t)));
    
          /* Sort CAN ID's for later bsearch */
         if (pbnn->size_1c > 1)
         {
            qsort(pbnn->p1c, pbnn->size_1c, sizeof(uint32_t), cmpfunc);// Sort for bsearch'ing
         }  
      }
      else
         pbnn->p1c = NULL; // Empty table
   }

#ifdef DBGPTBL
printf("****** SORTED 1 COLUMN ****** size: %i\n",pbnn->size_1c);
printtbl_1c(pbnn->p1c, pbnn->size_1c);
printf("***********************\n");
printf("...... SORTED 2 COLUMN ...... size: %i\n",pbnn->size_2c);
printtbl_2c(pbnn->p2c, pbnn->size_2c);
printf(".......................\n");
#endif

// Debugging print of tables
printf("##### prev %i %i size_1c: %i size_2c: %i type: %i\t", pt->rc_prev.r, pt->rc_prev.c,
   pbnn->size_1c, pbnn->size_2c, pbnn->type);
printf("cur %i %i size_1c: %3i          \n",pt->rc_cur .r,pt->rc_cur.c,pt->pbnn->size_1c);
if (pbnn->type == 0)
{ // Here, pass-on-match
   printf("SORTED PASS-ON-MATCH ....... size: %i\n",pbnn->size_2c);
//printtbl_2c(pbnn->p2c, pbnn->size_2c);
   printpassonmatch(pbnn->p2c, pbnn->size_2c, pt->rc_prev.r, pt->rc_prev.c);
   printf(".......................\n");
}
else
{ // Here, block-on-match
   printf("SORTED BLOCK-ON-MATCH 1C lookup ....... size: %i\n",pbnn->size_1c);
   printf("SORTED BLOCK-ON-MATCH 2c translation... size: %i\n",pbnn->size_2c);
//printtbl_2c(pbnn->p2c, pbnn->size_2c);   
   printblockonmatch(pbnn->p1c,pbnn->size_1c,pbnn->p2c,pbnn->size_2c,pt->rc_prev.r,pt->rc_prev.c);
   printf(".......................\n");
}
   return 0;
}
/* **************************************************************************************
 * static int extract_id(uint32_t* pid, char* p);
 * @brief   : Extract CAN ID, given a copy of a line from CANID_INSERT.sql file
 * @param   : pid = pointer to CAN ID extracted; 0 = failure
 * @param   : p  = pointer to input line
 * @return  :  0 = success; 
 *          : -1 = no comma
 *          : -2 = no apostrophe ahead of CAN ID
 *          : -3 = CAN ID field not valid hex
 * ************************************************************************************** */
static int extract_id(uint32_t* pid, char* p)
{
   uint32_t id;
   char* pp = strchr(p,','); // First occurence of ','
   if (pp == NULL)
   {
      printf("Extract CAN ID fail: no comma.\n");
      *pid = 0;
      return -1;
   }
   pp = strchr(pp,'\'');
   if (pp == NULL) // Expect \' ahead of CAN ID
   {
      printf("Extract CAN ID fail: no \' ahead of ID.\n");
      *pid = 0;
      return -2;      
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
      *pid = 0;
      return -3;       
   }
   *pid = id; 
//printf("& %08X\n",id);
   return 0; // Success
}
/* **************************************************************************************
 * struct CBF_TABLES* can_bridge_filter_init(FILE *fp);
 * @brief   : Read file, extract IDs, and build filter table
 * @param   : fp = file pointer to filter table
 * @return  : pointer to base table struct; NULL = failed
 * ************************************************************************************** */
 struct CBF_TABLES* can_bridge_filter_init(FILE *fp)
 {
   uint32_t itmp;
   uint32_t tmpid;
 
   /* Build each table here, then malloc and copy. */
   struct TMPTBL tmptbl;
   struct TMPTBL* pt = &tmptbl; // Convenience ptr
   tmptbl_init(pt); // Temporary table init

   struct CBF_TABLES* pcbf_tables = calloc(1,sizeof(struct CBF_TABLES));
   if (pcbf_tables == NULL)
   {
      printf("ERR: EGADS! calloc failed for only %li bytes!\n",sizeof(struct CBF_TABLES));
      return NULL;
   }

   /* Read in table */
   while ( (fgets (&pt->buf[0],LINESZ,fp)) != NULL)  // Get a line
   {
//printf("%4i:%s",pt->linectr,pt->buf);
      switch (pt->buf[0]) // First char of line identifies line type
      {
      case ' ': // Skip. Assume a blank line
      case '#': // Skip comment line
      case '\n': // Skip blank line
         break;
      case '@': // Matrix size 'n' (2 >= n <= CBFNxNMAX)
         if (pt->oto_at != 0)
         { // Here matrix size already defined!
            printf("ERR: Multiple @ (matrix size) definitions");printatline(pt);
            return NULL;
         }
         pt->oto_at = 1;
         if (((pt->buf[1] - '0') < 2) || ((pt->buf[1] - '0') >= CBFNxNMAX) )
         { // Here, matrix size is out-of-bounds
            printf("ERR: Table matrix size %c must be .GE.2 and .LE. %i",\
               pt->buf[1], CBFNxNMAX);printatline(pt);
            return NULL;
         }
         /* KEY parameter is matrix size */
         pcbf_tables->n = (pt->buf[1] - '0'); // Matrix size
         pt->otosw_n = 1; // Matrix size has been set

         /* Allocate memory for n*n CBFNxN structs */
         pcbf_tables->pnxn = calloc((pcbf_tables->n * pcbf_tables->n),sizeof(struct CBFNxN));
         if (pcbf_tables->pnxn == NULL)
         {
            printf("ERR: Failure to calloc %i instances of struct CBFNxN",\
               (pcbf_tables->n * pcbf_tables->n));printatline(pt);
            return NULL;
         }
         /* Next: expect to load, in sequence, tables. */
         break;

      case '%': // New In (nn) -> Out (mm) table
         /* Check "%nm t" (row col type) numbers are within bounds */
         pt->rc_cur.r = (pt->buf[1] - '0'); // row
         pt->rc_cur.c = (pt->buf[2] - '0'); // col
         if ((pt->rc_cur.r < 1) || (pt->rc_cur.r > pcbf_tables->n) ||
             (pt->rc_cur.c < 1) || (pt->rc_cur.c > pcbf_tables->n)  )
         {   
            if ((pt->rc_cur.r == 9) && (pt->rc_cur.c == 9))
            { // Here, end of table
               printf("END-of-TABLE\n");
               break;
            }
            else
            {
               printf ("ERR: row %i col %i, 1st and 2nd char not within matrix space,",\
                  pt->rc_cur.r, pt->rc_cur.c);printatline(pt);
               return NULL;
            }
         }
         if  ((pt->buf[3]) != ' ')
         {   
            printf ("ERR: row %i col %i, 4th char not a space",\
               pt->rc_cur.r, pt->rc_cur.c);printatline(pt);
            return NULL;
         }
         /* Type of table (pass or block) */
         pt->type = (pt->buf[4] - '0'); // Set type code
         if ((pt->type < 0 ) || (pt->type > 1))
         {
            printf ("ERR: Table type 4th char %i not 0 or 1",pt->type);printatlinebuf(pt);
            return NULL;            
         }
         /* Here, '%' line numbers are within bounds */
         /* Check that it is in sequence. */
         rc_inc(&pt->rc_test,pcbf_tables->n); // Increment row:col
         if (rc_cmp(&pt->rc_cur, &pt->rc_test) != 0)
         {
            printf ("ERR: Table not in sequence: %i %i, expected %i %i",\
               pt->rc_cur.r,  pt->rc_cur.c,\
               pt->rc_test.r, pt->rc_test.c);printatlinebuf(pt);
            return NULL;                        
         }
         /* Close previous table */
         close_table(pt);

         /* Compute pointer to struct array for this in:out pair. */
         pt->pbnn = pcbf_tables->pnxn;
         pt->pbnn += ((pt->rc_cur.r-1)*pcbf_tables->n) + (pt->rc_cur.c-1);
         /* Build sizes, and save type in array struct. */
         pt->pbnn->type = pt->type;
         pt->pbnn->size_1c = 0; // Block-on-match table size
         pt->pbnn->size_2c = 0; // Pass-on-match or block-on-pass or translate table size
         pt->rc_prev = pt->rc_cur; // Save for later close out of table
         break;

      case 'I': // Not a translated ID. Expect a line from CANID_INSERT.sql file
         oto_matrix(pt);
         if ((itmp=extract_id(&tmpid,pt->buf)) != 0)
         {
            printf("ERR: AN ID extraction failed with code %d",\
               itmp);printatlinebuf(pt);
            return NULL;
         }
#ifdef DBGINPT 
printf("$ %08X\n",tmpid); 
#endif
printinput('I',&pt->buf[0],tmpid); printf("\n");

         /* table type determines 1 or 2 column array. */
         if (pt->type == 0)
         { // Here, type: pass-on-match & 2nd column has translation id
            pt->id_2c[pt->pbnn->size_2c].in  = tmpid;
            pt->id_2c[pt->pbnn->size_2c].out = 0;
            if (size_2c_inc(pt) < 0) // Increment and check size_1c
              return NULL;
         }
         else
         { // Here, type: block-on-match 1 column; separate 2 column translation table
            pt->id_1c[pt->pbnn->size_1c] = tmpid;
            if (size_1c_inc(pt) < 0) // Increment and check size_1c
               return NULL;
         }
         break;

      case 'T': // 1st of translated pair, the incoming ID
         oto_matrix(pt);
         if (pt->Ttsw == 1) 
         {
            printf("ERR: Expecting \'t\' but got T");printatlinebuf(pt);
            return NULL;     
         }
         if ((itmp=extract_id(&tmpid, &pt->buf[2])) != 0)
         {
            printf("ERR: AN ID extraction failed with code %d",\
               itmp);printatlinebuf(pt);
            return NULL;
         }
#ifdef DBGINPT 
printf("T %08X ",tmpid); 
#endif   
printinput('T',&pt->buf[0],tmpid);

         pt->id_2c[pt->pbnn->size_2c].in = tmpid;
         pt->Ttsw = 1; // Expect next line to start with 't'
         break;

      case 't': // 2nd of translated pair, the outgoing ID
         oto_matrix(pt);
         if (pt->Ttsw != 1) 
         {
            printf("ERR: Expecting \'T\' but got T");printatlinebuf(pt);
            return NULL;     
         }
         pt->Ttsw = 0; // Expect next translation line to start with 'T' or 'I'
         if ((itmp=extract_id(&tmpid, &pt->buf[2])) != 0)
         {
            printf("ERR: AN ID extraction failed with code %d",\
               itmp);printatlinebuf(pt);
            return NULL;
         }
#ifdef DBGINPT 
printf("t %08X %3i\n",tmpid,pt->pbnn->size_2c);
#endif
printinput('t',&pt->buf[0],tmpid); printf("\n");

         pt->id_2c[pt->pbnn->size_2c].out = tmpid;
         if (size_2c_inc(pt) < 0)
            return NULL;
         break;

      default:
         printf("ERR: First char on line not recognized");printatlinebuf(pt);
         return NULL;
      } 
     pt->linectr += 1; // Count input file lines
   }
   /* END of .txt file. */
   close_table(pt); // Complete last table

   return pcbf_tables; // Success
}
/*******************************************************************************
 * void printtablesummary(struct CBF_TABLES* pcbf);
 * @brief   : print table summary
 * @param   : pcbf = pointer to beginning struct for tables
*******************************************************************************/
const char* ptype0 = "0 Pass__on_match";
const char* ptype1 = "1 Block_on_match";

void printtablesummary(struct CBF_TABLES* pcbf)
{
   struct CBFNxN* pbnn = pcbf->pnxn;
   int N = pcbf->n;
   printf("\nSummary: matrix: %ix%i\n",N,N);
   int r,c;
   for (r = 0; r < N; r++)
   {
      for (c = 0; c < N; c++)
      {
 //        pbnn = pbnn1 + c;
         printf("%i %i  %3i:size_1c %3i:size_2c  ",r+1,c+1,pbnn->size_1c,pbnn->size_2c);
         if (pbnn->type == 0)
            printf("%s",ptype0);
         else
            printf("%s",ptype1);
         if (pbnn->p1c == NULL)
         {
            printf("  NULL");
         }
         else
         {
            printf("  TBLL %li",(pbnn->p1c - (uint32_t*)pcbf->pnxn));
         }
         if (pbnn->p2c == NULL)
         {
            printf(" NULL\n");
         }
         else
         {
            printf(" TBLL %li\n",(pbnn->p2c - (struct CBF2C*)pcbf->pnxn));
         }
         pbnn += 1;
      }
   }
   return;
}
