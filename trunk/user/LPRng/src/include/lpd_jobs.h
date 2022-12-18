/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPD_JOBS_H_
#define _LPD_JOBS_H_ 1

/* PROTOTYPES */
int Do_queue_jobs( char *name, int subserver );
void Service_worker( struct line_list *args, int ) NORETURN;
void Service_queue( struct line_list *args, int ) NORETURN;
int Remove_done_jobs( void );

#endif
