/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LINKSUPPORT_H_
#define _LINKSUPPORT_H_ 1

#include "config.h"

#if !defined(HAVE_INET_NTOP)
 const char *inet_ntop( int family, const void *addr,
	char *str, size_t len );
#endif
#if !defined(HAVE_INET_PTON)
 int inet_pton( int family, const char *strptr, void *addr );
#endif

/* PROTOTYPES */
int Link_setreuse( int sock );
void Set_linger( int sock, int n );
int Link_listen( char *port_name );
int Unix_link_listen( char *unix_socket_path );
int Link_open(char *host, int timeout, struct sockaddr *bindto,
	char *unix_socket_path, char *errmsg, int errlen );
int Link_open_type(char *host, int timeout, int connection_type,
	struct sockaddr *bindto, char * unix_socket_path, char *errmsg, int errlen );
int Link_open_list( char *hostlist, char **result,
	int timeout, struct sockaddr *bindto, char *unix_socket_path, char *errmsg, int errlen );
void Link_close( int timeout, int *sock );
int Link_send( char *host, int *sock, int timeout,
	const char *sendstr, int count, int *ack );
int Link_copy( char *host, int *sock, int readtimeout, int writetimeout,
	const char *src, int fd, double pcount);
int Link_dest_port_num( char *port );
int Link_line_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_read(char *host, int *sock, int timeout,
	  char *buf, int *count );
int Link_file_read(char *host, int *sock, int readtimeout, int writetimeout,
	  int fd, double *count, int *ack );
const char *Link_err_str (int n);
const char *Ack_err_str (int n);
int AF_Protocol(void);
const char *inet_ntop_sockaddr( struct sockaddr *addr,
	char *str, int len );

#endif
