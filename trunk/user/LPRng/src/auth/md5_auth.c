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

/* 
 * md5 authentication
 
 
 Connection Level: md5_connect and md5_verify
 
 The client and server do a handshake, exchanging salts and values as follows:
 Client connects to server
      Server sends 'salt'
 Client gets user and user secret 
        gets server id and server secret 
        makes 'userid serverid'
        hashs the whole mess and gets md5 hash (see code)
        sends 'userid serverid XXXXXXX' where XXXXX is md5 hash
 Server checks userid and server, generates the same hash
        if values match, all is happy
 
 File Transfer: md5_send and md5 receive
 
 Client connects to server
     Server sends 'salt'
 Client
        get user and user secret
        gets server id and server secret 
        hashes the mess and the file
        Sends the hash followed by the file
 Server reads the file
        gets the hash
        dumps the rest of stuff into a file
        get user and user secret
        gets server id and server secret 
        hashes the mess and the file
        checks to see that all is well
        performs action
  Client keyfile:
    xxx=yyy key   where xxx is md5_id=xxx value from printcap
        yyy should be sent to server as the 'from' value
  Server keyfile:
    yyy=key       where yyy is the 'from' id value

*/


#include "md5.h"

/* Set the name of the file we'll be getting our key from.
   The keyfile should only be readable by the owner and
   by whatever group the user programs run as.
   This way, nobody can write their own authentication module */

/* key length (for MD5, it's 16) */
#define KEY_LENGTH MD5_KEY_LENGTH

/* The md5 hashing function, which does the real work */
static void MDString( const unsigned char *inString, unsigned char *outstring, int inlen)
{
	MD5_CONTEXT mdContext;

	MD5Init (&mdContext);
	MD5Update(&mdContext, inString, inlen);
	MD5Final(&mdContext, outstring);
}

static void MDFile( int fd, unsigned char *outstring )
{
	MD5_CONTEXT mdContext;
	unsigned char buffer[LARGEBUFFER];
	int n;

	MD5Init (&mdContext);
	while( (n = ok_read( fd, buffer, sizeof(buffer))) > 0 ){
		MD5Update(&mdContext, buffer, n);
	}
	MD5Final(&mdContext, outstring);
}

static char *hexstr( const unsigned char *str, int len, char *outbuf, int outlen )
{
	int i, j;
	for( i = 0; i < len && 2*(i+1) < outlen ; ++i ){
		j = ((unsigned char *)str)[i];
		plp_snprintf(&outbuf[2*i],4, "%02x",j);
	}
	if( outlen > 0 ) outbuf[2*i] = 0;
	return( outbuf );
}

static int md5key( const char *keyfile, char *name, char *key, int keysize, char *errmsg, int errlen )
{
	const char *keyvalue;
	int i,  keylength = -1;
	struct line_list keys;

	Init_line_list( &keys );
	memset(key,0,keysize);
	/*
		void Read_file_list( int required, struct line_list *model, char *str,
			const char *linesep, int sort, const char *keysep, int uniq, int trim,
			int marker, int doinclude, int nocomment, int depth, int maxdepth )
	*/
	Read_file_list( /*required*/0, /*model*/&keys, /*str*/(char *)keyfile,
		/*linesep*/Line_ends,/*sort*/1, /*keysep*/Option_value_sep,/*uniq*/1, /*trim*/1,
		/*marker*/0, /*doinclude*/0, /*nocomment*/1,/*depth*/0,/*maxdepth*/4 );
	/* read in the key from the key file */
	keyvalue = Find_exists_value( &keys, name,Hash_value_sep );
	if( keyvalue == 0 ){
		plp_snprintf(errmsg, errlen,
		"md5key: no key for '%s' in '%s'", name, keyfile );
		goto error;
	}
	DEBUG1("md5key: key '%s'", keyvalue );

	/* copy to string */
	for(i = 0; keyvalue[i] && i < keysize; ++i ){
		key[i] = keyvalue[i];
	}
	keylength = i;

 error:
	Free_line_list( &keys );
	return( keylength );
}

/**************************************************************
 *md5_send:
 *A simple implementation for testing user supplied authentication
 * The info line_list has the following fields
 * client=papowell      <- client id, can be forwarded
 * destination=test     <- destination ID - use for remote key
 * forward_id=test      <- forward_id from configuration/printcap
 * from=papowell        <- originator ID  - use for local key
 * id=test              <- id from configuration/printcap
 *
 * The start of the actual file has:
 * destination=test     <- destination ID (URL encoded)
 *                       (destination ID from above)
 * server=test          <- if originating from server, the server key (URL encoded)
 *                       (originator ID from above)
 * client=papowell      <- client id
 *                       (client ID from above)
 * input=%04t1          <- input that is 
 *   If you need to set a 'secure id' then you set the 'FROM'
 *
 **************************************************************/

static int md5_send( int *sock, int transfer_timeout, char *tempfile,
	char *errmsg, int errlen,
	const struct security *security UNUSED, struct line_list *info )
{
	unsigned char destkey[KEY_LENGTH+1];
	unsigned char challenge[KEY_LENGTH+1];
	unsigned char response[KEY_LENGTH+1];
	unsigned char filehash[KEY_LENGTH+1];
	int destkeylength, i, n;
	char smallbuffer[SMALLBUFFER];
	char keybuffer[SMALLBUFFER];
	char *s, *dest;
	const char *keyfile;
	char buffer[LARGEBUFFER];
	struct stat statb;
	int tempfd = -1, len, ack;
	int status = 0;

	errmsg[0] = 0;
	if(DEBUGL1)Dump_line_list("md5_send: info", info );
	if( !Is_server ){
		/* we get the value of the MD5KEYFILE variable */
		keyfile = getenv("MD5KEYFILE");
		if( keyfile == 0 ){
			plp_snprintf(errmsg, errlen,
				"md5_send: no MD5KEYFILE environment variable" );
			goto error;
		}
	} else {
		keyfile = Find_exists_value( info, "server_keyfile",Hash_value_sep );
		if( keyfile == 0 ){
			plp_snprintf(errmsg, errlen,
				"md5_send: no md5_server_keyfile entry" );
			goto error;
		}
	}

	dest = Find_str_value( info, DESTINATION );
	if( dest == 0 ){
		plp_snprintf(errmsg, errlen,
			"md5_send: no '%s' value in info", DESTINATION );
		goto error;
	}
	if( (destkeylength = md5key( keyfile, dest, keybuffer,
		sizeof(keybuffer), errmsg, errlen ) ) <= 0 ){
		goto error;
	}
	if( (s = strpbrk(keybuffer,Whitespace)) ){
		*s++ = 0;
		while( (isspace(cval(s))) ) ++s;
		if( *s == 0 ){
			plp_snprintf(errmsg, errlen,
				"md5_send: no '%s' value in keyfile", dest );
			goto error;
		}
		dest = keybuffer;
	} else {
		s = keybuffer;
		dest = Find_str_value( info, FROM );
		if( !dest ){
			plp_snprintf(errmsg,errlen,
				"md5_send: no '%s' value in info", FROM );
			goto error;
		}
	}
	destkeylength = safestrlen(s);
	if( destkeylength > KEY_LENGTH ) destkeylength = KEY_LENGTH;
	memcpy( destkey, s, destkeylength );

	DEBUG1("md5_send: sending on socket %d", *sock );
	/* Read the challenge dest server */
	len = sizeof(buffer);
	if( (n = Link_line_read(ShortRemote_FQDN,sock,
		transfer_timeout,buffer,&len)) ){
		plp_snprintf(errmsg, errlen,
		"md5_send: error reading challenge - '%s'", Link_err_str(n) );
		goto error;
	} else if( len == 0 ){
		plp_snprintf(errmsg, errlen,
		"md5_send: zero length challenge");
		goto error;
	}
	DEBUG1("md5_send: challenge '%s'", buffer );
	n = safestrlen(buffer);
	if( n == 0 || n % 2 || n/2 > KEY_LENGTH ){
		plp_snprintf(errmsg, errlen,
		"md5_send: bad challenge length '%d'", safestrlen(buffer) );
		goto error;
	}
	memset(challenge, 0, sizeof(challenge));
	smallbuffer[2] = 0;
	for(i = 0; buffer[2*i] && i < KEY_LENGTH; ++i ){
		memcpy(smallbuffer,&buffer[2*i],2);
		challenge[i] = strtol(smallbuffer,0,16);
	}

	DEBUG1("md5_send: decoded challenge '%s'",
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));
	DEBUG1("md5_send: destkey '%s'", 
		hexstr( destkey, KEY_LENGTH, buffer, sizeof(buffer) ));
	/* xor the challenge with the dest key */
	n = 0;
	len = destkeylength;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^destkey[n]);
		n=(n+1)%len;
	}
	DEBUG1("md5_send: challenge^destkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUG1("md5_send: challenge^destkey^idkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUG1("md5_send: opening tempfile '%s'", tempfile );

	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		plp_snprintf(errmsg, errlen,
			"md5_send: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("md5_send: doing md5 of file");
	MDFile( tempfd, filehash);
	DEBUG1("md5_send: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* xor the challenge with the file key */
	n = 0;
	len = KEY_LENGTH;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^filehash[n]);
		n=(n+1)%len;
	}

	DEBUG1("md5_send: challenge^destkey^idkey^filehash '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	plp_snprintf( smallbuffer, sizeof(smallbuffer), "%s", dest );

	/* now we xor the buffer with the key */
	len = safestrlen(smallbuffer);
	DEBUG1("md5_send: idstuff len %d '%s'", len, smallbuffer );
	n = 0;
	for(i = 0; i < len; i++){
		challenge[n] = (challenge[n]^smallbuffer[i]);
		n=(n+1)%KEY_LENGTH;
	}
	DEBUG1("md5_send: result challenge^destkey^idkey^filehash^idstuff '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* now, MD5 hash the string */
	MDString(challenge, response, KEY_LENGTH);

	/* return the response to the server */
	hexstr( response, KEY_LENGTH, buffer, sizeof(buffer) );
	n = safestrlen(smallbuffer);
	plp_snprintf(smallbuffer+n, sizeof(smallbuffer)-n-1, " %s", buffer );
	DEBUG1("md5_send: sending response '%s'", smallbuffer );
	safestrncat(smallbuffer,"\n");
	ack = 0;
	if( (n =  Link_send( RemoteHost_DYN, sock, transfer_timeout,
		smallbuffer, safestrlen(smallbuffer), &ack )) || ack ){
		/* keep the other end dest trying to read */
		if( (s = strchr(smallbuffer,'\n')) ) *s = 0;
		plp_snprintf(errmsg,errlen,
			"error '%s'\n sending str '%s' to %s@%s",
			Link_err_str(n), smallbuffer,
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	if( lseek( tempfd, 0, SEEK_SET ) == -1 ){
		plp_snprintf(errmsg,errlen,
			"md5_send: seek failed - '%s'", Errormsg(errno) );
		goto error;
	}

	DEBUG1("md5_send: starting transfer of file");
	while( (len = Read_fd_len_timeout( transfer_timeout, tempfd, buffer, sizeof(buffer)-1 )) > 0 ){
		buffer[len] = 0;
		DEBUG4("md5_send: file information '%s'", buffer );
		if( write( *sock, buffer, len) != len ){
			plp_snprintf(errmsg, errlen,
				"md5_send: write to socket failed - %s", Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	if( len < 0 ){
		plp_snprintf(errmsg, errlen,
			"md5_send: read dest '%s' failed - %s", tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	close(tempfd); tempfd = -1;
	/* we close the writing side */
	shutdown( *sock, 1 );

	DEBUG1("md5_send: sent file" );

	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0){
		plp_snprintf(errmsg, errlen,
			"md5_send: open '%s' for write failed - %s",
			tempfile, Errormsg(errno) );
		status = JABORT;
		goto error;
	}
	DEBUG1("md5_send: starting read of response");

	if( (len = Read_fd_len_timeout(transfer_timeout, *sock,buffer,1)) > 0 ){
		n = cval(buffer);
		DEBUG4("md5_send: response byte '%d'", n);
		status = n;
		if( isprint(n) && write(tempfd,buffer,1) != 1 ){
			plp_snprintf(errmsg, errlen,
				"md5_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	while( (len = Read_fd_len_timeout(transfer_timeout, *sock,buffer,sizeof(buffer)-1)) > 0 ){
		buffer[len] = 0;
		DEBUG4("md5_send: socket information '%s'", buffer);
		if( write(tempfd,buffer,len) != len ){
			plp_snprintf(errmsg, errlen,
				"md5_send: write to '%s' failed - %s", tempfile, Errormsg(errno) );
			status = JABORT;
			goto error;
		}
	}
	close( tempfd ); tempfd = -1;

 error:
	if( tempfd >= 0 ) close(tempfd); tempfd = -1;
	return(status);
}


static int md5_receive( int *sock, int transfer_timeout,
	char *user UNUSED, char *jobsize, int from_server, char *authtype UNUSED,
	struct line_list *info,
	char *errmsg, int errlen,
	struct line_list *header_info,
	const struct security *security UNUSED, char *tempfile,
	SECURE_WORKER_PROC do_secure_work)
{
	char input[SMALLBUFFER];
	char buffer[LARGEBUFFER];
	char keybuffer[LARGEBUFFER];
	int destkeylength, i, n, len, tempfd = -1;
	char *s, *dest, *hash;
	const char *keyfile;
	unsigned char destkey[KEY_LENGTH+1];
	unsigned char challenge[KEY_LENGTH+1];
	unsigned char response[KEY_LENGTH+1];
	unsigned char filehash[KEY_LENGTH+1];
	struct stat statb;
	int status_error = 0;
	double size;


	if(DEBUGL1)Dump_line_list("md5_receive: info", info );
	if(DEBUGL1)Dump_line_list("md5_receive: header_info", header_info );
	/* do validation and then write 0 */

	if( !Is_server ){
		plp_snprintf(errmsg, errlen,
			"md5_receive: not server" );
		goto error;
	} else {
		keyfile = Find_exists_value( info, "server_keyfile",Hash_value_sep );
		if( keyfile == 0 ){
			plp_snprintf(errmsg, errlen,
				"md5_receive: no md5_server_keyfile entry" );
			goto error;
		}
	}

	DEBUGF(DRECV1)("md5_receive: sending ACK" );
	if( (n = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		"", 1, 0 )) ){
		plp_snprintf(errmsg,errlen,
			"error '%s' ACK to %s@%s",
			Link_err_str(n), RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}
	/* First, seed the random number generator */
	srand(time(NULL));

	/* Now, fill the challenge with 16 random values */
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = rand() >> 8;
	}
	hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) );
	DEBUGF(DRECV1)("md5_receive: sending challenge '%s'", buffer );
	safestrncat(buffer,"\n");

	/* Send the challenge to the client */

	if( (n = Link_send( RemoteHost_DYN, sock, transfer_timeout,
		buffer, safestrlen(buffer), 0 )) ){
		/* keep the other end dest trying to read */
		if( (s = strchr(buffer,'\n')) ) *s = 0;
		plp_snprintf(errmsg,errlen,
			"error '%s' sending str '%s' to %s@%s",
			Link_err_str(n), buffer,
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	/* now read response */
	DEBUGF(DRECV1)("md5_receive: reading response");
	len = sizeof(input)-1;
	if( (n = Link_line_read(ShortRemote_FQDN,sock,
		transfer_timeout,input,&len) )){
		plp_snprintf(errmsg, errlen,
		"md5_receive: error reading challenge - '%s'", Link_err_str(n) );
		goto error;
	} else if( len == 0 ){
		plp_snprintf(errmsg, errlen,
		"md5_receive: zero length response");
		goto error;
	} else if( len >= (int)sizeof( input) -2 ){
		plp_snprintf(errmsg, errlen,
		"md5_receive: response too long");
		goto error;
	}
	DEBUGF(DRECV1)("md5_receive: response '%s'", input );

	dest = input;
	if( (s = strchr(input,' ')) ) *s++ = 0;
	if( s ){
		hash = s;
		if( strpbrk(hash,Whitespace) ){
			plp_snprintf(errmsg, errlen,
				"md5_receive: malformed response" );
			goto error;
		}
		n = safestrlen(hash);
		if( n == 0 || n%2 ){
			plp_snprintf(errmsg, errlen,
			"md5_receive: bad response hash length '%d'", n );
			goto error;
		}
	} else {
		plp_snprintf(errmsg, errlen,
			"md5_receive: no 'hash' in response" );
		goto error;
	}


	DEBUGF(DRECV1)("md5_receive: dest '%s', hash '%s', prefix '%s'",
		dest, hash, buffer );
	if( (destkeylength = md5key( keyfile, dest, keybuffer, KEY_LENGTH, errmsg, errlen ) ) <= 0 ){
		goto error;
	}
	if( (s = strpbrk(keybuffer,Whitespace)) ){
		*s++ = 0;
		while( isspace(cval(s))) ++s;
	} else {
		s = keybuffer;
	}
	destkeylength = safestrlen(s);
	if( destkeylength > KEY_LENGTH ) destkeylength = KEY_LENGTH;
	memcpy(destkey,s,destkeylength);

	
	DEBUGF(DRECV1)("md5_receive: success, sending ACK" );

	if((n = Link_send( RemoteHost_DYN, sock, transfer_timeout, "", 1, 0 )) ){
		/* keep the other end dest trying to read */
		plp_snprintf(errmsg,errlen,
			"error '%s' sending ACK to %s@%s",
			Link_err_str(n),
			RemotePrinter_DYN, RemoteHost_DYN );
		goto error;
	}

	DEBUGF(DRECV1)("md5_receive: reading file" );

	/* open a file for the output */
	if( (tempfd = Checkwrite(tempfile,&statb,O_WRONLY|O_TRUNC,1,0)) < 0 ){
		plp_snprintf(errmsg, errlen,
			"md5_receive: reopen of '%s' for write failed",
			tempfile );
	}

	DEBUGF(DRECV1)("md5_receive: starting read dest socket %d", *sock );
	while( (n = Read_fd_len_timeout(transfer_timeout, *sock, buffer,sizeof(buffer)-1)) > 0 ){
		buffer[n] = 0;
		DEBUGF(DRECV4)("md5_receive: remote read '%d' '%s'", n, buffer );
		if( write( tempfd,buffer,n ) != n ){
			plp_snprintf(errmsg, errlen,
				"md5_receive: bad write to '%s' - '%s'",
				tempfile, Errormsg(errno) );
			goto error;
		}
	}

	if( n < 0 ){
		plp_snprintf(errmsg, errlen,
		"md5_receive: bad read '%d' reading file ", n );
		goto error;
	}
	close(tempfd); tempfd = -1;
	DEBUGF(DRECV4)("md5_receive: end read" );

	DEBUG1("md5_receive: opening tempfile '%s'", tempfile );

	if( (tempfd = Checkread(tempfile,&statb)) < 0){
		plp_snprintf(errmsg, errlen,
			"md5_receive: open '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	DEBUG1("md5_receive: doing md5 of file");
	MDFile( tempfd, filehash);
	DEBUG1("md5_receive: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));
	close(tempfd); tempfd = -1;

	DEBUGF(DRECV1)("md5_receive: challenge '%s'",
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));
	DEBUGF(DRECV1)("md5_receive: destkey '%s'", 
		hexstr( destkey, KEY_LENGTH, buffer, sizeof(buffer) ));
	/* xor the challenge with the dest key */
	n = 0;
	len = destkeylength;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^destkey[n]);
		n=(n+1)%len;
	}
	DEBUGF(DRECV1)("md5_receive: challenge^destkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	DEBUGF(DRECV1)("md5_receive: challenge^destkey^idkey '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));


	DEBUGF(DRECV1)("md5_receive: filehash '%s'", 
		hexstr( filehash, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* xor the challenge with the file key */
	n = 0;
	len = KEY_LENGTH;
	for(i = 0; i < KEY_LENGTH; i++){
		challenge[i] = (challenge[i]^filehash[n]);
		n=(n+1)%len;
	}
	DEBUGF(DRECV1)("md5_receive: challenge^destkey^idkey^filehash '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));


	/* now we xor the buffer with the key */
	len = safestrlen(input);
	DEBUGF(DRECV1)("md5_receive: idstuff len %d '%s'", len, input );
	n = 0;
	for(i = 0; i < len; i++){
		challenge[n] = (challenge[n]^(unsigned char)input[i]);
		n=(n+1)%KEY_LENGTH;
	}
	DEBUGF(DRECV1)("md5_receive: result challenge^destkey^idkey^filehash^deststuff '%s'", 
		hexstr( challenge, KEY_LENGTH, buffer, sizeof(buffer) ));

	/* now, MD5 hash the string */
	MDString(challenge, response, KEY_LENGTH);

	hexstr( response, KEY_LENGTH, buffer, sizeof(buffer) );

	DEBUGF(DRECV1)("md5_receive: calculated hash '%s'", buffer );
	DEBUGF(DRECV1)("md5_receive: sent hash '%s'", hash );

	if( strcmp( buffer, hash ) ){
		plp_snprintf(errmsg, errlen,
		"md5_receive: bad response value");
		goto error;
	}
	
	DEBUGF(DRECV1)("md5_receive: success" );
	Set_str_value(header_info,FROM,dest);
	status_error = do_secure_work( jobsize, from_server, tempfile, header_info );
	DEBUGF(DRECV1)("md5_receive: Do_secure_work returned %d", status_error );

	/* we now have the encoded output */
	if( (tempfd = Checkread(tempfile,&statb)) < 0 ){
		plp_snprintf( errmsg, errlen,
			"md5_receive: reopen of '%s' for read failed - %s",
			tempfile, Errormsg(errno) );
		goto error;
	}
	size = statb.st_size;
	DEBUGF(DRECV1)( "md5_receive: return status encoded size %0.0f", size);
	if( size || status_error ){
		buffer[0] = ACK_FAIL;
		write( *sock,buffer,1 );
		while( (n = Read_fd_len_timeout(transfer_timeout, tempfd, buffer,sizeof(buffer)-1)) > 0 ){
			buffer[n] = 0;
			DEBUGF(DRECV4)("md5_receive: sending '%d' '%s'", n, buffer );
			if( write( *sock,buffer,n ) != n ){
				plp_snprintf( errmsg, errlen,
					"md5_receive: bad write to socket - '%s'",
					Errormsg(errno) );
				goto error;
			}
		}
		if( n < 0 ){
			plp_snprintf( errmsg, errlen,
				"md5_receive: read '%s' failed - %s",
				tempfile, Errormsg(errno) );
			goto error;
		}
	} else {
		write( *sock,"",1 );
	}
	return( 0 );


 error:
	return(JFAIL);
}


const struct security md5_auth =
	{ "md5",       "md5",	"md5",      0,              0,           md5_send, 0, md5_receive };

#ifdef WITHPLUGINS
plugin_get_func getter_name(md5);
size_t getter_name(md5)(const struct security **s, size_t max) {
	if( max > 0 )
		*s = &md5_auth;
	return 1;
}
#endif
