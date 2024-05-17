/*******************************************************************************
* File Name          : can-bridge-filter.c
* Date First Issued  : 05/16/2024
* Board              : 
* Description        : Setup bridging data from input file
*******************************************************************************/
/* 
file format: see CANfilter3x3.txt comment lines
*/
//ptbl = bsearch(&cantblx.id, &cantbl[0], cantblsz, sizeof(uint32_t), cmpfunc);
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <malloc.h>

#include "common_can.h"
#include "../../GliderWinchCommons/embed/svn_common/trunk/db/gen_db.h"
#include "can-bridge-filter.h"

#define LINESZ 256

/* Pointer and matrix size. */
struct CBF_TABLES cbf_tables;

/* Row:Column */
struct ROWCOL
{
   uint8_t r; // Row
   uint8_t c; // Col
};

struct TMPTBL
{
   struct CBFELEMENT cbfelement[CBFARRAYMAX];
   struct ROWCOL rc_cur;
   struct ROWCOL rc_prev;
   struct ROWCOL rc_test;
   uint32_t idarray[CBFIDARRAYMAX];
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
 * static int cmpfuncS (const void * a, const void * b);
 * @brief   : Compare function "struct" for qsort and bsearch (see man pages)--using direct access
 * @return  : -1, 0, +1
*******************************************************************************/
static int cmpfuncS (const void * a, const void * b)
{
   uint32_t aa =((struct CBFELEMENT*)a)->in;
   uint32_t bb =((struct CBFELEMENT*)b)->in;
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
 * static void printtbl_bp(uint32t* p, uint16_t size);
 * @brief   : Print table
 * @param   : p = pointer to array[0]
 * @param   : size = size
*******************************************************************************/
static void printtbl_bp(uint32_t* p, uint16_t size)
{
   for (int i=0; i<size; i++) printf("%3i %08X\n",(i+1),*p++);
   return;
}
/*******************************************************************************
 * static void printtbl_tx(struct CBFELEMENT* p, uint16_t size);
 * @brief   : Print table
 * @param   : p = pointer to array[0]
 * @param   : size = size
*******************************************************************************/
static void printtbl_tx(struct CBFELEMENT* p, uint16_t size)
{
   for (int i=0; i<size; i++) 
   {
      printf("%3i %08X  %08X\n",(i+1),p->in,p->out);
      p += 1;
   }
   return;
}
/* **************************************************************************************
 * static void rc_inc(struct ROWCOL* prc);
 * @brief   : Increment row:col 
 * @param   : pt = pointer row|col pair
 * ************************************************************************************** */
static void rc_inc(struct ROWCOL* prc)
{
   prc->c += 1; // Increment column
   if (prc->c > cbf_tables.n)
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
   struct CBFELEMENT* pel;
   struct CBFNxN* pbnn = pt->pbnn;
   uint32_t* pui;

   /* First time, there is no previous table to close. */
   if(pt->otosw == 0)
   {
      pt->otosw = 1;
      pt->rc_prev = pt->rc_cur;
      return 0;
   }

   pbnn->type_bp = pt->type;

printf("##### prev %i %i size_bp: %i size_tx: %i type: %i\t",pt->rc_prev.r,pt->rc_prev.c,
   pt->pbnn->size_bp,pt->pbnn->size_tx,pt->pbnn->type_bp);
printf("cur %i %i sizebp: %3i          \n",pt->rc_cur .r,pt->rc_cur.c,pt->pbnn->size_bp);

   /* If size is zero, then no block:pass lookup table is needed. */
   if (pbnn->size_bp != 0)
   {  /* Allocate memory for table. */
      pui = calloc(pbnn->size_bp, sizeof(uint32_t));
      if (pui == NULL)
      {
         printf("ERR: calloc for block:pass table failed\n");
         return -1;
      }
      pbnn->pbp = pui; // Save ptr to beginning of table

printf("****** INPUT BP ******* size: %i type: %i\n",pbnn->size_bp, pbnn->type_bp);
printtbl_bp(&pt->idarray[0], pbnn->size_bp);
printf("***********************\n");      

      /* Sort CAN ID's for later bsearch */
      if (pbnn->size_bp > 1)
      {
         qsort(&pt->idarray[0], pbnn->size_bp, sizeof(uint32_t), cmpfunc);// Sort for bsearch'ing
      }  
      /* Copy sorted list to allocated memory location. */
      memcpy(pbnn->pbp, &pt->idarray[0], (pbnn->size_bp * sizeof(uint32_t)));
   }
   else
      pbnn->pbp = NULL; // Empty table

   /* If size is zero, then no translation lookup table is needed. */   
   if (pbnn->size_tx != 0)
   {  /* Allocate memory for table. */
      pel = calloc(pbnn->size_tx, sizeof(struct CBFELEMENT));
      if (pel == NULL)
      {
         printf("ERR: calloc for translation table failed\n");
         return -1;
      }
      pbnn->ptx = pel; // Save ptr to beginning of table

printf("...... INPUT TX ....... size: %i\n",pbnn->size_tx);
printtbl_tx(&pt->cbfelement[0], pbnn->size_tx);
printf(".......................\n");

      /* Sort incoming CAN ID's for later bsearch */
      if (pbnn->size_tx > 1)
      {
         qsort(&pt->cbfelement[0], pbnn->size_tx, sizeof(struct CBFELEMENT), cmpfuncS);// Sort for bsearch'ing
      }
      
      /* Copy sorted list to allocated memory location. */
      memcpy(pbnn->ptx, &pt->cbfelement[0], (pbnn->size_tx * sizeof(struct CBFELEMENT)));      
   }
   else
      pbnn->ptx = NULL; // Empty table


printf("****** SORTED BP ****** size: %i\n",pbnn->size_bp);
printtbl_bp(pbnn->pbp, pbnn->size_bp);
printf("***********************\n");

printf("...... SORTED TX ...... size: %i\n",pbnn->size_tx);
printtbl_tx(pbnn->ptx, pbnn->size_tx);
printf(".......................\n");
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
   return 0; // Success
}
/* **************************************************************************************
 * struct CBF_TABLES* can_bridge_filter_init(FILE *fp);
 * @brief   : Read file, extract IDs, and build filter table
 * @param   : fp = file pointer to filter table
 * @return  : pointer to ready-to-use table struct
 * ************************************************************************************** */
 struct CBF_TABLES* can_bridge_filter_init(FILE *fp)
 {
   uint32_t itmp;
   uint32_t tmpid;
 
   /* Build each table here, then malloc and copy. */
   struct TMPTBL tmptbl;
   struct TMPTBL* pt = &tmptbl; // Convenience ptr
   tmptbl_init(pt);

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
         cbf_tables.n = (pt->buf[1] - '0'); // Matrix size
         pt->otosw_n = 1; // Matrix size has been set

         /* Allocate memory for n*n CBFNxN structs */
         cbf_tables.pnxn = calloc((cbf_tables.n*cbf_tables.n),sizeof(struct CBFNxN));
         if (cbf_tables.pnxn == NULL)
         {
            printf("ERR: Failure to calloc %i instances of struct CBFNxN",\
               (cbf_tables.n*cbf_tables.n));printatline(pt);
            return NULL;
         }
         /* Next: expect to load, in sequence, tables. */
         break;

      case '%': // New In (nn) -> Out (mm) table
         /* Check "%nm t" (row col type) numbers are within bounds */
         pt->rc_cur.r = (pt->buf[1] - '0'); // row
         pt->rc_cur.c = (pt->buf[2] - '0'); // col
         if ((pt->rc_cur.r < 1) || (pt->rc_cur.r > cbf_tables.n) ||
             (pt->rc_cur.c < 1) || (pt->rc_cur.c > cbf_tables.n)  )
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
         rc_inc(&pt->rc_test); // Increment row:col
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
         pt->pbnn = cbf_tables.pnxn;
         pt->pbnn += ((pt->rc_cur.r-1)*cbf_tables.n) + (pt->rc_cur.c-1);


         pt->rc_prev = pt->rc_cur; // Save close out of table
         pt->pbnn->size_bp = 0; // Block:pass table size
         pt->pbnn->size_tx = 0; // Translate table size
         break;

      case 'I': // Expect a line from CANID_INSERT.sql file
         oto_matrix(pt);
         if ((itmp=extract_id(&tmpid,pt->buf)) != 0)
         {
            printf("ERR: AN ID extraction failed with code %d",\
               itmp);printatlinebuf(pt);
            return NULL;
         }
         pt->idarray[pt->pbnn->size_bp] = tmpid;
         pt->pbnn->size_bp += 1;
         if (pt->pbnn->size_bp >= CBFIDARRAYMAX)
         {
            printf("ERR: EGADS! Number of CAN IDs %i too large: table %i %i",\
               pt->pbnn->size_bp, pt->rc_test.r, pt->rc_test.c);printatline(pt);
            return NULL;
         }
         break;

      case 'T': // Incoming CAN id to be translated
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
         pt->cbfelement[pt->pbnn->size_tx].in = tmpid;
         pt->Ttsw = 1; // Expect next line to start with 't'
         break;

      case 't': // Translated output id, paired with incoming id
         oto_matrix(pt);
         if (pt->Ttsw != 1) 
         {
            printf("ERR: Expecting \'T\' but got T");printatlinebuf(pt);
            return NULL;     
         }
         pt->Ttsw = 0; // Expect next translation line to start with 'T'
         if ((itmp=extract_id(&tmpid, &pt->buf[2])) != 0)
         {
            printf("ERR: AN ID extraction failed with code %d",\
               itmp);printatlinebuf(pt);
            return NULL;
         }
         pt->cbfelement[pt->pbnn->size_tx].out = tmpid;
         pt->pbnn->size_tx += 1;
         if (pt->pbnn->size_tx >= CBFARRAYMAX)
         {
            printf("ERR: EGADS! Number of CAN IDs %i too large: table %i %i",\
               CBFARRAYMAX, pt->rc_cur.r, pt->rc_cur.c);printatline(pt);
            return NULL;
         }
         break;

      default:
         printf("ERR: First char on line not recognized");printatlinebuf(pt);
         return NULL;
      } 
     pt->linectr += 1; // Count input file lines
   }
   /* END of .txt file. */
   close_table(pt); // Complete last table


// TODO debug listing of tables.

   return &cbf_tables;
}