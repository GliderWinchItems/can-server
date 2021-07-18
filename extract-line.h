/*******************************************************************************
* File Name          : extract-line.h
* Date First Issued  : 07/14/2021
* Board              : 
* Description        : Add incoming chars to buffer, build output line
*******************************************************************************/

#ifndef  __EXTRACT_LINE_H
#define __EXTRACT_LINE_H

/* **************************************************************************************/
 void extract_line_add(char *pin, int n);
/* @brief	: Add chars from 'read()' to big buffer and build output line
 * @param	: piin = pointer to input chars from 'read'()' (not used if n = 0)
 * @param	: n = number of chars in input
 * ************************************************************************************** */
 char *extract_line_get(void);
/* @brief	: Attempt to extract a line from the buffer 
 * @return	:  NULL = no line available. Waiting for more chars (for a valid line)
 *			:  pointer to '\0' terminated string
 * Note: output line (if available) must be "consumed" before next call to this routine
 * Note: Input with no newline longer than MAXOUTSZ are discarded
 * ************************************************************************************** */
 void extract_line_printerr(int ret);
/* @brief	: printf for return value of above code
 * ************************************************************************************** */

#endif