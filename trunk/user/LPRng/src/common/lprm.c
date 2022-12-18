/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************
 * MODULE: lprm.c
 * PURPOSE:
 **************************************************************************/

/***************************************************************************
 * SYNOPSIS
 *      lprm [ -PPrinter_DYN ]
 *    lprm [-Pprinter ]*[-a][-s][-l][+[n]][-Ddebugopt][job#][user]
 * DESCRIPTION
 *   lprm sends a status request to lpd(8)
 *   and reports the status of the
 *   specified jobs or all  jobs  associated  with  a  user.  lprm
 *   invoked  without  any arguments reports on the printer given
 *   by the default printer (see -P option).  For each  job  sub-
 *   mitted  (i.e.  invocation  of lpr(1)) lprm reports the user's
 *   name, current rank in the queue, the names of files compris-
 *   ing  the job, the job identifier (a number which may be sup-
 *   plied to lprm(1) for removing a specific job), and the total
 *   size  in  bytes.  Job ordering is dependent on the algorithm
 *   used to scan the spooling directory and is  FIFO  (First  in
 *   First Out), in order of priority level.  File names compris-
 *   ing a job may be unavailable (when lpr(1) is used as a  sink
 *   in  a  pipeline)  in  which  case  the  file is indicated as
 *   ``(STDIN)''.
 *    -P printer
 *         Specifies a particular printer, otherwise  the  default
 *         line printer is used (or the value of the PRINTER vari-
 *         able in the environment).  If PRINTER is  not  defined,
 *         then  the  first  entry in the /etc/printcap(5) file is
 *         reported.  Multiple printers can be displayed by speci-
 *         fying more than one -P option.
 *
 *   -a   All printers listed in the  /etc/printcap(5)  file  are
 *        reported.
 *
 *   -l   An alternate  display  format  is  used,  which  simply
 *        reports the user, jobnumber, and originating host.
 *
 *   [+[n]]
 *        Forces lprm to periodically display  the  spool  queues.
 *        Supplying  a  number immediately after the + sign indi-
 *        cates that lprm should sl eep n seconds in between  scans
 *        of the queue.
 *        Note: the screen will be cleared at the start of each
 *        display using the 'curses.h' package.
 ****************************************************************************
 *
 * Implementation Notes
 * Patrick Powell Tue May  2 09:58:29 PDT 1995
 * 
 * The LPD server will be returning the formatted status;
 * The format can be the following:
 * 
 * SHORT:
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * Rank   Owner      Job  Files                                 Total Size
 * active root       30   standard input                        5 bytes
 * 2nd    root       31   standard input                        5 bytes
 * 
 * LONG:
 * 
 * Warning: lp is down: lp is ready and printing
 * Warning: no daemon present
 * 
 * root: 1st                                [job 030taco]
 *         standard input                   5 bytes
 * 
 * root: 2nd                                [job 031taco]
 *         standard input                   5 bytes
 * 
 */

#include "lp.h"
#include "child.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "initialize.h"
#include "linksupport.h"
#include "sendreq.h"
#include "user_auth.h"

/**** ENDINCLUDE ****/

#undef EXTERN
#undef DEFINE
#define EXTERN
#define DEFINE(X) X
#include "lprm.h"
/**** ENDINCLUDE ****/

 static char *Username_JOB;

/***************************************************************************
 * main()
 * - top level of LPRM
 *
 ****************************************************************************/

int main(int argc, char *argv[], char *envp[])
{
	int i;
	struct line_list args;

	Init_line_list(&args);

	/* set signal handlers */
	(void) plp_signal (SIGHUP, cleanup_HUP);
	(void) plp_signal (SIGINT, cleanup_INT);
	(void) plp_signal (SIGQUIT, cleanup_QUIT);
	(void) plp_signal (SIGTERM, cleanup_TERM);
	(void) signal (SIGCHLD, SIG_DFL);
	(void) signal (SIGPIPE, SIG_IGN);

	/*
	 * set up the user state
	 */
#ifndef NODEBUG
	Debug = 0;
#endif


	Initialize(argc, argv, envp, 'D' );
	Setup_configuration();
	Get_parms(argc, argv);      /* scan input args */
	if( Auth && !getenv( "AUTH" ) ){
		FPRINTF(STDERR,
		_("authentication requested (-A option) and AUTH environment variable not set") );
		usage();
	}

	/* now force the printer list */
	if( All_printers || (Printer_DYN && safestrcasecmp(Printer_DYN,ALL) == 0 ) ){
		All_printers = 1;
		Get_all_printcap_entries();
		if(DEBUGL1)Dump_line_list("lprm - final All_line_list", &All_line_list);
	}
	DEBUG1("lprm: Printer_DYN '%s', All_printers %d, All_line_list.count %d",
		Printer_DYN, All_printers, All_line_list.count );
	if( Username_JOB && OriginalRUID ){
		struct line_list user_list;
		char *str, *t;
		struct passwd *pw;
		int found;
		uid_t uid;

		DEBUG2("lprm: checking '%s' for -U perms",
			Allow_user_setting_DYN );
		Init_line_list(&user_list);
		Split( &user_list, Allow_user_setting_DYN,File_sep,0,0,0,0,0,0);
		
		found = 0;
		for( i = 0; !found && i < user_list.count; ++i ){
			str = user_list.list[i];
			DEBUG2("lprm: checking '%s'", str );
			uid = strtol( str, &t, 10 );
			if( str == t || *t ){
				/* try getpasswd */
				pw = getpwnam( str );
				if( pw ){
					uid = pw->pw_uid;
				}
			}
			DEBUG2( "lprm: uid '%ld'", (long)uid );
			found = ( uid == OriginalRUID );
			DEBUG2( "lprm: found '%ld'", (long)found );
		}
		if( !found ){
			DEBUG1( "-U (username) can only be used by ROOT or authorized users" );
			Username_JOB = 0;
		}
	}
	if( Username_JOB ){
		Set_DYN( &Logname_DYN, Username_JOB );
	}
	Add_line_list(&args,Logname_DYN,0,0,0);
	for( i = Optind; argv[i]; ++i ){
		Add_line_list(&args,argv[i],0,0,0);
	}
	Check_max(&args,2);
	args.list[args.count] = 0;

	if( All_printers ){
		if( All_line_list.count == 0 ){
			FPRINTF(STDERR,"no printers\n");
			cleanup(0);
		}
		for( i = 0; i < All_line_list.count; ++i ){
			Set_DYN(&Printer_DYN,All_line_list.list[i] );
			Do_removal(args.list);
		}
	} else {
		Get_printer();
		Do_removal(args.list);
	}
	Free_line_list(&args);
	DEBUG1("lprm: done");
	Remove_tempfiles();
	DEBUG1("lprm: tempfiles removed");
	Errorcode = 0;
	DEBUG1("lprm: cleaning up");
	cleanup(0);
}


void Do_removal(char **argv)
{
	int fd, n;
	char msg[LINEBUFFER];

	DEBUG1("Do_removal: start");
	Fix_Rm_Rp_info(0,0);

	if( ISNULL(RemotePrinter_DYN) ){
		plp_snprintf( msg, sizeof(msg),
			_("Printer: %s - cannot remove jobs from device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}

	/* fix up authentication */
	if( Auth ){
		Set_DYN(&Auth_DYN, getenv("AUTH"));
	}
	if( Check_for_rg_group( Logname_DYN ) ){
		plp_snprintf( msg, sizeof(msg),
			_("Printer: %s - not in privileged group\n"),
			Printer_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	if( Direct_DYN && Lp_device_DYN ){
		plp_snprintf( msg, sizeof(msg),
			_("Printer: %s - direct connection to device '%s'\n"),
			Printer_DYN, Lp_device_DYN );
		if(  Write_fd_str( 1, msg ) < 0 ) cleanup(0);
		return;
	}
	fd = Send_request( 'M', REQ_REMOVE,
		argv, Connect_timeout_DYN, Send_query_rw_timeout_DYN, 1 );
	if( fd > 0 ){
		shutdown( fd, 1 );
		while( (n = Read_fd_len_timeout(Send_query_rw_timeout_DYN,
			fd, msg, sizeof(msg)) ) > 0 ){
			if( write(1,msg,n) < 0 ) cleanup(0);
		}
		close(fd);
	}
	DEBUG1("Do_removal: end");
}

/***************************************************************************
 * void Get_parms(int argc, char *argv[])
 * 1. Scan the argument list and get the flags
 * 2. Check for duplicate information
 ***************************************************************************/

 extern char *next_opt;
static void Get_parms_clean(int argc, char *argv[] );
static void Get_parms_lprm(int argc, char *argv[] );

void Get_parms(int argc, char *argv[] )
{
	if( argv[0] && (Name = strrchr( argv[0], '/' )) ) {
		++Name;
	} else {
		Name = argv[0];
	}
	/* check to see if we simulate (poorly) the LP options */
	if( Name && safestrcmp( Name, "clean" ) == 0 ){
		LP_mode = 1;
		Get_parms_clean(argc,argv);
	}
	else {
		Get_parms_lprm(argc,argv);
	}

	if( Verbose ){
		FPRINTF( STDERR, "%s\n", Version );
		if( Verbose > 1 ) Printlist( Copyright, 1 );
	}
}


static void Get_parms_lprm(int argc, char *argv[] )
{
	int option;
	while ((option = Getopt (argc, argv, "AaD:P:U:V" )) != EOF)
		switch (option) {
		case 'A': Auth = 1; break;
		case 'a': All_printers = 1; Set_DYN(&Printer_DYN,"all"); break;
		case 'D': Parse_debug(Optarg, 1); break;
		case 'V': ++Verbose; break;
		case 'U': Username_JOB = Optarg; break;
		case 'P': Set_DYN(&Printer_DYN, Optarg); break;
		default: usage(); break;
		}
}

static void Get_parms_clean(int argc, char *argv[] )
{
	char *name;
	int option;

	while ((option = Getopt (argc, argv, "AD:" )) != EOF)
		switch (option) {
		case 'A': Auth = 1; break;
		case 'D':
			Parse_debug( Optarg, 1 );
			break;
		default: usage(); break;
		}

	if( Optind < argc ){
		name = argv[argc-1];
		Get_all_printcap_entries();

		if( safestrcasecmp(name,ALL) != 0){
			if( Find_exists_value( &All_line_list, name, Hash_value_sep ) ){
				Set_DYN(&Printer_DYN,name);
				argv[argc-1] = 0;
			}
		} else {
			All_printers = 1;
			Set_DYN(&Printer_DYN,"all");
		}

	}
}

static void usage(void)
{
	if( !LP_mode ){
		FPRINTF(STDERR,
_(" usage: %s [-A] [-a | -Pprinter] [-Ddebuglevel] (jobid|user|'all')*\n"
"  -a           - all printers\n"
"  -A           - use authentication\n"
"  -Pprinter    - printer (default PRINTER environment variable)\n"
"  -Uuser       - impersonate this user (root or privileged user only)\n"
"  -Ddebuglevel - debug level\n"
"  -V           - show version information\n"
"  user           removes user jobs\n"
"  all            removes all jobs\n"
"  jobid          removes job number jobid\n"
" Example:\n"
"    'lprm -Plp 30' removes job 30 on printer lp\n"
"    'lprm -a'      removes all your jobs on all printers\n"
"    'lprm -a all'  removes all jobs on all printers\n"
"  Note: lprm removes only jobs for which you have removal permission\n"),
			Name );
	} else {
		FPRINTF(STDERR,
_(" usage: %s [-A] [-Ddebuglevel] (jobid|user|'all')* [printer]\n"
"  -A           - use authentication\n"
"  -Ddebuglevel - debug level\n"
"  user           removes user jobs\n"
"  all            removes all jobs\n"
"  jobid          removes job number jobid\n"
" Example:\n"
"    'clean 30 lp' removes job 30 on printer lp\n"
"    'clean'       removes first job on default printer\n"
"    'clean all'      removes all your jobs on default printer\n"
"    'clean all all'  removes all your jobs on all printers\n"
"  Note: clean removes only jobs for which you have removal permission\n"),
			Name );
	}
	{
	char buffer[128];
	FPRINTF( STDERR, "Security Supported: %s\n", ShowSecuritySupported(buffer,sizeof(buffer)) );
	}
	Parse_debug("=",-1);
	FPRINTF( STDOUT, "%s\n", Version );
	exit(1);
}
