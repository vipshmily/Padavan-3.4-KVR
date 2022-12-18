/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

#ifndef _OPENPRINTER_H_
#define _OPENPRINTER_H_ 1


int Printer_open( char *lp_device, int *status_fd, struct job *job,
	int max_attempts, int interval, int max_interval, int grace,
	int connect_tmout, int *filterpid, int *poll_for_status );

#endif
