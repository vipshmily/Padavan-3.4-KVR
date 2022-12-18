/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPD_STATUS_H_
#define _LPD_STATUS_H_ 1

/* PROTOTYPES */
int Job_status( int *sock, char *input );
void Get_queue_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size, char *hash_key );
void Print_different_last_status_lines( int *sock, int fd,
	int status_lines, int max_size );
void Get_local_or_remote_status( struct line_list *tokens, int *sock,
	int displayformat, int status_lines, struct line_list *done_list,
	int max_size, char *hash_key );

#endif
