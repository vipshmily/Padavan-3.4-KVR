/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-1999, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

/*
 * This code is, sadly,  a whimpy excuse for the dynamically loadable
 * modules.  The idea is that you can put your user code in here and it
 * will get included in various files.
 * 
 * Supported Sections:
 *   User Authentication
 * 
 *   DEFINES      FILE WHERE INCLUDED PURPOSE
 *   USER_RECEIVE  lpd_secure.c       define the user authentication
 *                                    This is an entry in a table
 *   USER_SEND     sendauth.c         define the user authentication
 *                                    This is an entry in a table
 *   RECEIVE       lpd_secure.c       define the user authentication
 *                            This is the code referenced in USER_RECEIVE
 *   SENDING       sendauth.c       define the user authentication
 *                            This is the code referenced in USER_SEND
 * 
 */

#include "lp.h"
#ifdef WITHPLUGINS
#include <dlfcn.h>
#endif
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
#include "globmatch.h"

#ifdef SSL_ENABLE
/* The Kerberos 5 support is MIT-specific. */
#define OPENSSL_NO_KRB5
# include "ssl_auth.h"
#endif


/**************************************************************
 * Secure Protocol
 *
 * the following is sent on *sock:  
 * \REQ_SECUREprintername C/F user authtype \n        - receive a command
 *             0           1   2   3
 * \REQ_SECUREprintername C/F user authtype jobsize\n - receive a job
 *             0           1   2   3        4
 *          Printer_DYN    |   |   |        + jobsize
 *                         |   |   authtype 
 *                         |  user
 *                        from_server=1 if F, 0 if C
 *                         
 * The authtype is used to look up the security information.  This
 * controls the dispatch and the lookup of information from the
 * configuration and printcap entry for the specified printer
 *
 * The info line_list has the information, stripped of the leading
 * xxxx_ of the authtype name.
 * For example:
 *
 * forward_id=test      <- forward_id from configuration/printcap
 * id=test              <- id from configuration/printcap
 * 
 * If there are no problems with this information, a single 0 byte
 * should be written back at this point, or a nonzero byte with an
 * error message.  The 0 will cause the corresponding transfer
 * to be started.
 * 
 * The handshake and with the remote end should be done now.
 *
 * The client will send a string with the following format:
 * destination=test\n     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test\n          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell\n      <- client id
 *                       (client ID from above)
 * input=%04t1\n          <- input or command
 * This information will be extracted by the server.
 * The 'Do_secure_work' routine can now be called,  and it will do the work.
 * 
 * ERROR MESSAGES:
 *  If you generate an error,  then you should log it.  If you want
 *  return status to be returned to the remote end,  then you have
 *  to take suitable precautions.
 * 1. If the error is detected BEFORE you send the 0 ACK,  then you
 *    can send an error back directly.
 * 2. If the error is discovered as the result of a problem with
 *    the encryption method,  it is strongly recommended that you
 *    simply send a string error message back.  This should be
 *    detected by the remote end,  which will then decide that this
 *    is an error message and not status.
 *
 **************************************************************/

#ifndef WITHPLUGINS
static const struct security *SecuritySupported[] = {
	/* name, server_name, config_name, flags,
        client  connect, send, send_done
		server  accept, receive, receive_done
	*/
#if defined(KERBEROS)
	&kerberos5_auth,
	&k5conn_auth,
#endif
	&test_auth,
	&md5_auth,
#ifdef SSL_ENABLE
	&ssl_auth,
#endif
	NULL
};
#else
static const struct security **SecuritySupported = NULL;

static int loadplugin(const char *filename, const char *realname) {
	void *plugin;
	char symbol[1024];
	int count;
	const struct security **n;
	plugin_get_func *getter;
	size_t got;

	plugin = dlopen(filename, RTLD_NOW | RTLD_LOCAL);
	if( plugin == NULL ) {
		DEBUG1("%s cannot be loaded: %s", filename, dlerror());
		return 0;
	}

	plp_snprintf(symbol, sizeof(symbol), "get_lprng_auth_%d_%s",
			AUTHPLUGINVERSION, realname);
	getter = dlsym(plugin, symbol);
	if( getter == NULL ) {
		DEBUG1("%s has no symbol named %s", filename, symbol);
		dlclose(plugin);
		return 0;
	}
	count = 0;
	if( SecuritySupported != NULL )
		while( SecuritySupported[count] != NULL )
			count++;
	n = realloc(SecuritySupported, sizeof(struct security*)*(count+10));
	if( n == NULL ) {
		dlclose(plugin);
		return 0;
	}
	n[count] = NULL;
	SecuritySupported = n;
	got = getter(n + count, 9);
	if( got < 0 || got > 9 ) {
		n[count] = NULL;
		dlclose(plugin);
		return 0;
	}
	n[count + got] = NULL;
	return 1;
}

static void LoadSecurityPlugin(const char *name) {
	const char *d;
	size_t l;

	if( name == NULL )
		return;
	l = strlen(name);
	d = Plugin_path_DYN;
	if( d == NULL )
		return;
	while( *d != '\0' ) {
		char *filename;
		const char *e;
		struct stat s;
		int i;

		e = strchr(d, ':');
		if( e == NULL )
			e = strchr(d, '\0');
		filename = malloc_or_die((e-d)+l+5, __FILE__, __LINE__);
		memcpy(filename, d, e-d);
		filename[e-d] = '/';
		memcpy(filename + (e-d) + 1, name, l);
		memcpy(filename + (e-d) + 1 + l, ".so", 4);

		i = lstat(filename, &s);
		if( i == 0 && S_ISLNK(s.st_mode) ) {
			size_t reserved = 4096;
			ssize_t linklen;
			char *linkname = malloc_or_die(reserved,
					__FILE__, __LINE__);

			linklen = readlink(filename, linkname, reserved);
			while( linklen >= reserved ) {
				reserved = linklen + 1;
				linkname = realloc_or_die(linkname, reserved,
						__FILE__, __LINE__);
			}
			if( linklen > 0 && linklen < reserved ) {
				char *realname, *ee;

				linkname[linklen] = '\0';

				realname = strrchr(linkname, '/');
				if( realname == NULL )
					realname = linkname;
				else
					realname++;
				ee = strchr(realname, '.');
				if( ee != NULL )
					*ee = '\0';
				if( loadplugin(filename, realname) ) {
					free(filename);
					free(linkname);
					return;
				}

			}
			free(linkname);
		} else if( i == 0 && S_ISREG(s.st_mode) ) {
			if( loadplugin(filename, name) ) {
				free(filename);
				return;
			}
		}
		free(filename);
		d = e;
	}
}

#endif

char *ShowSecuritySupported( char *str, int maxlen )
{
	int i, len;
	const char *name;
	str[0] = 0;

#ifdef WITHPLUGINS
	/* TODO: load all modules here? */
	if( SecuritySupported == NULL )
		return "";
#endif

	for( len = i = 0; SecuritySupported[i] != NULL; ++i ){
		name = SecuritySupported[i]->name;
		plp_snprintf( str+len,maxlen-len, "%s%s",len?",":"",name );
		len += strlen(str+len);
	}
	return( str );
}

const struct security *FindSecurity( const char *name ) {
	const struct security *s, **p;

	for( p = SecuritySupported ; p != NULL && (s = *p) != NULL ; p++ ) {
		if( !Globmatch(s->name, name ) )
			return s;
	}
#ifdef WITHPLUGINS
	LoadSecurityPlugin(name);
	for( p = SecuritySupported ; p != NULL && (s = *p) != NULL ; p++ ) {
		if( !Globmatch(s->name, name ) )
			return s;
	}
#endif
	return NULL;
}
