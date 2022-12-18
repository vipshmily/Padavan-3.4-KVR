/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPD_RCVJOB_H_
#define _LPD_RCVJOB_H_ 1

/* PROTOTYPES */
int Receive_job( int *sock, char *input );
int Receive_block_job( int *sock, char *input );
int Scan_block_file( int fd, char *error, int errlen, struct line_list *header_info );
int Check_space( double jobsize, int min_space, char *pathname );
int Check_for_missing_files( struct job *job, struct line_list *files,
	char *error, int errlen, struct line_list *header_info, int holdfile_fd );
int Setup_temporary_job_ticket_file( struct job *job, char *filename,
	int read_control_file,
	char *cf_file_image,
	char *error, int errlen  );

#endif
