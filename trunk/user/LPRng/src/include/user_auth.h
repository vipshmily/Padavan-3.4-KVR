/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _USER_AUTH_H_
#define _USER_AUTH_H_ 1

/***************************************************************
 * Security stuff - needs to be in a common place
 ***************************************************************/

struct security;
typedef int (*CONNECT_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *errmsg, int errlen,
	const struct security *security, struct line_list *info );

typedef int (*SEND_PROC)( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *error, int errlen,
	const struct security *security, struct line_list *info );

typedef int (*GET_REPLY_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *error, int errlen,
	const struct security *security, struct line_list *info );

typedef int (*SEND_DONE_PROC)( struct job *job, int *sock,
	int transfer_timeout,
	char *error, int errlen,
	const struct security *security, struct line_list *info );

typedef int (*ACCEPT_PROC)(
	int *sock, int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	const struct security *security );

typedef int (*SECURE_WORKER_PROC)(
	char *jobsize, int from_server,
	char *tempfile, struct line_list *header_info );

typedef int (*RECEIVE_PROC)(
	int *sock, int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *error, int errlen,
	struct line_list *header_info,
	const struct security *security, char *tempfile,
	SECURE_WORKER_PROC do_secure_work);

typedef int (*REPLY_PROC)(
	int *sock, char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	const struct security *security );

typedef int (*RCV_DONE_PROC)( int *sock,
	char *error, int errlen,
	struct line_list *info, struct line_list *header_info,
	const struct security *security );

struct security {
	const char *name;				/* authentication name */
	const char *server_tag;		/* send this tag to server */
	const char *config_tag;		/* use this tag for configuration information */
	int auth_flags;				/* flags */
#define IP_SOCKET_ONLY	1 /* use TCP/IP socket only */
	CONNECT_PROC client_connect;	/* client to server connection, talk to verify */
	SEND_PROC    client_send;		/* client to server authenticate transfer, talk to transfer */
	ACCEPT_PROC server_accept;		/* server accepts the connection, sets up transfer */
	RECEIVE_PROC server_receive;	/* server to client, receive from client */
};

typedef size_t (plugin_get_func)(const struct security **, size_t max);

/* if anything changes, increment this to avoid old plugins getting loaded */
#define AUTHPLUGINVERSION 0
#define getter_name(n) get_lprng_auth_0_ ## n

/* PROTOTYPES */
const struct security *FindSecurity( const char *name );
char *ShowSecuritySupported( char *str, int maxlen );

#ifndef WITHPLUGINS
extern const struct security test_auth;
extern const struct security md5_auth;
#endif

#endif
