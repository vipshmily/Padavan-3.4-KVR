/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "child.h"
#include "getqueue.h"
#include "linksupport.h"
#include "gethostinfo.h"
#include "permission.h"
#include "lpd_secure.h"
#include "lpd_dispatch.h"
#include "user_auth.h"
#include "krb5_auth.h"

#if defined(KERBEROS)

/*
 * deprecated: krb5_auth_con_initivector, krb5_get_in_tkt_with_keytab
 * WARNING: the current set of KRB5 support examples are not compatible
 * with the legacy LPRng use of the KRB utility functions.  This means
 * that sooner or later it will be necessary to severly upgrade LPRng
 * or to use an older version of Kerberos
 */
# define KRB5_DEPRECATED 1
# define KRB5_PRIVATE 1
# include <krb5.h>
# if defined(HAVE_COM_ERR_H)
#  include <com_err.h>
# endif

# undef FREE_KRB_DATA
# if defined(HAVE_KRB5_FREE_DATA_CONTENTS)
#  define FREE_KRB_DATA(context,data,suffix) krb5_free_data_contents(context,&data); data suffix = 0
# else
#  if defined(HAVE_KRB5_XFREE)
#   define FREE_KRB_DATA(context,data,suffix) krb5_xfree(data suffix); data suffix = 0
#  else
#    if !defined(HAVE_KRB_XFREE)
#     define FREE_KRB_DATA(context,data,suffix) krb_xfree(data suffix); data suffix = 0
#    else
#     error missing krb_xfree value or definition
#    endif
#  endif
# endif

# ifdef HAVE_KRB5_READ_MESSAGE
#  define read_message krb5_read_message
#  define write_message krb5_write_message
# else
static int net_read( int fd, char *buf, int len )
{
  int remaining = len;
  while (remaining) {
    int r;
    r = read(fd, buf, remaining);
    if (r <= 0)
      return (r);
    remaining -=r;
  }
  return (len);
}

static krb5_error_code read_message( krb5_context context, krb5_pointer fdp, krb5_data *inbuf )
{
	krb5_int32	len;
	int		len2, ilen;
	char		*buf = NULL;
	int		fd = *( (int *) fdp);

	if ((len2 = net_read( fd, (char *)&len, 4)) != 4)
		return((len2 < 0) ? errno : ECONNABORTED);
	len = ntohl(len);

	if ((len & (signed) VALID_UINT_BITS) != len)  /* Overflow size_t??? */
		return ENOMEM;

	inbuf->length = ilen = (int) len;
	if (ilen) {
		/*
		 * We may want to include a sanity check here someday....
		 */
		buf = malloc_or_die(  ilen, __FILE__,__LINE__ );
		if ((len2 = net_read( fd, buf, ilen)) != ilen) {
			free(buf);
			return((len2 < 0) ? errno : ECONNABORTED);
		}
	}
	inbuf->data = buf;
	return(0);
}

static int net_write( int fd, const char * buf, int len )
{
  int written;
  int total_len = len;
  while (len >0) {
    written = write(fd, buf, len);
    if (written <= 0)
      return (written);
    len -= written;
  }
  return (total_len);
}

static krb5_error_code write_message( krb5_context context, krb5_pointer fdp, krb5_data *outbuf )
{
	krb5_int32	len;
	int		fd = *( (int *) fdp);

	len = htonl(outbuf->length);
	if (net_write( fd, (char *)&len, 4) < 0) {
		return(errno);
	}
	if (outbuf->length && (net_write(fd, outbuf->data, outbuf->length) < 0)) {
		return(errno);
	}
	return(0);
}
# endif

/*
 * server_krb5_auth(
 *  char *keytabfile,	server key tab file - /etc/lpr.keytab
 *  char *service,		service is usually "lpr"
 *  char *prinicpal,	specifically supply principal name
 *  int sock,		   socket for communications
 *  char *auth, int len authname buffer, max size
 *  char *err, int errlen error message buffer, max size
 * RETURNS: 0 if successful, non-zero otherwise, error message in err
 *   Note: there is a memory leak if authentication fails,  so this
 *   should not be done in the main or non-exiting process
 */
 extern int des_read( krb5_context context, krb5_encrypt_block *eblock,
	int fd, int transfer_timeout, char *buf, int len, char *err, int errlen );
 extern int des_write( krb5_context context, krb5_encrypt_block *eblock,
	int fd, char *buf, int len, char *err, int errlen );

 /* we make these statics */
 static krb5_context context = 0;
 static krb5_auth_context auth_context = 0;
 static krb5_keytab keytab = 0;  /* Allow specification on command line */
 static krb5_principal server = 0;
 static krb5_ticket * ticket = 0;

static int server_krb5_auth( char *keytabfile, char *service, char *server_principal, int sock,
	char **auth, char *err, int errlen, char *file, int use_crypt_transfer )
{
	int retval = 0;
	int fd = -1;
	krb5_data   inbuf, outbuf;
	struct stat statb;
	int status;
	char *cname = 0;

	DEBUG1("server_krb5_auth: keytab '%s', service '%s', principal '%s', sock %d, file '%s'",
		keytabfile, service, server_principal, sock, file );
	if( !keytabfile ){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"no server keytab file",
			Is_server?"on server":"on client" );
		retval = 1;
		goto done;
	}
	if( (fd = Checkread(keytabfile,&statb)) == -1 ){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"cannot open server keytab file '%s' - %s",
			Is_server?"on server":"on client",
			keytabfile,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	close(fd);
	err[0] = 0;
	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_init_context failed - '%s' ",
			Is_server?"on server":"on client",
			error_message(retval) );
		goto done;
	}
	if( keytab == 0 && (retval = krb5_kt_resolve(context, keytabfile, &keytab) ) ){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_kt_resolve failed - file %s '%s'",
			Is_server?"on server":"on client",
			keytabfile,
			error_message(retval) );
		goto done;
	}
	if(server_principal){
		if ((retval = krb5_parse_name(context,server_principal, &server))){
			plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			server_principal, error_message(retval) );
			goto done;
		}
	} else {
		if ((retval = krb5_sname_to_principal(context, NULL, service,
						 KRB5_NT_SRV_HST, &server))){
			plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
				"krb5_sname_to_principal failed - service %s '%s'",
				Is_server?"on server":"on client",
				service, error_message(retval));
			goto done;
		}
	}
	if((retval = krb5_unparse_name(context, server, &cname))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_unparse_name failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	DEBUG1("server_krb5_auth: server '%s'", cname );

	if((retval = krb5_recvauth(context, &auth_context, (krb5_pointer)&sock,
				   service , server, 
				   0,   /* no flags */
				   keytab,  /* default keytab is NULL */
				   &ticket))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_recvauth '%s' failed '%s'",
			Is_server?"on server":"on client",
			cname, error_message(retval));
		goto done;
	}

	/* Get client name */
	if((retval = krb5_unparse_name(context, 
		ticket->enc_part2->client, &cname))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_unparse_name failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	if( auth ) *auth = safestrdup( cname,__FILE__,__LINE__);
	DEBUG1( "server_krb5_auth: client '%s'", cname );
	/* initialize the initial vector */
	if((retval = krb5_auth_con_initivector(context, auth_context))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_auth_con_initvector failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);
	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
			KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"krb5_auth_con_genaddr failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
  
	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	DEBUG1( "server_krb5_auth: opened for write '%s', fd %d", file, fd );
	if( fd < 0 ){
		plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
			"file open failed: %s",
			Is_server?"on server":"on client",
			Errormsg(errno));
		retval = 1;
		goto done;
	}
	if( use_crypt_transfer ) {
		while( (retval = read_message(context,&sock,&inbuf)) == 0 ){
			if(DEBUGL5){
				char small[16];
				memcpy(small,inbuf.data,sizeof(small)-1);
				small[sizeof(small)-1] = 0;
				LOGDEBUG( "server_krb5_auth: got %d, '%s'",
					inbuf.length, small );
			}
			if((retval = krb5_rd_priv(context,auth_context,
				&inbuf,&outbuf,NULL))){
				plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
					"krb5_rd_priv failed: %s",
					Is_server?"on server":"on client",
					error_message(retval));
				retval = 1;
				goto done;
			}
			status = Write_fd_len( fd, outbuf.data, outbuf.length );
			if( status < 0 ){
				plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
					"write to file failed: %s",
					Is_server?"on server":"on client",
					Errormsg(errno));
				retval = 1;
				goto done;
			}
			FREE_KRB_DATA(context,inbuf,.data);
			FREE_KRB_DATA(context,outbuf,.data);
			inbuf.length = 0;
			outbuf.length = 0;
		}
	} else {
		char buffer[SMALLBUFFER];
		while( (retval = ok_read( sock, buffer, sizeof(buffer))) > 0 ){
			if( Write_fd_len( fd, buffer, retval ) < 0 ){
				plp_snprintf( err, errlen, "%s server_krb5_auth failed - "
					"write to file failed: %s",
					Is_server?"on server":"on client",
					Errormsg(errno));
				retval = 1;
				goto done;
			}
		}
	}
	close(fd); fd = -1;
	retval = 0;

 done:
	if( cname )		free(cname); cname = 0;
	if( retval ){
		if( fd >= 0 )	close(fd);
		if( ticket )	krb5_free_ticket(context, ticket);
		ticket = 0;
		if( context && server )	krb5_free_principal(context, server);
		server = 0;
		if( context && auth_context)	krb5_auth_con_free(context, auth_context );
		auth_context = 0;
		if( context )	krb5_free_context(context);
		context = 0;
	}
	DEBUG1( "server_krb5_auth: retval %d, error: '%s'", retval, err );
	return(retval);
}


static int server_krb5_status( int sock, char *err, int errlen, char *file, int use_crypt_transfer )
{
	int fd = -1;
	int retval = 0;
	struct stat statb;
	char buffer[SMALLBUFFER];
	krb5_data   inbuf, outbuf;

	err[0] = 0;
	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkread( file, &statb );
	if( fd < 0 ){
		plp_snprintf( err, errlen,
			"file open failed: %s", Errormsg(errno));
		retval = 1;
		goto done;
	}
	DEBUG1( "server_krb5_status: sock '%d', file size %0.0f", sock, (double)(statb.st_size));

	while( (retval = ok_read( fd,buffer,sizeof(buffer)-1)) > 0 ){
		if( use_crypt_transfer ) {
			inbuf.length = retval;
			inbuf.data = buffer;
			buffer[retval] = 0;
			DEBUG4("server_krb5_status: sending '%s'", buffer );
			if((retval = krb5_mk_priv(context,auth_context,
				&inbuf,&outbuf,NULL))){
				plp_snprintf( err, errlen, "%s server_krb5_status failed - "
					"krb5_mk_priv failed: %s",
					Is_server?"on server":"on client",
					error_message(retval));
				retval = 1;
				goto done;
			}
			DEBUG4("server_krb5_status: encoded length '%d'", outbuf.length );
			if((retval= write_message(context,&sock,&outbuf))){
				plp_snprintf( err, errlen, "%s server_krb5_status failed - "
					"write_message failed: %s",
					Is_server?"on server":"on client",
					error_message(retval));
				retval = 1;
				goto done;
			}
			FREE_KRB_DATA(context,outbuf,.data);
			memset((char *)&outbuf, 0, sizeof(outbuf));
		} else {
			if( Write_fd_len( sock, buffer, retval ) < 0 ){
				plp_snprintf( err, errlen, "%s server_krb5_status failed - "
					"write_message failed: %s",
					Is_server?"on server":"on client",
					Errormsg(errno));
				retval = 1;
				goto done;
			}
		}
	}
	DEBUG1("server_krb5_status: done" );

 done:
	if( fd >= 0 )	close(fd);
	if( ticket )	krb5_free_ticket(context, ticket);
	ticket = 0;
	if( context && server )	krb5_free_principal(context, server);
	server = 0;
	if( context && auth_context)	krb5_auth_con_free(context, auth_context );
	auth_context = 0;
	if( context )	krb5_free_context(context);
	context = 0;
	DEBUG1( "server_krb5_status: retval %d, error: '%s'", retval, err );
	return(retval);
}

/*
 * client_krb5_auth(
 *  char * keytabfile	- keytabfile, NULL for users, file name for server
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char * principal	- server principal
 *  int options		 - options for server to server
 *  char *life			- lifetime of ticket
 *  char *renew_time	- renewal time of ticket
 *  char *err, int errlen - buffer for error messages 
 *  char *file			- file to transfer
 *  int use_crypt_transfer  - transfer using encryption
 */ 
# define KRB5_DEFAULT_OPTIONS 0
# define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */
# define VALIDATE 0
# define RENEW 1

 extern krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	krb5_principal server, krb5_data *outbuf, int opt );

static int client_krb5_auth( char *keytabfile, char *service, char *host,
	char *server_principal,
	int options, char *life, char *renew_time,
	int sock, char *err, int errlen, char *file, int use_crypt_transfer )
{
	krb5_context context = 0;
	krb5_principal client = 0, server = 0;
	krb5_error *err_ret = 0;
	krb5_ap_rep_enc_part *rep_ret = 0;
	krb5_data cksum_data;
	krb5_ccache ccdef;
	krb5_auth_context auth_context = 0;
	krb5_timestamp now;
	krb5_deltat lifetime = KRB5_DEFAULT_LIFE;   /* -l option */
	krb5_creds my_creds;
	krb5_creds *out_creds = 0;
	krb5_keytab keytab = 0;
	krb5_deltat rlife = 0;
	krb5_address **addrs = (krb5_address **)0;
	krb5_encrypt_block eblock;	  /* eblock for encrypt/decrypt */
	krb5_data   inbuf, outbuf;
	int retval = 0;
	char *cname = 0;
	char *sname = 0;
	int fd = -1, len;
	char buffer[SMALLBUFFER];
	struct stat statb;

	err[0] = 0;
	DEBUG1( "client_krb5_auth: euid/egid %d/%d, ruid/rguid %d/%d, keytab '%s',"
		" service '%s', host '%s', sock %d, file '%s'",
		geteuid(),getegid(), getuid(),getgid(),
		keytabfile, service, host, sock, file );
	if( !safestrcasecmp(host,LOCALHOST) ){
		host = FQDNHost_FQDN;
	}
	memset((char *)&my_creds, 0, sizeof(my_creds));
	memset((char *)&outbuf, 0, sizeof(outbuf));
	memset((char *)&eblock, 0, sizeof(eblock));
	options |= KRB5_DEFAULT_OPTIONS;

	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen,
			"%s krb5_init_context failed - '%s' ",
			Is_server?"on server":"on client",
			error_message(retval) );
		goto done;
	}
#if 0
	if (!valid_cksumtype(CKSUMTYPE_CRC32)) {
		plp_snprintf( err, errlen,
			"valid_cksumtype CKSUMTYPE_CRC32 - %s",
			error_message(KRB5_PROG_SUMTYPE_NOSUPP) );
		retval = 1;
		goto done;
	}
#endif
	DEBUG1( "client_krb5_auth: using host='%s', server_principal '%s'",
		host, server_principal );

	if(server_principal){
		if ((retval = krb5_parse_name(context,server_principal, &server))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			server_principal, error_message(retval) );
			goto done;
		}
	} else {
		/* XXX perhaps we want a better metric for determining localhost? */
		if (strncasecmp("localhost", host, sizeof(host)))
			retval = krb5_sname_to_principal(context, host, service,
							 KRB5_NT_SRV_HST, &server);
		else
			/* Let libkrb5 figure out its notion of the local host */
			retval = krb5_sname_to_principal(context, NULL, service,
							 KRB5_NT_SRV_HST, &server);
		if (retval) {
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"when parsing service/host '%s'/'%s'"
			" - %s",
			Is_server?"on server":"on client",
			service,host,error_message(retval) );
			goto done;
		}
	}

	if((retval = krb5_unparse_name(context, server, &sname))){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			" krb5_unparse_name of 'server' failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
	DEBUG1( "client_krb5_auth: server '%s'", sname );

	my_creds.server = server;

	if( keytabfile ){
		if ((retval = krb5_sname_to_principal(context, NULL, service, 
			 KRB5_NT_SRV_HST, &client))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"when parsing name '%s'"
			" - %s",
			Is_server?"on server":"on client",
			service, error_message(retval) );
			goto done;
		}
		if(cname)free(cname); cname = 0;
		if((retval = krb5_unparse_name(context, client, &cname))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"krb5_unparse_name of 'me' failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		DEBUG1("client_krb5_auth: client '%s'", cname );
		my_creds.client = client;
		if((retval = krb5_kt_resolve(context, keytabfile, &keytab))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			 "resolving keytab '%s'"
			" '%s' - ",
			Is_server?"on server":"on client",
			keytabfile, error_message(retval) );
			goto done;
		}
		if ((retval = krb5_timeofday(context, &now))) {
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			 "getting time of day"
			" - '%s'",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		if( life && (retval = krb5_string_to_deltat(life, &lifetime)) ){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"bad lifetime value '%s'"
			" '%s' - ",
			Is_server?"on server":"on client",
			life, error_message(retval) );
			goto done;
		}
		if( renew_time ){
			options |= KDC_OPT_RENEWABLE;
			if( (retval = krb5_string_to_deltat(renew_time, &rlife))){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"bad renew time value '%s'"
				" '%s' - ",
				Is_server?"on server":"on client",
				renew_time, error_message(retval) );
				goto done;
			}
		}
		if((retval = krb5_cc_default(context, &ccdef))) {
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"while getting default ccache"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}

		my_creds.times.starttime = 0;	 /* start timer when request */
		my_creds.times.endtime = now + lifetime;

		if(options & KDC_OPT_RENEWABLE) {
			my_creds.times.renew_till = now + rlife;
		} else {
			my_creds.times.renew_till = 0;
		}

		if(options & KDC_OPT_VALIDATE){
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, VALIDATE))) {
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"validating tgt"
				" - %s",
				Is_server?"on server":"on client",
				error_message(retval) );
				DEBUG1("%s", err );
			}
		}

		if (options & KDC_OPT_RENEW) {
			/* stripped down version of krb5_mk_req */
			if( (retval = krb5_tgt_gen(context,
				ccdef, server, &outbuf, RENEW))) {
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"renewing tgt"
				" - %s",
				Is_server?"on server":"on client",
				error_message(retval) );
				DEBUG1("%s", err );
			}
		}

		if((retval = krb5_get_in_tkt_with_keytab(context, options, addrs,
					0, 0, keytab, 0, &my_creds, 0))){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"while getting initial credentials"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		/* update the credentials */
		if( (retval = krb5_cc_initialize (context, ccdef, client)) ){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"when initializing cache"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
		if( (retval = krb5_cc_store_cred(context, ccdef, &my_creds))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"while storing credentials"
			" - %s",
			Is_server?"on server":"on client",
			error_message(retval) );
			goto done;
		}
	} else {
		/* we set RUID to user */
		if( Is_server ){
			To_ruid( DaemonUID );
		} else {
			To_ruid( OriginalRUID );
		}
		if((retval = krb5_cc_default(context, &ccdef))){
			plp_snprintf( err, errlen, "%s krb5_cc_default failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
			goto done;
		}
		if((retval = krb5_cc_get_principal(context, ccdef, &client))){
			plp_snprintf( err, errlen, "%s krb5_cc_get_principal failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
			goto done;
		}
		if( Is_server ){
			To_daemon();
		} else {
			To_user();
		}
		if(cname)free(cname); cname = 0;
		if((retval = krb5_unparse_name(context, client, &cname))){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"krb5_unparse_name of 'me' failed: %s",
				Is_server?"on server":"on client",
				error_message(retval));
			goto done;
		}
		DEBUG1( "client_krb5_auth: client '%s'", cname );
		my_creds.client = client;
	}

	cksum_data.data = host;
	cksum_data.length = safestrlen(host);

	if((retval = krb5_auth_con_init(context, &auth_context))){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"krb5_auth_con_init failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	if((retval = krb5_auth_con_genaddrs(context, auth_context, sock,
		KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR |
		KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR))){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"krb5_auth_con_genaddr failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}
  
	retval = krb5_sendauth(context, &auth_context, (krb5_pointer) &sock,
			   service, client, server,
			   AP_OPTS_MUTUAL_REQUIRED,
			   &cksum_data,
			   &my_creds,
			   ccdef, &err_ret, &rep_ret, &out_creds);

	if (retval){
		if( err_ret == 0 ){
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"krb5_sendauth failed - %s",
				Is_server?"on server":"on client",
				error_message( retval ) );
		} else {
			plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
				"krb5_sendauth - mutual authentication failed - %*s",
				Is_server?"on server":"on client",
				err_ret->text.length, err_ret->text.data);
		}
		goto done;
	} else if (rep_ret == 0) {
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"krb5_sendauth - did not do mutual authentication",
			Is_server?"on server":"on client" );
		retval = 1;
		goto done;
	} else {
		DEBUG1("client_krb5_auth: sequence number %d", rep_ret->seq_number );
	}
	/* initialize the initial vector */
	if((retval = krb5_auth_con_initivector(context, auth_context))){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"krb5_auth_con_initvector failed: %s",
			Is_server?"on server":"on client",
			error_message(retval));
		goto done;
	}

	krb5_auth_con_setflags(context, auth_context,
		KRB5_AUTH_CONTEXT_DO_SEQUENCE);

	memset( &inbuf, 0, sizeof(inbuf) );
	memset( &outbuf, 0, sizeof(outbuf) );

	fd = Checkread( file, &statb );
	if( fd < 0 ){
		plp_snprintf( err, errlen,
			"%s client_krb5_auth: could not open for reading '%s' - '%s'",
			Is_server?"on server":"on client",
			file,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	DEBUG1( "client_krb5_auth: opened for read %s, fd %d, size %0.0f", file, fd, (double)statb.st_size );
	while( (len = ok_read( fd, buffer, sizeof(buffer)-1 )) > 0 ){
		if( use_crypt_transfer ){
			inbuf.data = buffer;
			inbuf.length = len;
			if((retval = krb5_mk_priv(context, auth_context, &inbuf,
				&outbuf, NULL))){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"krb5_mk_priv failed: %s",
					Is_server?"on server":"on client",
					error_message(retval));
				goto done;
			}
			if((retval = write_message(context, (void *)&sock, &outbuf))){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"write_message failed: %s",
					Is_server?"on server":"on client",
					error_message(retval));
				goto done;
			}
			DEBUG4( "client_krb5_auth: freeing data");
			FREE_KRB_DATA(context,outbuf,.data);
		} else {
			if(Write_fd_len(sock, buffer,len) < 0 ){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"write_message failed: %s",
					Is_server?"on server":"on client",
					Errormsg(errno));
				goto done;
			}
		}
	}
	if( len < 0 ){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"client_krb5_auth: file read failed '%s' - '%s'", file,
			Is_server?"on server":"on client",
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	close(fd);
	fd = -1;
	DEBUG1( "client_krb5_auth: file copy finished %s", file );
	if( shutdown(sock, 1) == -1 ){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"shutdown failed '%s'",
			Is_server?"on server":"on client",
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	fd = Checkwrite( file, &statb, O_WRONLY|O_TRUNC, 1, 0 );
	if( fd < 0 ){
		plp_snprintf( err, errlen,
			"%s client_krb5_auth: could not open for writing '%s' - '%s'",
			Is_server?"on server":"on client",
			file,
			Errormsg(errno) );
		retval = 1;
		goto done;
	}
	if( use_crypt_transfer ){
		while((retval = read_message( context,&sock,&inbuf))==0){
			if((retval = krb5_rd_priv(context, auth_context, &inbuf,
				&outbuf, NULL))){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"krb5_rd_priv failed - %s",
					Is_server?"on server":"on client",
					Errormsg(errno) );
				retval = 1;
				goto done;
			}
			if(Write_fd_len(fd,outbuf.data,outbuf.length) < 0){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"write to '%s' failed - %s",
					Is_server?"on server":"on client",
					file, Errormsg(errno) );
				retval = 1;
				goto done;
			}
			FREE_KRB_DATA(context,inbuf,.data);
			FREE_KRB_DATA(context,outbuf,.data);
		}
	} else {
		while( (retval = ok_read( sock, buffer, sizeof(buffer))) > 0 ){
			if(Write_fd_len(fd,buffer,retval) < 0){
				plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
					"write to '%s' failed - %s",
					Is_server?"on server":"on client",
					file, Errormsg(errno) );
				retval = 1;
				goto done;
			}
		}
	}
	close(fd); fd = -1;
	fd = Checkread( file, &statb );
	err[0] = 0;
	if( fd < 0 ){
		plp_snprintf( err, errlen,
			"%s client_krb5_auth: could not open for reading '%s' - '%s'",
				Is_server?"on server":"on client",
				file,
				Errormsg(errno) );
		retval = 1;
		goto done;
	}
	DEBUG1( "client_krb5_auth: reopened for read %s, fd %d, size %0.0f", file, fd, (double)statb.st_size );
	if( dup2(fd,sock) == -1){
		plp_snprintf( err, errlen, "%s client_krb5_auth failed - "
			"dup2(%d,%d) failed - '%s'",
			Is_server?"on server":"on client",
			fd, sock, Errormsg(errno) );
	}
	retval = 0;

 done:
	if( fd >= 0 && fd != sock ) close(fd);
	DEBUG4( "client_krb5_auth: freeing my_creds");
	krb5_free_cred_contents( context, &my_creds );
	DEBUG4( "client_krb5_auth: freeing rep_ret");
	if( rep_ret )	krb5_free_ap_rep_enc_part( context, rep_ret ); rep_ret = 0;
	DEBUG4( "client_krb5_auth: freeing err_ret");
	if( err_ret )	krb5_free_error( context, err_ret ); err_ret = 0;
	DEBUG4( "client_krb5_auth: freeing auth_context");
	if( auth_context) krb5_auth_con_free(context, auth_context );
	auth_context = 0;
	DEBUG4( "client_krb5_auth: freeing context");
	if( context )	krb5_free_context(context); context = 0;
	DEBUG1( "client_krb5_auth: retval %d, error '%s'",retval, err );
	return(retval);
}

#if 0
/*
 * remote_principal_krb5(
 *  char * service		-service, usually "lpr"
 *  char * host			- server host name
 *  char *buffer, int bufferlen - buffer for credentials
 *  get the principal name of the remote service
 */ 
static int remote_principal_krb5( char *service, char *host, char *err, int errlen )
{
	krb5_context context = 0;
	krb5_principal server = 0;
	int retval = 0;
	char *cname = 0;

	DEBUG1("remote_principal_krb5: service '%s', host '%s'",
		service, host );
	if ((retval = krb5_init_context(&context))){
		plp_snprintf( err, errlen, "%s '%s'",
			"krb5_init_context failed - '%s' ", error_message(retval) );
		goto done;
	}
	if((retval = krb5_sname_to_principal(context, host, service,
			 KRB5_NT_SRV_HST, &server))){
		plp_snprintf( err, errlen, "krb5_sname_to_principal %s/%s failed - %s",
			service, host, error_message(retval) );
		goto done;
	}
	if((retval = krb5_unparse_name(context, server, &cname))){
		plp_snprintf( err, errlen,
			"krb5_unparse_name failed - %s", error_message(retval));
		goto done;
	}
	strncpy( err, cname, errlen );
 done:
	if( cname )		free(cname); cname = 0;
	if( server )	krb5_free_principal(context, server); server = 0;
	if( context )	krb5_free_context(context); context = 0;
	DEBUG1( "remote_principal_krb5: retval %d, result: '%s'",retval, err );
	return(retval);
}
#endif

/*
 * Initialize a credentials cache.
 */

# define KRB5_DEFAULT_OPTIONS 0
# define KRB5_DEFAULT_LIFE 60*60*10 /* 10 hours */

# define VALIDATE 0
# define RENEW 1

/* stripped down version of krb5_mk_req */
 krb5_error_code krb5_tgt_gen( krb5_context context, krb5_ccache ccache,
	 krb5_principal server, krb5_data *outbuf, int opt )
{
	krb5_error_code	   retval;
	krb5_creds		  * credsp;
	krb5_creds			creds;

	/* obtain ticket & session key */
	memset((char *)&creds, 0, sizeof(creds));
	if ((retval = krb5_copy_principal(context, server, &creds.server)))
		goto cleanup;

	if ((retval = krb5_cc_get_principal(context, ccache, &creds.client)))
		goto cleanup_creds;

	if(opt == VALIDATE) {
			if ((retval = krb5_get_credentials_validate(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	} else {
			if ((retval = krb5_get_credentials_renew(context, 0,
					ccache, &creds, &credsp)))
				goto cleanup_creds;
	}

	/* we don't actually need to do the mk_req, just get the creds. */
 cleanup_creds:
	krb5_free_cred_contents(context, &creds);

 cleanup:

	return retval;
}


 char *storage;
 int nstored = 0;
 char *store_ptr;
 krb5_data desinbuf,desoutbuf;

# define ENCBUFFERSIZE 2*LARGEBUFFER

 int des_read( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, int transfer_timeout, char *buf, int len,
	char *err, int errlen )
{
	int nreturned = 0;
	long net_len,rd_len;
	int cc;
	unsigned char len_buf[4];
	
	if( len <= 0 ) return(len);

	if( desinbuf.data == 0 ){
		desinbuf.data = malloc_or_die(  ENCBUFFERSIZE, __FILE__,__LINE__ );
		storage = malloc_or_die( ENCBUFFERSIZE, __FILE__,__LINE__ );
	}
	if (nstored >= len) {
		memcpy(buf, store_ptr, len);
		store_ptr += len;
		nstored -= len;
		return(len);
	} else if (nstored) {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		buf += nstored;
		len -= nstored;
		nstored = 0;
	}
	
	if ((cc = Read_fd_len_timeout(transfer_timeout, fd, (char*)len_buf, 4)) != 4) {
		/* XXX can't read enough, pipe must have closed */
		return(0);
	}
	rd_len =
		((len_buf[0]<<24) | (len_buf[1]<<16) | (len_buf[2]<<8) | len_buf[3]);
	net_len = krb5_encrypt_size(rd_len,eblock->crypto_entry);
	if ((net_len <= 0) || (net_len > ENCBUFFERSIZE )) {
		/* preposterous length; assume out-of-sync; only
		   recourse is to close connection, so return 0 */
		plp_snprintf( err, errlen, "des_read: "
			"read size problem");
		return(-1);
	}
	if ((cc = Read_fd_len_timeout( transfer_timeout, fd, desinbuf.data, net_len)) != net_len) {
		/* pipe must have closed, return 0 */
		plp_snprintf( err, errlen, "des_read: "
		"Read error: length received %d != expected %d.",
				(int)cc, (int)net_len);
		return(-1);
	}
	/* decrypt info */
	if((cc = krb5_decrypt(context, desinbuf.data, (krb5_pointer) storage,
						  net_len, eblock, 0))){
		plp_snprintf( err, errlen, "des_read: "
			"Cannot decrypt data from network - %s", error_message(cc) );
		return(-1);
	}
	store_ptr = storage;
	nstored = rd_len;
	if (nstored > len) {
		memcpy(buf, store_ptr, len);
		nreturned += len;
		store_ptr += len;
		nstored -= len;
	} else {
		memcpy(buf, store_ptr, nstored);
		nreturned += nstored;
		nstored = 0;
	}
	
	return(nreturned);
}


 int des_write( krb5_context context,
	krb5_encrypt_block *eblock,
	int fd, char *buf, int len,
	char *err, int errlen )
{
	char len_buf[4];
	int cc;

	if( len <= 0 ) return( len );
	if( desoutbuf.data == 0 ){
		desoutbuf.data = malloc_or_die( ENCBUFFERSIZE, __FILE__,__LINE__ );
	}
	desoutbuf.length = krb5_encrypt_size(len, eblock->crypto_entry);
	if (desoutbuf.length > ENCBUFFERSIZE ){
		plp_snprintf( err, errlen, "des_write: "
		"Write size problem - wanted %d", desoutbuf.length);
		return(-1);
	}
	if ((cc=krb5_encrypt(context, (krb5_pointer)buf,
					   desoutbuf.data,
					   len,
					   eblock,
					   0))){
		plp_snprintf( err, errlen, "des_write: "
		"Write encrypt problem. - %s", error_message(cc));
		return(-1);
	}
	
	len_buf[0] = (len & 0xff000000) >> 24;
	len_buf[1] = (len & 0xff0000) >> 16;
	len_buf[2] = (len & 0xff00) >> 8;
	len_buf[3] = (len & 0xff);
	if( Write_fd_len(fd, len_buf, 4) < 0 ){
		plp_snprintf( err, errlen, "des_write: "
		"Could not write len_buf - %s", Errormsg( errno ));
		return(-1);
	}
	if(Write_fd_len(fd, desoutbuf.data,desoutbuf.length) < 0 ){
		plp_snprintf( err, errlen, "des_write: "
		"Could not write data - %s", Errormsg(errno));
		return(-1);
	}
	else return(len); 
}

static int Krb5_receive_work( int *sock,
	int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security, char *tempfile,
	SECURE_WORKER_PROC do_secure_work, int use_crypt_transfer)
{
	int status = 0;
	char *from = 0;
	char *keytab = 0;
	char *service = 0;
	char *principal = 0;

	errmsg[0] = 0;
	DEBUG1("Krb5_receive_work: tempfile '%s', use_crypt_transfer %d", tempfile, use_crypt_transfer );
	keytab = Find_str_value(info,"keytab");
	service = Find_str_value(info,"service");
	if( !(principal = Find_str_value(info,"server_principal")) ){
		principal = Find_str_value(info,"id");
	}
	if( Write_fd_len( *sock, "", 1 ) < 0 ){
		status = JABORT;
		plp_snprintf( errmsg, errlen, "Krb5_receive_work: ACK 0 write errmsg - %s",
			Errormsg(errno) );
	} else if( (status = server_krb5_auth( keytab, service, principal, *sock,
		&from, errmsg, errlen, tempfile, use_crypt_transfer )) ){
		if( errmsg[0] == 0 ){
			plp_snprintf( errmsg, errlen, "Krb5_receive_work: server_krb5_auth failed - no reason given" );
		}
	} else {
		DEBUGF(DRECV1)("Krb5_receive_work: from '%s'", from );
		Set_str_value( header_info, FROM, from );
		status = do_secure_work( jobsize, from_server, tempfile, header_info );
		if( server_krb5_status( *sock, errmsg, errlen, tempfile, use_crypt_transfer ) ){
			plp_snprintf( errmsg, errlen, "Krb5_receive_work: status send failed - '%s'",
				errmsg );
		}
	}
#if 0
	if( *errmsg ){
		logmsg(LOG_INFO, "%s", errmsg );
	}
#endif
	if( from ) free(from); from = 0;
	return(status);
}

static int Krb5_receive( int *sock,
	int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security, char *tempfile,
	SECURE_WORKER_PROC do_secure_work)
{
	DEBUG1("Krb5_receive: starting");
	return Krb5_receive_work( sock, transfer_timeout,
			user, jobsize, from_server, authtype, info,
			errmsg, errlen, header_info, security, tempfile,
			do_secure_work, 1 );
}

static int Krb5_receive_nocrypt( int *sock,
	int transfer_timeout,
	char *user, char *jobsize, int from_server, char *authtype,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security, char *tempfile,
	SECURE_WORKER_PROC do_secure_work)
{
	DEBUG1("Krb5_receive_nocrypt: starting");
	return Krb5_receive_work( sock, transfer_timeout,
			user, jobsize, from_server, authtype, info,
			errmsg, errlen, header_info, security, tempfile,
			do_secure_work, 0 );
}



/*
 * 
 * The following routines simply implement the encryption and transfer of
 * the files and/or values
 * 
 * By default, when sending a command,  the file will contain:
 *   key=value lines.
 *   KEY           PURPOSE
 *   client        client or user name
 *   from          originator - server if forwarding, client otherwise
 *   command       command to send
 * 
 */

static int Krb5_send_work( int *sock,
	int transfer_timeout,
	char *tempfile,
	char *error, int errlen,
	const struct security *security, struct line_list *info, int use_crypt_transfer )
{
	char *keyfile = 0;
	int status = 0, fd = -1;
	struct stat statb;
	char *principal = 0;
	char *service = 0;
	char *life,  *renew;

	DEBUG1("Krb5_send_work: tempfile '%s', use_crypt_transfer %d", tempfile, use_crypt_transfer );
	life = renew = 0;
	if( Is_server ){
		if( !(keyfile = Find_str_value(info,"keytab")) ){
			plp_snprintf( error, errlen, "no server keytab file" );
			status = JFAIL;
			goto error;
		}
		DEBUG1("Krb5_send_work: keyfile '%s'", keyfile );
		if( (fd = Checkread(keyfile,&statb)) == -1 ){
			plp_snprintf( error, errlen,
				"cannot open server keytab file - %s",
				Errormsg(errno) );
			status = JFAIL;
			goto error;
		}
		close(fd);
		principal = Find_str_value(info,"forward_principal");
	} else {
		if( !(principal = Find_str_value(info,"server_principal")) ){
			principal = Find_str_value(info,"id");
		}
	}
	service = Find_str_value(info, "service" );
	life = Find_str_value(info, "life");
	renew = Find_str_value(info, "renew" );
	status= client_krb5_auth( keyfile, service,
		RemoteHost_DYN, /* remote host */
		principal,	/* principle name of the remote server */
		0,	/* options */
		life,	/* lifetime of server ticket */
		renew,	/* renewable time of server ticket */
		*sock, error, errlen, tempfile, use_crypt_transfer );
	DEBUG1("Krb5_send_work: client_krb5_auth returned '%d' - error '%s'",
		status, error );
	if( status && error[0] == 0 ){
		plp_snprintf( error, errlen,
		"krb5 authenticated transfer to remote host failed");
	}
#if 0
	if( error[0] ){
		DEBUG2("Krb5_send_work: writing error to file '%s'", error );
		if( safestrlen(error) < errlen-2 ){
			memmove( error+1, error, safestrlen(error)+1 );
			error[0] = ' ';
		}
		if( (fd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
			plp_snprintf(error,errlen,
			"Krb5_send_work: open '%s' for write failed - %s",
				tempfile, Errormsg(errno));
		}
		Write_fd_str(fd,error);
		close( fd ); fd = -1;
		error[0] = 0;
	}
#endif
  error:
	return(status);
}

static int Krb5_send( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	const struct security *security, struct line_list *info )
{
	return Krb5_send_work( sock, transfer_timeout,
	tempfile, error, errlen, security, info, 1 );
}

static int Krb5_send_nocrypt( int *sock, int transfer_timeout, char *tempfile,
	char *error, int errlen,
	const struct security *security, struct line_list *info )
{
	return Krb5_send_work( sock, transfer_timeout,
	tempfile, error, errlen, security, info, 0 );
}

const struct security kerberos5_auth =
	{ "kerberos*", "kerberos", "kerberos", IP_SOCKET_ONLY, 0,           Krb5_send, 0, Krb5_receive };
const struct security k5conn_auth =
	{ "k5conn", "k5conn", "kerberos", IP_SOCKET_ONLY, 0,           Krb5_send_nocrypt, 0, Krb5_receive_nocrypt };


#ifdef WITHPLUGINS
plugin_get_func getter_name(kerberos5);
size_t getter_name(kerberos5)(const struct security **s, size_t max) {
	if( max > 0 )
		s[0] = &kerberos5_auth;
	if( max > 1 )
		s[1] = &k5conn_auth;
	return 2;
}
#endif
#endif
