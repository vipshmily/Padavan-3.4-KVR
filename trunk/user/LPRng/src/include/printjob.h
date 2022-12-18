/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _PRINTJOB_H_
#define _PRINTJOB_H_ 1

/* PROTOTYPES */
int Print_job( int output, int status_device, struct job *job,
	int send_job_rw_timeout, int poll_for_status, char *user_filter );
int Get_status_from_OF( struct job *job, const char *title, int of_pid,
	int of_error, char *msg, int msgmax,
	int timeout, int suspend, int max_wait, char *status_file );
int Wait_for_pid( int of_pid, const char *name, int suspend, int timeout );
void Add_banner_to_job( struct job *job );

#endif
