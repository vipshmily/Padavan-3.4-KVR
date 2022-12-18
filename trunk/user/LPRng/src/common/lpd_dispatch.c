/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

 static char *const _id =
"$Id: lpd_dispatch.c,v 1.74 2004/09/24 20:19:58 papowell Exp $";


#include "lp.h"
#include "errorcodes.h"
#include "getqueue.h"
#include "getprinter.h"
#include "gethostinfo.h"
#include "linksupport.h"
#include "child.h"
#include "fileopen.h"
#include "permission.h"
#include "proctitle.h"
#include "lpd_rcvjob.h"
#include "lpd_remove.h"
#include "lpd_status.h"
#include "lpd_control.h"
#include "lpd_secure.h"
#include "krb5_auth.h"
#include "lpd_dispatch.h"

static void Service_lpd( int talk, const char *from_addr ) NORETURN;

void Dispatch_input(int *talk, char *input, const char *from_addr )
{
	switch( input[0] ){
		default:
			fatal(LOG_INFO,
				_("Dispatch_input: bad request line '%s' from %s"), input, from_addr );
			break;
		case REQ_START:
			/* simply send a 0 ACK and close connection - NOOP */
			Write_fd_len( *talk, "", 1 );
			break;
		case REQ_RECV:
			Receive_job( talk, input );
			break;
		case REQ_DSHORT:
		case REQ_DLONG:
		case REQ_VERBOSE:
			Job_status( talk, input );
			break;
		case REQ_REMOVE:
			Job_remove( talk, input );
			break;
		case REQ_CONTROL:
			Job_control( talk, input );
			break;
		case REQ_BLOCK:
			Receive_block_job( talk, input );
			break;
		case REQ_SECURE:
			Receive_secure( talk, input );
			break;
	}
}

void Service_all( struct line_list *args, int reportfd )
{
	int i, printable, held, move, printing_enabled,
		server_pid, change, error, done, do_service;
	char buffer[SMALLBUFFER], *pr, *forwarding;
	int first_scan;
	char *remove_prefix = 0;

	/* we start up servers while we can */
	Name = "SERVICEALL";
	setproctitle( "lpd %s", Name );

	first_scan = Find_flag_value(args,FIRST_SCAN);
	Free_line_list(args);

	if(All_line_list.count == 0 ){
		Get_all_printcap_entries();
	}
	for( i = 0; i < All_line_list.count; ++i ){
		Set_DYN(&Printer_DYN,0);
		Set_DYN(&Spool_dir_DYN,0);
		pr = All_line_list.list[i];
		DEBUG1("Service_all: checking '%s'", pr );
		if( Setup_printer( pr, buffer, sizeof(buffer), 0) ) continue;
		/* now check to see if there is a server and unspooler process active */
		server_pid = 0;
		remove_prefix = 0;
		if( first_scan ){
			remove_prefix = Fifo_lock_file_DYN;
		}
		server_pid = Read_pid_from_file( Printer_DYN );
		DEBUG3("Service_all: printer '%s' checking server pid %d", Printer_DYN, server_pid );
		if( server_pid > 0 && kill( server_pid, 0 ) == 0 ){
			DEBUG3("Get_queue_status: server %d active", server_pid );
			continue;
		}
		change = Find_flag_value(&Spool_control,CHANGE);
		printing_enabled = !(Pr_disabled(&Spool_control) || Pr_aborted(&Spool_control));

		Free_line_list( &Sort_order );
		if( Scan_queue( &Spool_control, &Sort_order,
				&printable,&held,&move, 1, &error, &done, 0, 0  ) ){
			continue;
		}
		forwarding = Find_str_value(&Spool_control,FORWARDING);
		do_service = 0;
		if( !(Save_when_done_DYN || Save_on_error_DYN )
			&& (Done_jobs_DYN || Done_jobs_max_age_DYN)
			&& (error || done ) ){
			do_service = 1;
		}
		if( do_service || change || move || (printable && (printing_enabled||forwarding)) ){
			if( Server_queue_name_DYN ){
				pr = Server_queue_name_DYN;
			} else {
				pr = Printer_DYN;;
			}
			DEBUG1("Service_all: starting '%s'", pr );
			plp_snprintf(buffer,sizeof(buffer), ".%s\n",pr );
			if( Write_fd_str(reportfd,buffer) < 0 ) cleanup(0);
		}
	}
	Free_line_list( &Sort_order );
	Errorcode = 0;
	cleanup(0);
}

/***************************************************************************
 * Service_connection( struct line_list *args )
 *  Service the connection on the talk socket
 * 1. fork a connection
 * 2. Mother:  close talk and return
 * 2  Child:  close listen
 * 2  Child:  read input line and decide what to do
 *
 ***************************************************************************/

void Service_connection( struct line_list *args, int talk )
{
#ifdef IPP_STUBS
	char input[16];
	int status;		/* status of operation */
#endif /* IPP_STUBS */
	char from_addr[128];
	int permission;
	int port = 0;
	struct sockaddr sinaddr;

	memset( &sinaddr, 0, sizeof(sinaddr) );
	Name = "SERVER";
	setproctitle( "lpd %s", Name );
	(void) plp_signal (SIGHUP, cleanup );

	if( !talk ){
		Errorcode = JABORT;
		fatal(LOG_ERR, "Service_connection: no talk fd");
	}

	DEBUG1("Service_connection: listening fd %d", talk );

	Free_line_list(args);

	/* make sure you use blocking IO */
	Set_block_io(talk);

	{
		socklen_t len;
		len = sizeof( sinaddr );
		if( getpeername( talk, &sinaddr, &len ) ){
			logerr_die(LOG_DEBUG, _("Service_connection: getpeername failed") );
		}
	}

	DEBUG1("Service_connection: family %d, "
#ifdef AF_LOCAL
		"AF_LOCAL %d,"
#endif
#ifdef AF_UNIX
		"AF_UNIX %d"
#endif
	"%s" , sinaddr.sa_family,
#ifdef AF_LOCAL
	AF_LOCAL,
#endif
#ifdef AF_UNIX
	AF_UNIX,
#endif
	"");
	if( sinaddr.sa_family == AF_INET ){
		port = ((struct sockaddr_in *)&sinaddr)->sin_port;
#if defined(IPV6)
	} else if( sinaddr.sa_family == AF_INET6 ){
		port = ((struct sockaddr_in6 * )&sinaddr)->sin6_port;
#endif
	} else if( sinaddr.sa_family == 0
#if defined(AF_LOCAL)
	 	|| sinaddr.sa_family == AF_LOCAL
#endif
#if defined(AF_UNIX)
	 	|| sinaddr.sa_family == AF_UNIX
#endif
		){
		/* force the localhost address */
		int len;
		void *s, *addr;
		memset( &sinaddr, 0, sizeof(sinaddr) );
		Perm_check.unix_socket = 1;
	 	sinaddr.sa_family = Localhost_IP.h_addrtype;
		len = Localhost_IP.h_length;
		if( sinaddr.sa_family == AF_INET ){
			addr = &(((struct sockaddr_in *)&sinaddr)->sin_addr);
#if defined(IPV6)
		} else if( sinaddr.sa_family == AF_INET6 ){
			addr = &(((struct sockaddr_in6 *)&sinaddr)->sin6_addr);
#endif
		} else {
			fatal(LOG_INFO, _("Service_connection: BAD LocalHost_IP value"));
			addr = 0;
		}
		s = Localhost_IP.h_addr_list.list[0];
		memmove(addr,s,len);
	} else {
		fatal(LOG_INFO, _("Service_connection: bad protocol family '%d'"), sinaddr.sa_family );
	}
	inet_ntop_sockaddr( &sinaddr, from_addr, sizeof(from_addr) );
	{
		int len = strlen(from_addr);
		plp_snprintf(from_addr+len,sizeof(from_addr)-len, " port %d", ntohs(port));
	}

	DEBUG2("Service_connection: socket %d, from %s", talk, from_addr );

	/* get the remote name and set up the various checks */

	Get_remote_hostbyaddr( &RemoteHost_IP, &sinaddr, 0 );
	Perm_check.remotehost  =  &RemoteHost_IP;
	Perm_check.host = &RemoteHost_IP;
	Perm_check.port =  ntohs(port);


	/* read the permissions information */

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list, "");
	}

	Perm_check.service = 'X';

	permission = Perms_check( &Perm_line_list, &Perm_check, 0, 0 );
	if( permission == P_REJECT ){
		DEBUG1("Service_connection: no perms on talk socket '%d' from %s", talk, from_addr );
		safefprintf(talk, "\001%s\n", _("no connect permissions"));
		cleanup(0);
	}

#ifdef IPP_STUBS
	memset(input,0,sizeof(input));

	do {
		int my_len = sizeof( input ) - 1;
		static int timeout;
		timeout = (Send_job_rw_timeout_DYN>0)?Send_job_rw_timeout_DYN:
					((Connect_timeout_DYN>0)?Connect_timeout_DYN:10);
		DEBUG1( "Service_connection: doing peek for %d on fd %d, timeout %d",
			my_len, talk, timeout );
		if( Set_timeout() ){
			Set_timeout_alarm( timeout );
			status = recv( talk, input, my_len, MSG_PEEK );
		} else {
			status = -1;
		}
		Clear_timeout();

		if( status <= 0 ){
			logerr_die(LOG_DEBUG, _("Service_connection: peek of length %d failed"), my_len );
		}
		DEBUG1("Service_connection: status %d 0x%02x%02x%02x%02x (%c%c%c%c)", status,
			cval(input+0), cval(input+1), cval(input+2), cval(input+3),
			cval(input+0), cval(input+1), cval(input+2), cval(input+3));
	} while( status < 2 );

	if( isalpha(cval(input+0)) &&
		isalpha(cval(input+1)) && isalpha(cval(input+2)) ){
		/* 
			Service_ipp( talk, from_addr );
		*/
	} else if( cval(input+0) == 0x80 ) {
		/*
			Service_ssh_ipp( talk, from_addr );
		*/
	}
#endif /* not IPP_STUBS */
	Service_lpd( talk, from_addr );
}

static void Service_lpd( int talk, const char *from_addr )
{
	char input[LINEBUFFER];
	int status;
	int len = sizeof( input ) - 1;
	int timeout = (Send_job_rw_timeout_DYN>0)?Send_job_rw_timeout_DYN:
					((Connect_timeout_DYN>0)?Connect_timeout_DYN:10);

	memset(input,0,sizeof(input));
	DEBUG1( "Service_connection: starting read on fd %d, timeout %d", talk, timeout );

	status = Link_line_read(ShortRemote_FQDN,&talk,
		timeout,input,&len);
	if( len >= 0 ) input[len] = 0;
	DEBUG1( "Service_connection: read status %d, len %d, '%s'",
		status, len, input );
	if( len == 0 ){
		DEBUG3( "Service_connection: zero length read" );
		cleanup(0);
	}
	if( status ){
		logerr_die(LOG_DEBUG, _("Service_connection: cannot read request from %s in %d seconds"),
			from_addr, timeout );
	}
	if( len < 2 ){
		fatal(LOG_INFO, _("Service_connection: short request line '%s', from '%s'"),
			input, from_addr );
	}
	Dispatch_input(&talk,input,from_addr);
	cleanup(0);
}
