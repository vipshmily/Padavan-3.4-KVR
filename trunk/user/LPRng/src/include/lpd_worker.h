/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

#ifndef _LPD_WORKER_H_
#define _LPD_WORKER_H_ 1

pid_t Start_worker( const char *name, WorkerProc *proc, struct line_list *parms, int fd );

#endif
