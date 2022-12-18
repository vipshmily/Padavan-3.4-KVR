/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

#include "lp.h"
#include "user_auth.h"
#include "krb5_auth.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linksupport.h"
#include "child.h"
#include "getqueue.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "permission.h"

#if 0
/*
  Test_connect: send the validation information  
    expect to get back NULL or error message
 */

static int Test_connect( struct job *job UNUSED, int *sock,
	int transfer_timeout,
	char *errmsg, int errlen,
	struct security *security UNUSED, struct line_list *info )
{
	const char *secure = "TEST\n";
	int status = 0, ack = 0;

	if(DEBUGL1)Dump_line_list("Test_connect: info", info );
	DEBUG3("Test_connect: sending '%s'", secure);
	status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		secure, safestrlen(secure), &ack );
	DEBUG3("Test_connect: status '%s'", Link_err_str(status) );
	if( status ){
		plp_snprintf(errmsg, errlen,
			"Test_connect: error '%s'", Link_err_str(status) );
		status = JFAIL;
	}
	if( ack ){
		plp_snprintf(errmsg, errlen,
			"Test_connect: ack '%d'", ack );
		status = JFAIL;
	}
	return( status );
}

static int Test_accept( int *sock, int transfer_timeout,
	char *user UNUSED, char *jobsize UNUSED, int from_server UNUSED,
	char *authtype UNUSED, char *errmsg, int errlen,
	struct line_list *info, struct line_list *header_info,
	struct security *security UNUSED)
{
	int status, len;
	char input[SMALLBUFFER];

	DEBUGFC(DRECV1)Dump_line_list("Test_accept: info", info );
	DEBUGFC(DRECV1)Dump_line_list("Test_accept: header_info", header_info );

	len = sizeof(input)-1;
	status = Link_line_read(ShortRemote_FQDN,sock,
		transfer_timeout,input,&len);
	if( len >= 0 ) input[len] = 0;
	if( status ){
		plp_snprintf(errmsg,errlen,
			"error '%s' READ from %s@%s",
			Link_err_str(status), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	DEBUG1( "Test_accept: read status %d, len %d, '%s'",
		status, len, input );
	if( (status = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		"", 1, 0 )) ){
		plp_snprintf(errmsg,errlen,
			"error '%s' ACK to %s@%s",
			Link_err_str(status), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	DEBUG1( "Test_accept: ACK sent");

 error:
	return( status );
}
#endif

/**************************************************************
 *Test_send:
 *A simple implementation for testing user supplied authentication
 *
 * Simply send the authentication information to the remote end
 * destination=test     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell      <- client id
 *                       (client ID from above)
 * input=%04t1          <- input that is 
 *
 **************************************************************/

static int Test_send( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *errmsg, int errlen,
	const struct security *security UNUSED, struct line_list *info )
{
	char buffer[LARGEBUFFER];
	struct stat statb;
	int tempfd, len;
	int status = 0;

	if(DEBUGL1)Dump_line_list("Test_send: info", info );
	DEBUG1("Test_send: sending on socket %d", *sock );
	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		plp_snprintf(errmsg, errlen,
			"Test_send: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("Test_send: starting read");
	while( (len = Read_fd_len_timeout( transfer_timeout, tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("Test_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			plp_snprintf(errmsg, errlen,
				"Test_send: write to socket failed - %s", Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	if( len < 0 ){
		plp_snprintf(errmsg, errlen,
			"Test_send: read from '%s' failed - %s", tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );

	DEBUG1("Test_send: sent file" );

	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		plp_snprintf(errmsg, errlen,
			"Test_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("Test_send: starting read");

	while( (len = Read_fd_len_timeout(transfer_timeout, *sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[len] = 0;
		DEBUG4("Test_send: socket information '%s'", buffer);
		if( write(tempfd,buffer,len) != len ){
			plp_snprintf(errmsg, errlen,
				"Test_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	close( tempfd ); tempfd = -1;

 error:
	return(status);
}

static int Test_receive( int *sock, int transfer_timeout,
	char *user UNUSED, char *jobsize, int from_server, char *authtype UNUSED,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security UNUSED, char *tempfile,
	SECURE_WORKER_PROC do_secure_work)
{
	int tempfd, status, n;
	char buffer[LARGEBUFFER];
	struct stat statb;

	tempfd = -1;

	DEBUGFC(DRECV1)Dump_line_list("Test_receive: info", info );
	DEBUGFC(DRECV1)Dump_line_list("Test_receive: header_info", header_info );
	/* do validation and then write 0 */
	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		plp_snprintf( errmsg, errlen, "Test_receive: ACK 0 write error - %s",
			Errormsg(errno) );
		goto error;
	}

	/* open a file for the output */
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Test_receive: reopen of '%s' for write failed",
			tempfile );
	}

	DEBUGF(DRECV1)("Test_receive: starting read from socket %d", *sock );
	while( (n = Read_fd_len_timeout(transfer_timeout, *sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Test_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			DEBUGF(DRECV1)( "Test_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
	}
	if( n < 0 ){
		DEBUGF(DRECV1)("Test_receive: bad read '%d' getting command", n );
		status = JFAIL;
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("Test_receive: end read" );

	/*** at this point you can check the format of the received file, etc.
     *** if you have an error message at this point, you should write it
	 *** to the socket,  and arrange protocol can handle this.
	 ***/

	status = do_secure_work( jobsize, from_server, tempfile, header_info );

	/*** if an error message is returned, you should write this
	 *** message to the tempfile and the proceed to send the contents
	 ***/
	DEBUGF(DRECV1)("Test_receive: doing reply" );
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Test_receive: reopen of '%s' for write failed",
			tempfile );
	}

	while( (n = Read_fd_len_timeout(transfer_timeout, tempfd, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("Test_receive: sending '%d' '%s'", n, buffer );
		if( write( *sock,buffer,n ) != n ){
			DEBUGF(DRECV1)( "Test_receive: bad write to socket - '%s'",
				Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
	}
	if( n < 0 ){
		DEBUGF(DRECV1)("Test_receive: bad read '%d' getting status", n );
		status = JFAIL;
		goto error;
	}
	DEBUGF(DRECV1)("Test_receive: reply done" );

 error:
	if( tempfd>=0) close(tempfd); tempfd = -1;
	return(status);
}

const struct security test_auth =
	{ "test",      "test",	"test",     0,              0,           Test_send, 0, Test_receive };

#ifdef WITHPLUGINS
plugin_get_func getter_name(test);
size_t getter_name(test)(const struct security **s, size_t max) {
	if( max > 0 )
		*s = &test_auth;
	return 1;
}
#endif
