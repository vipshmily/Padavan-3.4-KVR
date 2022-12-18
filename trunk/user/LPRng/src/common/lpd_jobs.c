/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"
#include "accounting.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "gethostinfo.h"
#include "getopt.h"
#include "getprinter.h"
#include "getqueue.h"
#include "linksupport.h"
#include "lockfile.h"
#include "lpd_remove.h"
#include "merge.h"
#include "permission.h"
#include "printjob.h"
#include "proctitle.h"
#include "sendjob.h"
#include "sendmail.h"
#include "stty.h"
#include "openprinter.h"

#include "lpd_jobs.h"
#include "lpd_rcvjob.h"
#include "lpd_worker.h"

#if defined(USER_INCLUDE)
# include USER_INCLUDE
#else
# if defined(CHOOSER_ROUTINE)
#   error No 'USER_INCLUDE' file with function prototypes specified
    You need an include file with function prototypes
# endif
#endif


/**** ENDINCLUDE ****/
static int Fork_subserver( struct line_list *server_info, int use_subserver,
	struct line_list *parms );
static void Wait_for_subserver( int timeout, int pid_to_wait_for, struct line_list *servers
	/*, struct line_list *order */ );
static void Update_status( int fd, struct job *job, int status );
static int Check_print_perms( struct job *job );
static void Setup_user_reporting( struct job *job );
static void Filter_files_in_job( struct job *job, int outfd, char *user_filter );
static int Move_job(int fd, struct job *job, struct line_list *sp,
	char *errmsg, int errlen );

/***************************************************************************
 * Commentary:
 * Patrick Powell Thu May 11 09:26:48 PDT 1995
 * 
 * Job processing algorithm
 * 
 * 1. Check to see if there is already a spooler process active.
 *    The active file will contain the PID of the active spooler process.
 * 2. Build the job queue
 * 3. For each job in the job queue, service them.
 * 
 * MULTIPLE Servers for a Single Queue.
 * In the printcap, the "sv" flag sets the Server_names_DYN variable with
 * the list of servers to be used for this queue.  The "ss"  flag sets
 * the Server_queue_name_DYN flag with the queue that this is a server for.
 * 
 * Under normal conditions, the following process hierarchy is used:
 * 
 *  server process - printer 'spool queue'
 *     subserver process - first printer in 'Server_name'
 *     subserver process - second printer in 'Server_name'
 *     ...
 * 
 * The server process does the following:
 *   for each printer in the Server_name list
 *      sort them by the last order that you had in the control file
 *   for each printer in the Server_name list
 *      check the status of the queue, and gets the control file
 *      information and the numbers of jobs waiting.
 *   for each printer in the Server_name list
 *      if printable jobs and printing enabled
 *      start up a subserver
 * 
 *   while(1){
 * 	for all printable jobs do
 * 		check to see if there is a queue for them to be
 * 		printed on and the the queue is not busy;
 * 	if( job does not need a server ) then
 * 		do whatever is needed;
 * 		update job status();
 * 	else if( no jobs to print and no servers active ) then
 * 		break;
 * 	else if( no jobs to print or server active ) then
 * 		wait for server to exit();
 * 		if( pid is for server printing job)
 * 			update job status();
 * 			update server status();
 * 	else
 * 		dofork(0) a server process;
 * 		record pid of server doing work;
 * 	endif
 *   }
 *     
 * We then check to see if we are a slave (sv) to a master spool queue;
 * if we are and we are not a child process of the 'master' server,
 * we exit.
 * 
 * Note: if we spool something to a slave queue,  then we need to start
 * the master server to make the slave printer work.
 * 
 * Note: the slave queue processes
 * will not close the masters lock files;  this means a new master
 * cannot start serving the queue until all the slaves are dead.
 * Why this action do you ask?  The reason is that it is difficult
 * for a new master to inherit slaves from a dead master.
 * 
 * It turns out that many implementations of some network
 * based databased systems and other network database routines are broken,
 * and have memory leaks or never close file descriptors.  Up to the point
 * where the loop for checking the queue starts,  there is a known number
 * of file descriptors open,  and dynamically allocated memory.
 * After this,  it is difficult to predict just what is going to happen.
 * By forking a 'subserver' process, we firewall the actual memory and
 * file descriptor screwups into the subserver process.
 * 
 * When the subserver exits, it returns an error code that the server
 * process then interprets.  This error code is used to either remove the job
 * or retry it.
 * 
 * Note that there are conditions under which a job cannot be removed.
 * We simply abort at that point and let higher level authority (admins)
 * deal with this.
 * 
 ***************************************************************************/

/*
 * Signal handler to set flags and terminate system calls
 *  NOTE: use 'volatile' so that the &*()()&* optimizing compilers
 *  handle the value correctly. 
 */

 static volatile int Susr1, Chld;

 static void Sigusr1(void)
{
	++Susr1;
	(void) plp_signal_break(SIGUSR1,  (plp_sigfunc_t)Sigusr1);
	return;
}

 static void Sigchld(void)
{
	++Chld;
	signal( SIGCHLD, SIG_DFL );
	return;
}


/***************************************************************************
 * Update_spool_info()
 *  get updated spool control file information
 ***************************************************************************/


static void Update_spool_info( struct line_list *sp )
{
	struct line_list info;
	char *sc;

	Init_line_list(&info);

	Set_str_value(&info,SPOOLDIR, Find_str_value(sp,SPOOLDIR) );
	Set_str_value(&info,PRINTER, Find_str_value(sp,PRINTER) );
	Set_str_value(&info,QUEUE_CONTROL_FILE, Find_str_value(sp,QUEUE_CONTROL_FILE) );
	Set_str_value(&info,HF_NAME, Find_str_value(sp,HF_NAME) );
	Set_str_value(&info,IDENTIFIER, Find_str_value(sp,IDENTIFIER) );
	Set_str_value(&info,SERVER, Find_str_value(sp,SERVER) );
	Set_str_value(&info,DONE_TIME, Find_str_value(sp,DONE_TIME) );

	sc = Find_str_value(&info,QUEUE_CONTROL_FILE);

	DEBUG1("Update_spool_info: file '%s'", sc );

	Free_line_list(sp);
	Get_spool_control(sc,sp);
	Merge_line_list(sp,&info,Hash_value_sep,1,1);
	Free_line_list(&info);
}

static int cmp_server( const void *left, const void *right, const void *p )
{   
    struct line_list *l, *r;
	int tr, tl;
	l = ((struct line_list **)left)[0];
	r = ((struct line_list **)right)[0];
	tl = Find_flag_value(l,DONE_TIME);
	tr = Find_flag_value(r,DONE_TIME);
	if(DEBUGL5)Dump_line_list("cmp_server - l",l);
	if(DEBUGL5)Dump_line_list("cmp_server - r",r);
	DEBUG5("cmp_server: tl %d, tr %d, cmp %d, p %d",
		tl, tr, tl - tr, (int)(p!=0) );
	return( tl - tr );
}


static void Get_subserver_pc( char *printer, struct line_list *subserver_info, int done_time )
{
	int printable, held, move, err, done;
	char *path;
	char buffer[SMALLBUFFER];

	printable = held = move = err = done = 0;

	DEBUG1("Get_subserver_pc: '%s'", printer );
	buffer[0] = 0;
	if( Setup_printer( printer, buffer, sizeof(buffer), 1 ) ){
		Errorcode = JABORT;
		fatal(LOG_ERR, "Get_subserver_pc: '%s' - '%s'", printer, buffer);
	}

	Set_str_value(subserver_info,PRINTER,Printer_DYN);
	Set_str_value(subserver_info,SPOOLDIR,Spool_dir_DYN);
	path = Make_pathname( Spool_dir_DYN, Queue_control_file_DYN );
	Set_str_value(subserver_info,QUEUE_CONTROL_FILE,path);
	if( path ) free(path); path = 0;

	Update_spool_info( subserver_info );

	DEBUG1("Get_subserver_pc: scanning '%s'", Spool_dir_DYN );
	Scan_queue( subserver_info, 0, &printable, &held, &move, 1, &err, &done, 0, 0);
	Set_flag_value(subserver_info,PRINTABLE,printable);
	Set_flag_value(subserver_info,HELD,held);
	Set_flag_value(subserver_info,MOVE,move);
	Set_flag_value(subserver_info,DONE_TIME,done_time);
	if( !(Save_when_done_DYN || Save_on_error_DYN )
		&& (Done_jobs_DYN || Done_jobs_max_age_DYN)
		&& (err || done ) ){
		Set_flag_value(subserver_info,DONE_REMOVE,1);
	}

	DEBUG1("Get_subserver_pc: printable %d, held %d, move %d, done_remove %d, fowarding '%s'",
		printable, held, move,
		Find_flag_value(subserver_info,DONE_REMOVE),
		Find_str_value(subserver_info,FORWARDING) );
}

/***************************************************************************
 * Dump_subserver_info()
 *  dump the server information list
 ***************************************************************************/

static void Dump_subserver_info( const char *title, struct line_list *l)
{
	char buffer[LINEBUFFER];
	int i;
	LOGDEBUG("*** Dump_subserver_info: '%s' - %d subservers",
		title,	l->count );
	for( i = 0; i < l->count; ++i ){
		plp_snprintf(buffer,sizeof(buffer), "server %d",i);
		Dump_line_list_sub(buffer,(struct line_list *)l->list[i]);
	}
}

/***************************************************************************
 * Get_subserver_info()
 *  hack up the server information list into a list of servers
 ***************************************************************************/

static void Get_subserver_info( struct line_list *order,
	char *list, char *old_order)
{
	struct line_list server_order, server, *pl;
	int i;
	char *s;

	Unescape( old_order ); /* this is ugly - we make it forwards compatible */
	Init_line_list(&server_order);
	Init_line_list(&server);

	DEBUG1("Get_subserver_info: old_order '%s', list '%s'",old_order, list);
	Split(&server_order,old_order,File_sep,0,0,0,1,0,0);
	Split(&server_order,     list,File_sep,0,0,0,1,0,0);
	if(DEBUGL1)Dump_line_list("Get_subserver_info - starting",&server_order);

	/* get the info of printers */
	for( i = 0; i < server_order.count; ++i ){
		s = server_order.list[i];
		DEBUG1("Get_subserver_info: doing '%s'",s);
		if( Find_str_value(&server,s) ){
			DEBUG1("Get_subserver_info: already done '%s'",s);
			continue;
		}
		pl = malloc_or_die(sizeof(pl[0]),__FILE__,__LINE__);
		Init_line_list(pl);
		Get_subserver_pc( s, pl, i+1 );
		Check_max(order,1);
		DEBUG1("Get_subserver_info: adding to list '%s' at %d",s,order->count);
		order->list[order->count++] = (char *)pl;
		Set_str_value(&server,s,s);
		pl = 0;
	}
	Free_line_list(&server_order);
	Free_line_list(&server);
	if(DEBUGL1)Dump_subserver_info("Get_subserver_info - starting order",order);
}

/***************************************************************************
 * Make_temp_copy - make a temporary copy in the directory
 ***************************************************************************/

static char *Make_temp_copy( char *srcfile, char *destdir )
{
	char buffer[LARGEBUFFER];
	char *path = 0;
	struct stat statb;
	int srcfd, destfd, fail, n, len, count;

	fail = 0;
	srcfd = destfd = -1;

	DEBUG3("Make_temp_copy: '%s' to '%s'", srcfile, destdir);

	destfd = Make_temp_fd_in_dir(&path, destdir);
	unlink(path);
	if( link( srcfile, path ) == -1 ){
		DEBUG3("Make_temp_copy: link '%s' to '%s' failed, '%s'",
			srcfile, path, Errormsg(errno) );
		srcfd = Checkread(srcfile, &statb );
		if( srcfd < 0 ){
			logerr(LOG_INFO, "Make_temp_copy: open '%s' failed", srcfile );
			fail = 1;
			goto error;
		}
		while( (n = ok_read(srcfd,buffer,sizeof(buffer))) > 0 ){
			for( count = len = 0; len < n
				&& (count = write(destfd, buffer+len,n-len)) > 0;
				len += count );
			if( count < 0 ){
				logerr(LOG_INFO, "Make_temp_copy: copy to '%s' failed", path );
				fail = 1;
				goto error;
			}
		}
	}

 error:
	if( fail ){
		unlink(path); path = 0;
	}
	if( srcfd >= 0 ) close(srcfd); srcfd = -1;
	if( destfd >= 0 ) close(destfd); destfd = -1;
	return( path );
}

/***************************************************************************
 * Do_queue_jobs: process the job queue
 ***************************************************************************/

 static int Done_count;
 static time_t Done_time;

int Do_queue_jobs( char *name, int subserver )
{
	int master = 0;		/* this is the master */
	int opened_logfile = 0;	/* we have not opened the log file */
	int lock_fd;	/* fd for files */
	char buffer[SMALLBUFFER], *savename = 0, errmsg[SMALLBUFFER], *save_move_dest;
	char *path, *s, *id, *tempfile, *transfername, *openname,
		*new_dest, *move_dest, *pr, *hf_name, *forwarding;
	struct stat statb;
	int i, j, mod, fd, pid, printable, held, move, destinations,
		destination, use_subserver, job_to_do, working, printing_enabled,
		all_done, job_index, change, in_tempfd, out_tempfd, len,
		chooser_did_not_find_server, error, done, done_remove, check_for_done;
	struct line_list servers, tinfo, *sp, chooser_list, chooser_env;
	plp_block_mask oblock;
	struct job job;
	int jobs_printed = 0;
	int errlen = sizeof(errmsg);

	Init_line_list(&tinfo);

	lock_fd = -1;

	Init_job(&job);
	Init_line_list(&servers);
	Init_line_list(&chooser_list);
	Init_line_list(&chooser_env);
	id = transfername = 0;
	in_tempfd = out_tempfd = -1;

	Name = "(Server)";
	Set_DYN(&Printer_DYN,name);
	DEBUG1("Do_queue_jobs: called with name '%s', subserver %d",
		Printer_DYN, subserver );
	name = Printer_DYN;

	if(DEBUGL4){ int fdx; fdx = dup(0); LOGDEBUG("Do_queue_jobs: start next fd %d",fdx); close(fdx); };

 begin:
	Set_DYN(&Printer_DYN,name);
	DEBUG1("Do_queue_jobs: begin name '%s'", Printer_DYN );
	tempfile = 0;
	Free_listof_line_list( &servers );
	if( lock_fd != -1 ) close( lock_fd ); lock_fd = -1;

	Errorcode = JABORT;
	/* you need to have a spool queue */
	if( Setup_printer( Printer_DYN, buffer, sizeof(buffer), 0 ) ){
		cleanup(0);
	}
	if(DEBUGL4){ int fdx; fdx = dup(0);
	LOGDEBUG("Do_queue_jobs: after Setup_printer next fd %d",fdx); close(fdx); };

	setproctitle( "lpd %s '%s'", Name, Printer_DYN );

 again:
	/* block signals */
	plp_block_one_signal(SIGCHLD, &oblock);
	plp_block_one_signal(SIGUSR1, &oblock);
	(void) plp_signal(SIGCHLD,  SIG_DFL);
	(void) plp_signal(SIGUSR1,  (plp_sigfunc_t)Sigusr1);
	Susr1 = 0;

	path = Make_pathname( Spool_dir_DYN, Queue_lock_file_DYN );
	DEBUG1( "Do_queue_jobs: checking lock file '%s'", path );
	lock_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 );
	if( lock_fd < 0 ){
		logerr_die(LOG_ERR, _("Do_queue_jobs: cannot open lockfile '%s'"),
			path ); 
	}
	if(path) free(path); path = 0;

	/*
	This code is very tricky,  and may cause some headaches
	First, you want to make sure that you have an active process
	to do the unspooling.  If you CAN lock the lock file, then you
	are the active process.  If you CANNOT lock the lock file
	then some other process is the active process.

	If some other process is the active process then you want to
	make sure that you signal it.  When the process is exiting
	then it will first truncate lock file and then close it.
	We make the brutal assumption that from the time that the
	process truncates the log file until it exits will be less than
	the time taken for this process to read the file and send a signal.
	We help a bit by having the exiting process sleep 2 seconds -
	i.e. - we make sure that it will take a fairish time.

	This makes sure that processes that read the PID before it is
	truncated will send the signal to an existing processes.
	*/

	while( Do_lock( lock_fd, 0 ) < 0 ){
		pid = Read_pid( lock_fd );
		DEBUG1( "Do_queue_jobs: server process '%d' may be active", pid );
		if( pid == 0 || kill( pid, SIGUSR1 ) ){
			plp_usleep(1000);
			continue;
		}
		Errorcode = 0;
		cleanup(0);
	}
	pid = getpid();
	DEBUG1( "Do_queue_jobs: writing lockfile '%s' with pid '%d'",
		Queue_lock_file_DYN, pid );
	Write_pid( lock_fd, pid, (char *)0 );

	/* we now now new queue status so we force update */
	if( Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN);
	}
	if( Log_file_DYN && !opened_logfile ){
		fd = Trim_status_file( -1, Log_file_DYN, Max_log_file_size_DYN,
			Min_log_file_size_DYN );
		if( fd > 0 && fd != 2 ){
			dup2(fd,2);
			close(fd);
		}
		opened_logfile = 1;
	}

	s = Find_str_value(&Spool_control,DEBUG);
	if(!s) s = New_debug_DYN;
	Parse_debug( s, 0);

	if( Server_queue_name_DYN ){
		if( subserver == 0 ){
			/* you really need to start up the master queue */
			name = Server_queue_name_DYN;
			DEBUG1("Do_queue_jobs: starting up master queue '%s'", name );
			goto begin;
		}
		Name = "(Sub)";
	}
	/* set up the server name information */
	Check_max(&servers,1);
	sp = malloc_or_die(sizeof(sp[0]),__FILE__,__LINE__);
	memset(sp,0,sizeof(sp[0]));
	Set_str_value(sp,PRINTER,Printer_DYN);
	Set_str_value(sp,SPOOLDIR,Spool_dir_DYN);
	Set_str_value(sp,QUEUE_CONTROL_FILE,Queue_control_file_DYN);
	servers.list[servers.count++] = (char *)sp;
	Update_spool_info(sp);

	change = Find_flag_value(&Spool_control,CHANGE);
	if( change ){
		Set_flag_value(sp,CHANGE,0);
		Set_flag_value(&Spool_control,CHANGE,0);
		Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
	}

	master = 0;
	if( !ISNULL(Server_names_DYN) ){
		DEBUG1( "Do_queue_jobs: Server_names_DYN '%s', Server_order '%s'",
			Server_names_DYN, Srver_order(&Spool_control) );
		if( Server_queue_name_DYN ){
			Errorcode = JABORT;
			fatal(LOG_ERR, "Do_queue_jobs: serving '%s' and subserver for '%s'",
				Server_queue_name_DYN, Server_names_DYN );
		}

		/* save the Printer_DYN name */
		savename = safestrdup(Printer_DYN,__FILE__,__LINE__);
		/* we now get the subserver information */
		Get_subserver_info( &servers,
			Server_names_DYN, Srver_order(&Spool_control) );

		/* reset the main printer */
		if( Setup_printer( savename, buffer, sizeof(buffer), 0 ) ){
			cleanup(0);
		}
		if(savename) free(savename); savename = 0;

		master = 1;
		/* start the queues that need it */
		for( i = 1; i < servers.count; ++i ){
			sp = (void *)servers.list[i];
			pr = Find_str_value(sp,PRINTER);
			DEBUG1("Do_queue_jobs: subserver '%s' checking for independent action", pr );
			if( (s = Find_str_value(sp,SERVER)) ){
				DEBUG1("Do_queue_jobs: subserver '%s' active server '%s'", pr,s );
			}
			printable = !Pr_disabled(sp) && !Pr_aborted(sp)
				&& Find_flag_value(sp,PRINTABLE);
			move = Find_flag_value(sp,MOVE);
			forwarding = Find_str_value(sp,FORWARDING);
			change = Find_flag_value(sp,CHANGE);
			DEBUG1("Do_queue_jobs: subserver '%s', printable %d, move %d, forwarding '%s'",
				pr, printable, move, forwarding );
			/* now see if we need to clean up the old jobs */
			done_remove = Find_flag_value(sp,DONE_REMOVE);

			if( printable || move || change || forwarding || done_remove ){
				pid = Fork_subserver( &servers, i, 0 );
				jobs_printed = 1;
			}
			Set_flag_value(sp,CHANGE,0);
		}
	}


	if(DEBUGL3)Dump_subserver_info("Do_queue_jobs - after setup",&servers);

	if(DEBUGL4){ int fdx; fdx = dup(0);
		LOGDEBUG("Do_queue_jobs: after subservers next fd %d",fdx);close(fdx);};
	/* get new job values */
	if( Scan_queue( &Spool_control, &Sort_order,
			&printable, &held, &move, 1, &error, &done, 0, 0 ) ){
		Errorcode = JFAIL;
		fatal(LOG_ERR, "Do_queue_jobs: cannot read queue directory '%s'",
			Spool_dir_DYN );
	}

	DEBUG1( "Do_queue_jobs: printable %d, held %d, move %d, err %d, done %d",
		printable, held, move, error, done );

	if(DEBUGL1){ i = dup(0);
		LOGDEBUG("Do_queue_jobs: after Scan_queue next fd %d", i); close(i); }

	/* remove junk fields from job information */
	fd = -1;
	for( i = 0; i < Sort_order.count; ++i ){
		/* fix up the sort stuff */
		if( fd > 0 ) close(fd); fd = -1;
		Free_job(&job);
		Get_job_ticket_file(&fd, &job,Sort_order.list[i] );
		if( !job.info.count ) continue;
		if(DEBUGL3)Dump_job("Do_queue_jobs - info", &job);

		/* debug output */
		mod = 0;
		if( Find_flag_value(&job.info,SERVER ) ){
			Set_decimal_value(&job.info,SERVER,0);
			mod = 1;
		}
		if((destinations = Find_flag_value(&job.info,DESTINATIONS))){
			if( Find_str_value(&job.info,DESTINATION ) ){
				Set_str_value(&job.info,DESTINATION,0);
				mod = 1;
			}
			for( j = 0; j < destinations; ++j ){
				Get_destination(&job,j);
				if( Find_flag_value(&job.destination,SERVER) ){
					mod = 1;
					Set_decimal_value(&job.destination,SERVER,0);
					Update_destination(&job);
				}
			}
		}
		if( mod ) Set_job_ticket_file(&job, 0, fd );
	}
	if( fd > 0 ) close(fd); fd = -1;


	Free_job(&job);

	check_for_done = 1;
	
	fd = -1;
	while(1){
		DEBUG1( "Do_queue_jobs: MAIN LOOP" );
		if(DEBUGL4){ int fdx; fdx = dup(0);
		LOGDEBUG("Do_queue_jobs: MAIN LOOP next fd %d",fdx); close(fdx); };
		Unlink_tempfiles();
		if( fd > 0 ) close(fd); fd = -1;

		/* check for changes to spool control information */
		plp_unblock_all_signals( &oblock );
		plp_set_signal_mask( &oblock, 0 );

		if( (Done_jobs_DYN > 0 && Done_count > Done_jobs_DYN)
			 || (Done_jobs_max_age_DYN > 0
					&& Done_time
					&& (time(0) - Done_time) > Done_jobs_max_age_DYN) ){
			Susr1 = 1;
		}
		DEBUG1( "Do_queue_jobs: Susr1 before scan %d, check_for_done %d",
			Susr1, check_for_done );
		while( Susr1 ){
			Susr1 = 0;
			Done_time = 0;
			Done_count = 0;
			DEBUG1( "Do_queue_jobs: rescanning" );

			Get_spool_control( Queue_control_file_DYN, &Spool_control);
			if( Scan_queue( &Spool_control, &Sort_order,
					&printable, &held, &move, 1, &error, &done, 0, 0 ) ){
				logerr_die(LOG_ERR, "Do_queue_jobs: cannot read queue '%s'",
					Spool_dir_DYN );
			}
			DEBUG1( "Do_queue_jobs: printable %d, held %d, move %d, error %d, done %d",
				printable, held, move, error, done );

			for( i = 0; i < servers.count; ++i ){
				sp = (void *)servers.list[i];
				Update_spool_info( sp );
				change = Find_flag_value(sp,CHANGE);
				pid = Find_flag_value(sp,SERVER);
				if( i > 0 && change && pid == 0 ){
					pid = Fork_subserver( &servers, i, 0 );
					jobs_printed = 1;
				}
				Set_flag_value(sp,CHANGE,0);
			}
			if(DEBUGL1) Dump_subserver_info( "Do_queue_jobs - rescan",
				&servers );

			/* check for changes to spool control information */
			plp_unblock_all_signals( &oblock);
			plp_set_signal_mask( &oblock, 0);
			DEBUG1( "Do_queue_jobs: Susr1 at end of scan %d", Susr1 );
			/* now check to see if you remove jobs */
			check_for_done = 0;
			if( !(Save_when_done_DYN || Save_on_error_DYN )
				&& (Done_jobs_DYN || Done_jobs_max_age_DYN)
				&& (error || done) ){
				check_for_done = 1;
			}
		}

		if(DEBUGL4) Dump_line_list("Do_queue_jobs - sort order printable",
			&Sort_order );

		Remove_done_jobs();

		/* make sure you can print */
		printing_enabled
			= !(Pr_disabled(&Spool_control) || Pr_aborted(&Spool_control));
		forwarding = Find_str_value(&Spool_control,FORWARDING);
		DEBUG3("Do_queue_jobs: printing_enabled '%d', forwarding '%s'",
			printing_enabled, forwarding );

		openname = transfername = hf_name = id = move_dest = new_dest = 0;
		destination = use_subserver = job_to_do = -1;
		working =  destinations = chooser_did_not_find_server = 0;

		if(DEBUGL2) Dump_subserver_info("Do_queue_jobs- checking for server",
			&servers );
		for( j = 0; j < servers.count; ++j ){
			sp = (void *)servers.list[j];
			pid = Find_flag_value(sp,SERVER);
			pr = Find_str_value(sp,PRINTER);
			DEBUG2("Do_queue_jobs: printer '%s', server %d", pr, pid );
			if( pid ){
				++working;
			}
		}

		fd = -1;
		for( job_index = 0; job_to_do < 0 && job_index < Sort_order.count;
			++job_index ){
			if( fd > 0 ) close(fd); fd = -1;

			/*
			 * This is a very special case:
			 *  if we have a Chooser AND we have :sv=xx,xx,xx AND
			 *  none of the destinations are available to the first job
			 *   then the chooser_did_not_find_server flag will be set
			 *  If the 'chooser_scan_queue' flag is set, then we will keep
			 *   scanning the queue to see if any job can be sent
			 */

			DEBUG1("Do_queue_jobs: chooser_did_not_find_server %d, Chooser_scan_queue_DYN %d",
				chooser_did_not_find_server, Chooser_scan_queue_DYN ); 
			if( chooser_did_not_find_server && !Chooser_scan_queue_DYN ){
				 break;
			}
			Free_job(&job);
			id = move_dest = new_dest = 0;
			destination = use_subserver = job_to_do = -1;
			destinations = 0;

			if( !Sort_order.list[job_index] ) continue;
			DEBUG3("Do_queue_jobs: job_index [%d] '%s'", job_index,
				Sort_order.list[job_index] );
			Get_job_ticket_file( &fd, &job, Sort_order.list[job_index] );

			if(DEBUGL4)Dump_job("Do_queue_jobs: job ",&job);
			if( job.info.count == 0 ) continue;

			/* check to see if active */
			if( (pid = Find_flag_value(&job.info,SERVER)) ){
				DEBUG3("Do_queue_jobs: [%d] active %d", job_index, pid );
				continue;
			}

			/* get printable status */
			/* Setup_cf_info( &job, 1 ); */
			Job_printable(&job,&Spool_control,&printable,&held,&move,&error,&done);
			if( (!(printable && (printing_enabled || forwarding)) && !move) || held ){
				DEBUG3("Do_queue_jobs: [%d] not processable", job_index );
				/* free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0; */
				continue;
			}
			if( Check_print_perms(&job) == P_REJECT ){
				Set_str_value(&job.info,ERROR,"no permission to print");
				Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
				if( Set_job_ticket_file( &job, 0, fd ) ){
					/* you cannot update job ticket file!! */
					setstatus( &job, _("cannot update job ticket file for '%s'"),
						id);
					fatal(LOG_ERR,
						_("Do_queue_jobs: cannot update job ticket file for '%s'"), 
						id);
				}
				if( !(Save_on_error_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
					setstatus( &job, _("removing job '%s' - no permissions"), id);
					Remove_job( &job );
					free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0;
				}
				continue;
			}
			{
				double jobsize = Find_double_value(&job.info,SIZE);
				if( jobsize == 0 && Discard_zero_length_jobs_DYN ){
					Set_str_value(&job.info,ERROR,"not printing zero length job");
					Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
					if( Set_job_ticket_file( &job, 0, fd ) ){
						/* you cannot update job ticket file!! */
						setstatus( &job, _("cannot update job ticket file for '%s'"),
							id);
						fatal(LOG_ERR,
							_("Do_queue_jobs: cannot update job ticket file for '%s'"), 
							id);
					}
					if( !(Save_on_error_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
						setstatus( &job, _("removing job '%s' - no permissions"), id);
						Remove_job( &job );
						free( Sort_order.list[job_index] ); Sort_order.list[job_index] = 0;
					}
					continue;
				}
			}

			/* get destination information */
			destinations = Find_flag_value(&job.info,DESTINATIONS);
			if( !destinations ){
				move_dest = new_dest = Find_str_value(&job.info,MOVE);
				if( !new_dest ) move_dest = new_dest = Frwarding(&Spool_control);
			} else {
				all_done = 0;
				for( j = 0; !new_dest && j < destinations; ++j ){
					Get_destination(&job,j);
					if( Find_flag_value(&job.destination,SERVER) ){
						break;
					}
					if( Find_flag_value(&job.destination,DONE_TIME ) ){
						++all_done;
						continue;
					}
					if( Find_flag_value(&job.destination,ERROR_TIME)
						|| Find_flag_value(&job.destination,HOLD_TIME) ){
						continue;
					}
					if( (move_dest = new_dest = Find_str_value( &job.destination, MOVE)) ){
						destination = j;
						break;
					}
					if( Find_flag_value(&job.destination, PRINTABLE )
						&& printing_enabled ){
						new_dest = Find_str_value( &job.destination,DEST);
						destination = j;
						break;
					}
				}
				if( !new_dest ){
					printable = 0;
				}
				if( all_done == destinations ){
					DEBUG3("Do_queue_jobs: destinations %d, done %d",
						destinations, all_done );
					Update_status( fd, &job, JSUCC );
					continue;
				}
			}

			DEBUG3("Do_queue_jobs: new_dest '%s', printable %d, master %d, destinations %d, destination %d",
				new_dest, printable, master, destinations, destination );
			if( move_dest ){
				sp = (void *)servers.list[0];
				/* we will start a process up to do move */
				use_subserver = 0;
				job_to_do = job_index;
			} else if( printing_enabled && printable ){
				/*
				 * find the subserver with a class that will print this job
				 * if master = 1, then we start with 1
				 */
				int printers_available = 0;
				Free_line_list( &chooser_list );
				Free_line_list( &chooser_env );
				DEBUG1("Do_queue_jobs: chooser '%s', chooser_routine %lx",
					Chooser_DYN, Cast_ptr_to_long(Chooser_routine_DYN) );
				for( j = master; use_subserver < 0 && j < servers.count; ++j ){
					sp = (void *)servers.list[j];
					s = Find_str_value(sp,PRINTER);
					DEBUG1("Do_queue_jobs: checking '%s'", s );
					if( Pr_disabled(sp)
						|| Pr_aborted(sp)
						|| Sp_disabled(sp)
						|| Find_flag_value(sp,SERVER)){
						DEBUG1("Do_queue_jobs: cannot use [%d] '%s'",
							j, s );
						continue;
					}
					if( Get_hold_class(&job.info,sp) ){
						DEBUG1("Do_queue_jobs: cannot use [%d] '%s' class conflict",
							j, s );
						/* we found a server that was not available */
						continue;
					}
					if( Chooser_DYN == 0 && Chooser_routine_DYN == 0 ){
						use_subserver = j;
						job_to_do = job_index;
					} else {
						char *t;
						/* add to the list of possible servers */
						++printers_available;
						Set_flag_value(&chooser_list,s,j);
						if( !Chooser_routine_DYN ){
							/* get the environment values for the possible server */
							t = Join_line_list_with_sep(sp,"\n");
							Set_str_value(&chooser_env,s,t);
							if( t ) free(t); t = 0;
							t = Find_str_value( &chooser_env,"PRINTERS" );
							if( t ){
								t = safestrdup3(t,",",s,__FILE__,__LINE__);
								Set_str_value( &chooser_env,"PRINTERS",t );
								if( t ) free(t); t = 0;
							} else {
								Set_str_value( &chooser_env,"PRINTERS",s );
							}
						}
					}
				}
				/* we now have to find out if we really need to call the chooser
				 * if we are working and a single queue, then we do not need to call the chooser
				 * if :sv= p1,p2  but none are available then we do not need to call the chooser
				 * if :sv == ""   - then we do need to call the chooser
				 * if :sv == p1,p2 and at least one is available - then we do need to call the chooser
				 */
				DEBUG1("Do_queue_jobs: Chooser %s, working %d, master %d",
					Chooser_DYN, working, master);
				if( (Chooser_routine_DYN || Chooser_DYN) && working && master == 0 ){
					chooser_did_not_find_server = 1;
				} else if( servers.count > 1 && printers_available == 0 ){
					chooser_did_not_find_server = 1;
				} else if( Chooser_routine_DYN ){
#if defined(CHOOSER_ROUTINE)
					extern int CHOOSER_ROUTINE( struct line_list *servers,
						struct line_list *available, int *use_subserver );
					/* return status for job */
					DEBUG1("Do_queue_jobs: using CHOOSER_ROUTINE %s", STR(CHOOSER_ROUTINE) );
					j =  CHOOSER_ROUTINE( &servers, &chooser_list, &use_subserver );
					if( j ){
						setstatus(&job, "CHOOSER_ROUTINE exit status %s", Server_status(j));
						chooser_did_not_find_server = 1;
						if( j != JFAIL && j != JABORT ){
							Update_status( fd, &job, j );
						}
						if( j == JABORT ){
							Errorcode = JABORT;
							fatal(LOG_ERR, "Do_queue_jobs: Chooser_routine aborted" );
						}
					} else if( use_subserver >= 0 ){
						/* we use this subserver */
						job_to_do = job_index;
					} else {
						/* we did not find a server queue */
						chooser_did_not_find_server = 1;
					}
#else
					Errorcode = JABORT;
					fatal(LOG_ERR,"Do_queue_jobs: 'chooser_routine' select and no routine defined");
#endif
				} else if( Chooser_DYN ){
					if( in_tempfd > 0 ) close( in_tempfd ); in_tempfd = -1;
					if( out_tempfd > 0 ) close( out_tempfd ); out_tempfd = -1;
					in_tempfd = Make_temp_fd(0);
					out_tempfd = Make_temp_fd(0);
					s = Find_str_value( &chooser_env,"PRINTERS" );
					if( s && (Write_fd_str( in_tempfd, s ) < 0 
						|| Write_fd_str( in_tempfd, "\n" ) < 0) ){
						Errorcode = JABORT;
						logerr_die(LOG_ERR, "Do_queue_jobs: write(%d) failed", in_tempfd);
					}
					/* we invoke the chooser with the list
					 * of printers we have found in the order
					 */
					if( lseek(in_tempfd,0,SEEK_SET) == -1 ){
						Errorcode = JFAIL;
						logerr_die(LOG_INFO, "Do_queue_jobs: fseek(%d) failed",
							out_tempfd);
					}
					j = Filter_file( Send_query_rw_timeout_DYN, in_tempfd, out_tempfd, "CHOOSER",
						Chooser_DYN, Filter_options_DYN, &job, &chooser_env, 1 );
					if( j ){
						setstatus(&job, "CHOOSER exit status %s", Server_status(j));
						chooser_did_not_find_server = 1;
						if( j != JFAIL && j != JABORT ){
							Update_status( fd, &job, j );
						}
						if( j == JABORT ){
							Errorcode = JABORT;
							fatal(LOG_ERR, "Do_queue_jobs: Chooser aborted" );
						}
					} else {
						if( lseek(out_tempfd,0,SEEK_SET) == -1 ){
							Errorcode = JFAIL;
							logerr_die(LOG_INFO, "Do_queue_jobs: fseek(%d) failed",
								out_tempfd);
						}
						len = Read_fd_len_timeout( Send_query_rw_timeout_DYN, out_tempfd, buffer,sizeof(buffer)-1 );
						if( len >= 0 ){
							buffer[len] = 0;
						} else {
							Errorcode = JFAIL;
							logerr_die(LOG_INFO, "Do_queue_jobs: read(%d) failed",
								out_tempfd);
						}
						while( isspace(cval(buffer)) ){
							memmove(buffer,buffer+1,safestrlen(buffer+1)+1);
						}
						if( (s = strpbrk(buffer,Whitespace)) ){
							*s = 0;
						}
						if( buffer[0] ){
							/* we found a server queue */
							setstatus(&job, "CHOOSER selected '%s'", buffer);
							if( Find_str_value( &chooser_list,buffer ) ){
								use_subserver = Find_flag_value(&chooser_list,buffer);
								job_to_do = job_index;
							} else if( strchr( buffer,'@') ){
								/* we are routing to a remote queue not in list */
								use_subserver = 0;
								job_to_do = job_index;
								Set_str_value( &job.info,NEW_DEST,buffer);
								new_dest = Find_str_value( &job.info,NEW_DEST);
								sp = (void *)servers.list[0];
								/* we will start a process up to do move */
								use_subserver = 0;
								job_to_do = job_index;
							} else {
								logmsg(LOG_ERR, "Do_queue_jobs: CHOOSER selection '%s' not a subserver",
									buffer );
							}
						} else {
							/* we did not find a server queue */
							chooser_did_not_find_server = 1;
						}
					}
				}
				if( in_tempfd >= 0 ) close( in_tempfd ); in_tempfd = -1;
				if( out_tempfd >= 0 ) close( out_tempfd ); out_tempfd = -1;
				Free_line_list( &chooser_env );
				Free_line_list( &chooser_list );
			}
		}

		/* first, we see if there is no work and no server active */
		DEBUG1("Do_queue_jobs: job_to_do %d, use_subserver %d, working %d, move_dest %s",
			job_to_do, use_subserver, working, move_dest );

		if( job_to_do < 0 && !working && chooser_did_not_find_server == 0 ){
			DEBUG1("Do_queue_jobs: nothing to do");
			if( fd > 0 ) close(fd); fd = -1;
			break;
		}

		/* now we see if we have to wait */ 
		if( use_subserver < 0 ){
			if( fd > 0 ) close(fd); fd = -1;
			if( chooser_did_not_find_server ){
				setstatus(0, "chooser did not find subserver, waiting %d sec",
					Chooser_interval_DYN );
				Wait_for_subserver( Chooser_interval_DYN, -1, &servers );
			} else if( working ){
				if( servers.count > 1 ){
					setstatus(0, "waiting for server queue process to exit" );
				} else {
					setstatus(0, "waiting for subserver to exit" );
				}
				Wait_for_subserver( 0, -1, &servers );
			}
			continue;
		}

		/*
		 * get the job information
		 */
		hf_name = Find_str_value(&job.info,HF_NAME);
		id = Find_str_value(&job.info,IDENTIFIER);
		if( !id ){
			Errorcode = JABORT;
			fatal(LOG_ERR,
				_("Do_queue_jobs: LOGIC ERROR - no identifer '%s'"), hf_name);
		}

		/*
		 * check for circular forwarding of jobs
		 */
		if( Max_move_count_DYN > 0 && Find_flag_value(&job.info,MOVE_COUNT) > Max_move_count_DYN ){
			Errorcode = JABORT;
			fatal(LOG_ERR,
				_("Do_queue_jobs: FORWARDING LOOP - '%s'"), hf_name);
		}

		/*
		 * set the job ticket file information
		 */

		if( destination >= 0 ){
			plp_snprintf(buffer,sizeof(buffer), "DEST%d",destination );
			Set_str_value(&job.info,DESTINATION,buffer);
		}

		DEBUG1("Do_queue_jobs: setting %s SERVER %d", id, (int)getpid() );
		Set_decimal_value(&job.info,SERVER,getpid());
		Set_flag_value(&job.info,START_TIME,time((void *)0));

		sp = (void *)servers.list[use_subserver];

		pr = Find_str_value(sp,PRINTER);
		DEBUG1("Do_queue_jobs: starting job '%s' on '%s', use_subserver %d, move_dest '%s'",
			id, pr, use_subserver, move_dest );

		if( Set_job_ticket_file( &job, 0, fd ) ){
			/* you cannot update job ticket file!! */
			setstatus( &job, _("cannot update job ticket file '%s'"), hf_name);
			fatal(LOG_ERR,
				_("Do_queue_jobs: cannot update job ticket file '%s'"), hf_name);
		}

		if( Status_file_DYN ){
			int fd = -1;
			DEBUG1("Do_queue_jobs: trimming status file '%s'/'%s'", Spool_dir_DYN, Status_file_DYN );
			fd = Trim_status_file( -1, Status_file_DYN, Max_status_size_DYN,
				Min_status_size_DYN );
			if( fd > 0 ) close(fd); fd = -1;
		}

		/*
		 * at this point, if use_subserver != 0, then we can start up
		 * a subserver process.  We can move the job to the spool queue of the
		 * subserver process.  This job should have all of the characteristics
		 * of a new job for this queue
		 */
		if( use_subserver > 0 ){
			if( !Move_job( fd, &job, sp, buffer, sizeof(buffer)) ){
				/* now we deal with the job in the original queue */
				Set_str_value(sp,IDENTIFIER,id);
				setstatus(&job, "starting subserver '%s'", pr );
				pid = Fork_subserver( &servers, use_subserver, 0 );
			}
			jobs_printed = 1;
			if( fd > 0 ) close(fd); fd = -1;
			continue;
		} else if( move_dest ){
			/*
			 * we are moving a job to another queue.  This can be done
			 * by either linking or copying files.  The destination can be either
			 * a local queue or a remote printer
			 */
			static struct line_list new_sp; /* we are going to set a pointer to this */
			Init_line_list(&new_sp);
			savename = safestrdup(Printer_DYN,__FILE__,__LINE__);
			save_move_dest = safestrdup(move_dest,__FILE__,__LINE__);
			move_dest = save_move_dest;
			/*
			 * is it a remote printer?
			 */
			if( safestrchr(move_dest ,'@') ){
				hf_name = Find_str_value(&job.info,HF_NAME);
				id = Find_str_value(&job.info,IDENTIFIER);
				DEBUG1( "Do_queue_jobs: move_dest '%s', hf_name '%s', id '%s'",
					move_dest, hf_name, id );
				if( use_subserver ){
					Errorcode = JABORT;
					fatal(LOG_ERR,
						_("Do_queue_jobs: LOGIC ERROR! new_dest and use_subserver == %d"),
							use_subserver );
				}

				sp = (void *)servers.list[0];
				Merge_line_list(&new_sp,sp,Hash_value_sep,1,1);
				Set_str_value(&new_sp,HF_NAME,hf_name);
				Set_str_value(&new_sp,IDENTIFIER,id);

				servers.list[0] = (void *)&new_sp;
				Free_line_list(&tinfo);
				Set_str_value(&tinfo,HF_NAME,hf_name);
				Set_str_value(&tinfo,NEW_DEST,new_dest);
				Set_str_value(&tinfo,MOVE_DEST,move_dest);
				if( fd > 0 ) close(fd); fd = -1;
				if( (pid = Fork_subserver( &servers, 0, &tinfo )) < 0 ){
					setstatus( &job, _("sleeping, waiting for processes to exit"));
					plp_sleep(1);
				} else {
					Wait_for_subserver( 0, pid, &servers );
				}
				if(DEBUGL4)Dump_line_list("Do_queue_jobs - sp after wait", sp );
				Free_line_list(&new_sp);
				servers.list[0] = (void *)sp;
			} else if( Setup_printer( move_dest, errmsg, errlen, 1 ) ){
				/* we failed to find the destination directory */
				plp_snprintf(buffer,sizeof(buffer), "dest '%s' setup failed - %s'",
						move_dest, errmsg );
				Set_str_value(&job.info,ERROR,buffer);
				Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
				Set_job_ticket_file(&job, 0, fd );
			} else {
				/* we have found the destination directory, reset to original */
				Set_str_value(&new_sp,PRINTER,Printer_DYN);
				Set_str_value(&new_sp,SPOOLDIR,Spool_dir_DYN);
				if( Setup_printer( savename, errmsg, errlen, 1 ) ){
					/* could not get back to the original */
					Errorcode = JABORT;
					fatal(LOG_ERR, "Do_queue_jobs: move_dest subserver '%s' setup failed '%s'",
						savename, errmsg );
				}
				if( !safestrcmp( Printer_DYN, move_dest ) ){
					/* we are moving to the same spool queue */
					plp_snprintf(buffer,sizeof(buffer), "loop moving '%s' to '%s'",
							move_dest, Printer_DYN );
					Set_str_value(&job.info,ERROR,buffer);
					Set_nz_flag_value(&job.info,ERROR_TIME,time(0));
					Set_job_ticket_file(&job, 0, fd );
				} else if( !Move_job( fd, &job, &new_sp, buffer, sizeof(buffer)) ){
					Set_flag_value(&job.info,DONE_TIME,time((void *)0));
					setstatus( &job, "%s@%s: job '%s' moved",
						Printer_DYN, FQDNHost_FQDN, id );
					/* send a request to start the queue server */
					plp_snprintf( buffer, sizeof(buffer), "%s\n", move_dest );
					DEBUG1("Do_queue_jobs: sending '%s' to LPD", move_dest );
					if( Write_fd_str( Lpd_request, buffer ) < 0 ){
						Errorcode = JABORT;
						logerr_die(LOG_ERR, _("Do_queue_jobs: write to fd '%d' failed"),
							Lpd_request );
					}
				}
			}
			if( fd > 0 ) close(fd); fd = -1;
			if(savename) free(savename); savename = 0;
			if(save_move_dest) free(save_move_dest); save_move_dest = 0;
			move_dest = 0;
			Free_line_list(&new_sp);
			jobs_printed = 1;
			continue;
		} else {
			/* if( !Find_flag_value(sp,SERVER) ) */
			Free_line_list(&tinfo);
			hf_name = Find_str_value(&job.info,HF_NAME);
			id = Find_str_value(&job.info,IDENTIFIER);
			Set_str_value(&tinfo,HF_NAME,hf_name);
			Set_str_value(&tinfo,NEW_DEST,new_dest);
			Set_str_value(&tinfo,MOVE_DEST,move_dest);
			Set_str_value(sp,HF_NAME,hf_name);
			Set_str_value(sp,IDENTIFIER,id);
			if( (pid = Fork_subserver( &servers, 0, &tinfo )) < 0 ){
				setstatus( &job, _("sleeping, waiting for processes to exit"));
				plp_sleep(1);
				Set_str_value(sp,HF_NAME,0);
				Set_str_value(sp,IDENTIFIER,0);
			}
			jobs_printed = 1;
		}
		if( fd > 0 ) close(fd); fd = -1;
	}

	/* now we reset the server order */

	Errorcode = JSUCC;
	Free_job(&job);
	Free_line_list(&tinfo);
	if( Server_names_DYN ){
		if( jobs_printed ) setstatus( 0, "no more jobs to process in load balance queue" );
		jobs_printed = 0;
		for( i = 1; i < servers.count; ++i ){
			sp = (void *)servers.list[i];
			s = Find_str_value(sp,PRINTER);
			Add_line_list(&tinfo,s,0,0,0);
		}
		s = Join_line_list_with_sep(&tinfo,",");
		Set_str_value(&Spool_control,SERVER_ORDER,s);
		Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
		if(s) free(s); s = 0;
		Free_line_list(&tinfo);
	}
	Free_listof_line_list(&servers);

	/* truncate and close the lock file then wait a short time for signal */
	ftruncate( lock_fd, 0 );
	close( lock_fd );
	lock_fd = -1;
	/* force status update */
	if( Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN);
	}
	plp_unblock_all_signals( &oblock);
	plp_usleep(500);
	DEBUG1( "Do_queue_jobs: Susr1 at end %d", Susr1 );
	if( Susr1 ){
		DEBUG1("Do_queue_jobs: SIGUSR1 just before exit" );
		Susr1 = 0;
		goto again;
	}
	cleanup(0);
}

/***************************************************************************
 * Remote_job()
 * Send a job to a remote server.  This code is actually trickier
 *  than it looks, as the Send_job code takes most of the heat.
 *
 ***************************************************************************/

static int Remote_job( struct job *job, int lpd_bounce, char *move_dest, char *id )
{
	int status, tempfd, n, fd;
	double job_size;
	char buffer[SMALLBUFFER], *s, *tempfile, *oldid, *newid, *old_lp_value, *hf_name;
	struct line_list *lp, *firstfile;
	struct job jcopy;
	struct stat statb;

	DEBUG1("Remote_job: %s", id );
	/* setmessage(job,STATE,"SENDING"); */
	status = 0;
	Init_job(&jcopy);

	Set_str_value(&job->info,PRSTATUS,0);
	Set_str_value(&job->info,ERROR,0);
	Set_flag_value(&job->info,ERROR_TIME,0);

	Setup_user_reporting(job);

	if( Accounting_remote_DYN && Accounting_file_DYN && Accounting_start_DYN ){
		status = Do_accounting( 0,
				Accounting_start_DYN, job, Connect_interval_DYN );
		DEBUG1("Remote_job: accounting status %s", Server_status(status) );
		if( status ){
			/*
			 * special case when job has been removed while attempting to print
			 */
			fd = -1;
			hf_name = Find_str_value(&job->info,HF_NAME);
			safestrncpy(buffer,hf_name);
			Get_job_ticket_file( &fd, job, buffer );
			if( job->info.count ){
				switch(status){
				case JHOLD: Set_flag_value(&job->info,HOLD_TIME,time((void *)0)); break;
				case JREMOVE: Set_flag_value(&job->info,REMOVE_TIME,time((void *)0)); break;
				default:
						plp_snprintf(buffer,sizeof(buffer),
							"accounting check failed '%s'", Server_status(status));
						setstatus(job, "%s", buffer );
						Set_str_value(&job->info,ERROR,buffer);
						Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
						break;
				}
				Set_job_ticket_file(job, 0, fd );
			}
			close(fd);
			goto exit;
		}
	}

	Errorcode = status = 0;

	Copy_job(&jcopy,job);
	if(DEBUGL2)Dump_job("Remote_job - jcopy", &jcopy );
	if( lpd_bounce ){
		if(DEBUGL2) Dump_job( "Remote_job - before filtering", &jcopy );
		tempfd = Make_temp_fd(&tempfile);

		old_lp_value = safestrdup(Find_str_value( &PC_entry_line_list, "lp" ),
			__FILE__,__LINE__ );
		Set_str_value( &PC_entry_line_list, LP, tempfile );
		Print_job( tempfd, -1, &jcopy, 0, 0, 0 );
		Set_str_value( &PC_entry_line_list, LP, old_lp_value );
		if( old_lp_value ) free(old_lp_value); old_lp_value = 0;

		if( fstat( tempfd, &statb ) ){
			logerr(LOG_INFO, "Remote_job: fstatb failed" );
			status = JFAIL;
		}
		if( (close(tempfd) == -1 ) ){
			status = JFAIL;
			logerr(LOG_INFO, "Remote_job: close(%d) failed",
				tempfd);
		}
		if( statb.st_size == 0 ){
			logmsg( LOG_ERR, "Remote_job: zero length job after filtering");
			status = JABORT;
		}
		if( status ) goto exit;
		job_size = statb.st_size;
		Free_listof_line_list(&jcopy.datafiles);
		lp = malloc_or_die(sizeof(lp[0]),__FILE__,__LINE__);
		memset(lp,0,sizeof(lp[0]));
		Check_max(&jcopy.datafiles,1);
		jcopy.datafiles.list[jcopy.datafiles.count++] = (void *)lp;
		Set_str_value(lp,OPENNAME,tempfile);
		firstfile = (void *)(job->datafiles.list[0]);
		s = Find_str_value( firstfile, DFTRANSFERNAME );
		Set_str_value(lp,DFTRANSFERNAME,s);
		Set_str_value(lp,"N","(lpd_filter)");
		Set_flag_value(lp,COPIES,1);
		Set_double_value(lp,SIZE,job_size);
		Fix_bq_format( 'f', lp );
	} else if( !move_dest ) {
		int err = Errorcode;
		Errorcode = 0;
		if( Generate_banner_DYN ){
			Add_banner_to_job( &jcopy );
		}
		Filter_files_in_job( &jcopy, -1, 0 );
		status = Errorcode;
		Errorcode = err;
		if( status ){
			goto done;
		}
	}

	/* fix up the control file */
	Fix_control( &jcopy, Control_filter_DYN, Xlate_format_DYN, 1 );
	oldid = Find_str_value(&job->info,IDENTIFIER );
	newid = Find_str_value(&jcopy.destination,IDENTIFIER );
	if( newid == 0 ){
		newid = Find_str_value(&jcopy.info,IDENTIFIER );
	}
	n = Find_flag_value( &jcopy.info, DATAFILE_COUNT );
	if( Max_datafiles_DYN > 0 && n > Max_datafiles_DYN ){
		Errorcode = JABORT;
		fatal(LOG_ERR,
				_("Remote_job: %d datafiles and only allowed %d"),
					n, Max_datafiles_DYN );
	}
	setmessage(job,STATE,"SENDING OLDID=%s NEWID=%s DEST=%s@%s",
		oldid, newid, RemotePrinter_DYN, RemoteHost_DYN );
	if(DEBUGL3)Dump_job("Remote_job - after Fix_control", &jcopy );
	status = Send_job( &jcopy, job, Connect_timeout_DYN, Connect_interval_DYN,
		Max_connect_interval_DYN, Send_job_rw_timeout_DYN, 0 );
	DEBUG1("Remote_job: %s, status '%s'", id, Link_err_str(status) );
	buffer[0] = 0;

	if(DEBUGL2)Dump_job("Remote_job - final jcopy value", &jcopy );

 done:
	fd = -1;
	hf_name = Find_str_value(&job->info,HF_NAME);
	safestrncpy(buffer,hf_name);
	Get_job_ticket_file( &fd, job, buffer );
	if( job->info.count == 0 ){
		/*
		 * you have removed the job!
		 */
		goto exit;
	}
	s = 0;
	if( status ){
		s = Find_str_value(&jcopy.info,ERROR);
		if( !s ){
			Set_str_value(&job->info,ERROR,"Mystery error from Send_job");
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		}
	}
	s = 0;

	Free_job(&jcopy);

	switch( status ){
	case JSUCC:
	case JABORT:
	case JFAIL:
	case JREMOVE:
		break;
	case LINK_ACK_FAIL:
		plp_snprintf(buffer,sizeof(buffer),
			_("link failure while sending job '%s'"), id );
		s = buffer;
		status = JFAIL;
		break;
	case LINK_PERM_FAIL:
		plp_snprintf(buffer,sizeof(buffer),
			 _("no permission to spool job '%s'"), id );
		s = buffer;
		status = JREMOVE;
		break;
	default:
		plp_snprintf(buffer,sizeof(buffer),
			_("failed to send job '%s'"), id );
		s = buffer;
		status = JFAIL;
		break;
	}
	if( s ){
		if( !Find_str_value(&job->info,ERROR) ){
			Set_str_value(&job->info,ERROR,s);
		}
		if( !Find_flag_value(&job->info,ERROR_TIME) ){
			Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		}
	}

	Set_str_value(&job->info,PRSTATUS,Server_status(status));

	Set_job_ticket_file(job, 0, fd );
	close(fd); fd = -1;

	if( Accounting_remote_DYN && Accounting_file_DYN  ){
		if( Accounting_end_DYN ){
			Do_accounting( 1, Accounting_end_DYN, job,
				Connect_interval_DYN );
		}
	}
 exit:
	return( status );
}

/***************************************************************************
 * Local_job()
 * Send a job to a local printer.
 ***************************************************************************/

static int Local_job( struct job *job, char *id )
{
	int status, fd, status_fd, pid, poll_for_status;
	char *old_lp_value;
	char buffer[SMALLBUFFER];

	status_fd = fd = -1;

	DEBUG1("Local_job: starting %s", id );
	setmessage(job,STATE,"PRINTING");
	Errorcode = status = 0;
	Set_str_value(&job->info,PRSTATUS,0);
	Set_str_value(&job->info,ERROR,0);
	Set_flag_value(&job->info,ERROR_TIME,0);

	Setup_user_reporting(job);

	setstatus(job, "subserver pid %ld starting", (long)getpid());

	if( Accounting_file_DYN && Local_accounting_DYN ){
		setstatus(job, "accounting at start");
		if( Accounting_start_DYN ){
			status = Do_accounting( 0,
				Accounting_start_DYN, job, Connect_interval_DYN );
		}

		DEBUG1("Local_job: accounting status %s", Server_status(status) );
		if( status ){
			plp_snprintf(buffer,sizeof(buffer),
				"accounting check failed '%s'", Server_status(status));
			setstatus(job, "%s", buffer );
			switch(status){
			case JFAIL: break;
			case JHOLD: /* Set_flag_value(&job->info,HOLD_TIME,time((void *)0)); */ break;
			case JREMOVE: /* Set_flag_value(&job->info,REMOVE_TIME,time((void *)0)); */ break;
			default:
				Set_str_value(&job->info,ERROR,buffer);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				Set_job_ticket_file(job, 0, 0 );
				break;
			}
			goto exit;
		}
	}
 	Errorcode = status = 0;

	setstatus(job, "opening device '%s'", Lp_device_DYN);
	pid = 0;
	fd = Printer_open(Lp_device_DYN, &status_fd, job,
		Send_try_DYN, Connect_interval_DYN, Max_connect_interval_DYN,
		Connect_grace_DYN, Connect_timeout_DYN, &pid, &poll_for_status );

	/* note: we NEVER return fd == 0 or horrible things have happened */
	DEBUG1("Local_job: fd %d", fd );
	if( fd <= 0 ){
		status = JFAIL;
		goto exit;
	}
	setstatus(job, "printing job '%s'", id );
	/* Print_job( output_device, status_device, job, timeout, poll_for_status, filter ) */
	old_lp_value = safestrdup(Find_str_value( &PC_entry_line_list, LP ),
		__FILE__,__LINE__ );
	Set_str_value( &PC_entry_line_list, LP, Lp_device_DYN );
	status = Print_job( fd, status_fd, job, Send_job_rw_timeout_DYN, poll_for_status, 0 );
	Set_str_value( &PC_entry_line_list, LP, old_lp_value );
	if( old_lp_value ) free(old_lp_value); old_lp_value = 0;
	/* we close close device */
	DEBUG1("Local_job: shutting down fd %d", fd );

	fd = Shutdown_or_close( fd );
	DEBUG1("Local_job: after shutdown fd %d, status_fd %d", fd, status_fd );
	if( status_fd > 0 ){
		/* we shut down this connection as well */
		status_fd = Shutdown_or_close( status_fd );
		/* we wait for eof on status_fd */
		buffer[0] = 0;
		if( status_fd > 0 ){
			Get_status_from_OF(job,"LP",pid,
				status_fd, buffer, sizeof(buffer)-1, Send_job_rw_timeout_DYN,
				0, 0, Status_file_DYN );
		}
	}
	if( fd > 0 ) close( fd ); fd = -1;
	if( status_fd > 0 ) close( status_fd ); status_fd = -1;
	if( pid > 0 ){
		setstatus(job, "waiting for printer filter to exit");
		status = Wait_for_pid( pid, "LP", 0, Send_job_rw_timeout_DYN );
	}
	DEBUG1("Local_job: status %s", Server_status(status) );

	Set_str_value(&job->info,PRSTATUS,Server_status(status));
	if( Accounting_file_DYN && Local_accounting_DYN ){
		setstatus(job, "accounting at end");
		if( Accounting_end_DYN ){
			Do_accounting( 1, Accounting_end_DYN, job,
				Connect_interval_DYN );
		}
	}
	setstatus(job, "finished '%s', status '%s'", id, Server_status(status));

 exit:
	if( fd != -1 ) close(fd); fd = -1;
	if( status_fd != -1 ) close(status_fd); status_fd = -1;
	return( status );
}

static int Fork_subserver( struct line_list *server_info, int use_subserver,
	struct line_list *parms )
{
	char *pr;
	struct line_list *sp;
	int pid;
	struct line_list pl;

	Init_line_list(&pl);
	if( parms == 0 ) parms = &pl;
	sp = (void *)server_info->list[use_subserver];
	Set_str_value(sp,PRSTATUS,0);
	Set_decimal_value(sp,SERVER,0);

	pr = Find_str_value(sp,PRINTER);
	Set_str_value(parms,PRINTER,pr);
	Set_flag_value(parms,SUBSERVER,use_subserver);
	DEBUG1( "Fork_subserver: starting '%s'", pr );
	if(DEBUGL4)Dump_line_list("Fork_subserver - sp", sp );
	if( use_subserver > 0 ){
		pid = Start_worker( "queue", Service_queue, parms, 0 );
	} else {
		pid = Start_worker( "printer", Service_worker, parms, 0 );
	}

	if( pid > 0 ){
		Set_decimal_value(sp,SERVER,pid);
	} else {
		logerr(LOG_ERR, _("Fork_subserver: fork failed") );
	}
	Free_line_list(parms);
	return( pid );
}

/***************************************************************************
 * struct server_info *Wait_for_subserver( int timeout int pid,
 * struct line_list *servers,
 *  wait for a server process to exit
 *  if none present return 0
 *  look up the process in the process table
 *  update the process table status
 *  return the process table entry
 ***************************************************************************/

static void Wait_for_subserver( int timeout, int pid_to_wait_for, struct line_list *servers
	/*, struct line_list *order */ )
{
	pid_t pid;
	plp_status_t procstatus;
	int found, sigval, status, i, done, flags, fd;
	struct line_list *sp = 0;
	struct job job;
	char buffer[SMALLBUFFER], *pr, *hf_name, *id;

	flags = WNOHANG;
	if( pid_to_wait_for != -1 ){
		flags = 0;
	}
	/*
	 * wait for the process to finish or a signal to be delivered
	 */

	Init_job(&job);
	sigval = errno = 0;

	done = 0;
	fd = -1;
 again:
	DEBUG1("Wait_for_subserver: pid_to_wait_for %d, flags %d", pid_to_wait_for, flags );
	if( fd > 0 ) close(fd); fd = -1;
	while( (pid = plp_waitpid( pid_to_wait_for, &procstatus, flags )) > 0 ){
		++done;
		if( fd > 0 ) close(fd); fd = -1;
		DEBUG1("Wait_for_subserver: pid %ld, status '%s'", (long)pid,
			Decode_status(&procstatus));
		if( WIFSIGNALED( procstatus ) ){
			sigval = WTERMSIG( procstatus );
			DEBUG1("Wait_for_subserver: pid %ld terminated by signal '%s'",
				(long)pid, Sigstr( sigval ) );
			switch( sigval ){
			/* generated by the program */
			case 0:
			case SIGINT:
			case SIGKILL:
			case SIGQUIT:
			case SIGTERM:
			case SIGUSR1:
				status = JFAIL;
				break;
			default:
				status = JSIGNAL;
				break;
			}
		} else {
			status = WEXITSTATUS( procstatus );
			if( status > 0 && status < 32 ) status += JFAIL-1;
		}
		DEBUG1( "Wait_for_subserver: pid %ld final status %s",
			(long)pid, Server_status(status) );

		if( status != JSIGNAL ){
			plp_snprintf(buffer,sizeof(buffer),
				 _("subserver pid %ld exit status '%s'"),
				(long)pid, Server_status(status));
		} else {
			plp_snprintf(buffer,sizeof(buffer),
				_("subserver pid %ld died with signal '%s'"),
				(long)pid, Sigstr(sigval));
			status = JABORT;
		}
		if(DEBUGL4) Dump_subserver_info("Wait_for_subserver", servers );

		for( found = i = 0; !found && i < servers->count; ++i ){
			if( fd > 0 ) close(fd); fd = -1;
			sp = (void *)servers->list[i];
			if( pid == Find_flag_value(sp,SERVER) ){
				DEBUG3("Wait_for_subserver: found %ld", (long)pid );
				found = 1;
				++done;

				Free_job(&job);
				Set_decimal_value(sp,SERVER,0);
				Set_flag_value(sp,DONE_TIME,time((void *)0));

				/* we get the job ticket file information */
				hf_name = Find_str_value(sp,HF_NAME);
				Get_job_ticket_file( &fd, &job, hf_name );
				if( !job.info.count ) continue;
				/* Setup_cf_info( &job, 0 ); */

				pr = Find_str_value(sp,PRINTER);
				id = Find_str_value(sp,IDENTIFIER);
				DEBUG1( "Wait_for_subserver: server pid %ld for '%s' for '%s' '%s' finished",
					(long)pid, pr, hf_name, id );

				/* see if you can get the job ticket file and update the status */
				Update_status( fd, &job, status );
				Set_str_value(sp,HF_NAME,0);
				Set_str_value(sp,IDENTIFIER,0);
				Update_spool_info(sp);
				if( i == 0 ){
					/* this is the information for the master spool queue */
					Get_spool_control(Queue_control_file_DYN, &Spool_control );
				}
			}
		}
		if( fd > 0 ) close(fd); fd = -1;
		Free_job(&job);
		/* sort server order */
		if( Mergesort( servers->list+1, servers->count-1,
			sizeof( servers->list[0] ), cmp_server, 0 ) ){
			fatal(LOG_ERR,
				_("Wait_for_subserver: Mergesort failed") );
		}
		if(DEBUGL4) Dump_subserver_info(
			"Wait_for_subserver: after sorting", servers );
		if( pid_to_wait_for != -1 ) break;
	}
	if( fd > 0 ) close(fd); fd = -1;
	if( !done ){
		if( pid_to_wait_for != -1){
			Errorcode = JABORT;
			fatal(LOG_ERR,
				_("Wait_for_subserver: LOGIC ERROR! waiting for pid %d failed"), pid_to_wait_for );
		}
		/* we need to unblock signals and wait for event */
		Chld = 0;
		Set_timeout_break( timeout );
		(void) plp_signal(SIGCHLD,  (plp_sigfunc_t)Sigchld);
		plp_sigpause();
		Clear_timeout();
		signal( SIGCHLD, SIG_DFL );
		if( Chld ) goto again;
	}

	Free_job(&job);
}

/***************************************************************************
 * int Decode_transfer_failure( int attempt, struct job *job, int status )
 * When you get a job failure more than a certain number of times,
 *  you check the 'Send_failure_action_DYN' variable
 *  This can be abort, retry, or remove
 * If retry, you keep retrying; if abort you shut the queue down;
 *  if remove, you remove the job and try again.
 ***************************************************************************/

 static struct keywords keys[] = {
	{"succ", N_("succ"), INTEGER_K, (void *)0, JSUCC,0,0},
	{"jsucc", N_("jsucc"), INTEGER_K, (void *)0, JSUCC,0,0},
	{"success", N_("success"), INTEGER_K, (void *)0, JSUCC,0,0},
	{"jsuccess", N_("jsuccess"), INTEGER_K, (void *)0, JSUCC,0,0},
	{"abort", N_("abort"), INTEGER_K, (void *)0, JABORT,0,0},
	{"jabort", N_("jabort"), INTEGER_K, (void *)0, JABORT,0,0},
	{"hold", N_("hold"), INTEGER_K, (void *)0, JHOLD,0,0},
	{"jhold", N_("jhold"), INTEGER_K, (void *)0, JHOLD,0,0},
	{"remove", N_("remove"), INTEGER_K, (void *)0, JREMOVE,0,0},
	{"jremove", N_("jremove"), INTEGER_K, (void *)0, JREMOVE,0,0},
	{ 0,0,0,0,0,0,0 }
};

static int Decode_transfer_failure( int attempt, struct job *job )
{
	struct keywords *key;
	int result, n, len, c;
	char line[SMALLBUFFER], *outstr;

	result = JREMOVE;
	outstr = Send_failure_action_DYN;
	if( outstr ) while( isspace(cval(outstr)) ) ++outstr;
	DEBUG1("Decode_transfer_failure: send_failure_action '%s'", outstr );
	if( outstr && cval(outstr) == '|' ){
		/* check to see if it is a filter */
		int out_tempfd, in_tempfd;

		outstr = 0;
		plp_snprintf( line, sizeof(line), "%d\n", attempt );
		out_tempfd = Make_temp_fd( 0);
		in_tempfd = Make_temp_fd( 0);
		if( Write_fd_str(in_tempfd,line) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Decode_transfer_failure: write(%d) failed",
				in_tempfd);
		}
		if( lseek(in_tempfd,0,SEEK_SET) == -1 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Decode_transfer_failure: fseek(%d) failed",
				in_tempfd);
		}
		n = Filter_file( Send_query_rw_timeout_DYN, in_tempfd, out_tempfd, "TRANSFER_FAILURE",
			Send_failure_action_DYN, Filter_options_DYN, job, 0, 1 );
		DEBUG1("Decode_transfer_failure: exit status %s", Server_status(n));
		if( n ){
			result = n;
			setstatus( job, "send_failure_action filter exit status '%s'",
				Server_status(result) );
		} else {
			if( lseek(out_tempfd,0,SEEK_SET) == -1 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO, "Decode_transfer_failure: fseek(%d) failed",
					out_tempfd);
			}
			len = ok_read( out_tempfd, line,sizeof(line)-1 );
			if( len >= 0 ){
				line[len] = 0;
			} else {
				Errorcode = JFAIL;
				logerr_die(LOG_INFO, "Decode_transfer_failure: read(%d) failed",
					out_tempfd);
			}
			while( (c = cval(line)) && strchr( Whitespace, c) ){
				memmove( line, line+1, safestrlen(line+1)+1 );
			}
			while( (len = safestrlen(line)) && (c = cval(line+len-1))
				&& strchr( Whitespace, c) ){
				line[len-1] = 0;
			}
			setstatus( job, "send_failure_action filter returned '%s'",
				outstr );
		}
		close( out_tempfd ); out_tempfd = -1;
		close( in_tempfd ); in_tempfd = -1;
	}
	if( outstr && *outstr ){
		DEBUG1("Decode_transfer_failure: outstr '%s'", outstr );
		for( key = keys; key->keyword; ++key ){
			DEBUG1("Decode_transfer_failure: comparing '%s' to '%s'",
				outstr, key->keyword );
			if( safestrcasecmp( key->keyword, outstr ) == 0 ){
				result = key->maxval;
				break;
			}
		}
	}
	DEBUG1("Decode_transfer_failure: result '%s'", Server_status(result) );
	setstatus( job, "send_failure_action '%s'", Server_status(result) );
	return( result );
}

static void Update_status( int fd, struct job *job, int status )
{
	char buffer[SMALLBUFFER];
	char *id, *did, *strv, *hf_name;
	struct line_list *destination;
	int copy, copies, attempt, destinations, n, done = 0;
	
	did = 0;
	destinations = 0;
	destination = 0;
	Set_decimal_value(&job->info,SERVER,0);

	id = Find_str_value(&job->info,IDENTIFIER);
	if( !id ){
		if(DEBUGL1)Dump_job("Update_status - no ID", job );
		return;
	}
	destinations = Find_flag_value(&job->info,DESTINATIONS);
	DEBUG1("Update_status: id '%s', destinations %d", id, destinations );

	if( destinations ){
		did = Find_str_value(&job->info,DESTINATION );
		DEBUG1("Update_status: id '%s', destinations %d, DESTINATION '%s'",
			id, destinations, did );
		if( !Get_destination_by_name( job, did ) ){
			destination = &job->destination;
			did = Find_str_value(destination,IDENTIFIER);
			if(!did) did = Find_str_value(destination,XXCFTRANSFERNAME);
			if(!did) did = Find_str_value(destination,HF_NAME);
			Set_decimal_value(destination,SERVER,0);
		}
	}
	setmessage(job,STATE,"EXITSTATUS %s", Server_status(status));

 again:
	DEBUG1("Update_status: again - status '%s', id '%s', dest id '%s'",
		Server_status(status), id, did );

	setmessage(job,STATE,"PROCESSSTATUS %s", Server_status(status));
	switch( status ){
		/* hold the destination stuff */
	case JHOLD:
		if( destination ){
			Set_flag_value(destination,HOLD_TIME,time((void *)0) );
			Update_destination(job);
		} else {
			Set_flag_value(&job->info,HOLD_TIME, time((void *)0) );
			Set_flag_value(&job->info,PRIORITY_TIME, 0 );
		}
		Set_job_ticket_file( job, 0, fd );
		break;

	case JSUCC:	/* successful, remove job */
		if(DEBUGL3)Dump_job("Update_status - JSUCC start", job );
		if( destination ){
			done = 0;
			copies = Find_flag_value(&job->info,SEQUENCE);
			Set_flag_value(&job->info,SEQUENCE,copies+1);
			copies = Find_flag_value(destination,COPIES);
			copy = Find_flag_value(destination,COPY_DONE);
			n = Find_flag_value(destination,DESTINATION);
			if( Find_str_value(destination,MOVE) ){
				Set_flag_value(destination,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: route job '%s' moved",
					Printer_DYN, FQDNHost_FQDN, did );
				done = 1;
			} else {
				++copy;
				Set_flag_value(destination,COPY_DONE,copy);
				if( copies ){
					setstatus( job,
					"%s@%s: route job '%s' printed copy %d of %d",
					Printer_DYN, FQDNHost_FQDN, id, copy, copies );
				}
				if( copy >= copies ){
					Set_flag_value(destination,DONE_TIME,time((void *)0));
					done = 1;
					++n;
				}
			}
			Update_destination(job);
			id = Find_str_value(&job->info,IDENTIFIER);
			if( done && n >= destinations ){
				Set_flag_value(&job->info,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: job '%s' printed",
					Printer_DYN, FQDNHost_FQDN, id );
				goto done_job;
			}
			Set_job_ticket_file( job, 0, fd );
			break;
		} else {
			copies = Find_flag_value(&job->info,COPIES);
			copy = Find_flag_value(&job->info,COPY_DONE);
			id = Find_str_value(&job->info,IDENTIFIER);
			if( !id ){
				Errorcode = JABORT;
				fatal(LOG_ERR,
					_("Update_status: no identifier for '%s'"),
					Find_str_value(&job->info,HF_NAME) );
			}

			if( Find_str_value(&job->info,MOVE) ){
				Set_flag_value(&job->info,DONE_TIME,time((void *)0));
				setstatus( job, "%s@%s: job '%s' moved",
					Printer_DYN, FQDNHost_FQDN, id );
			} else {
				++copy;
				Set_flag_value(&job->info,COPY_DONE,copy);
				if( copies ){
					setstatus( job, "%s@%s: job '%s' printed copy %d of %d",
					Printer_DYN, FQDNHost_FQDN, id, copy, copies );
				}
				if( copy >= copies ){
					Set_flag_value(&job->info,DONE_TIME,time((void *)0));
					Sendmail_to_user( status, job );
					setstatus( job, "%s@%s: job '%s' printed",
						Printer_DYN, FQDNHost_FQDN, id );
				} else {
					Set_job_ticket_file( job, 0, fd );
					break;
				}
			}
		done_job:
			hf_name = Find_str_value(&job->info,HF_NAME);
			DEBUG3("Update_status: done_job, id '%s', hf '%s'", id, hf_name );
			if( hf_name ){
				Set_flag_value(&job->info,REMOVE_TIME,time((void *)0));
				Set_job_ticket_file( job, 0, fd );
				if(DEBUGL3)Dump_job("Update_status - done_job", job );
				if( (Save_when_done_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
					setstatus( job, _("job '%s' saved"), id );
					++Done_count;
					if( !Done_time ) Done_time = time(0);
				} else {
					if( Remove_job( job ) ){
						setstatus( job, _("could not remove job '%s'"), id);
					} else {
						setstatus( job, _("job '%s' removed"), id );
					}
				}
			}
		}
		break;

	case JTIMEOUT:
	case JFAIL:	/* failed, retry ?*/
		status = JFAIL;
		if( destination ){
			attempt = Find_flag_value(destination,ATTEMPT);
			++attempt;
			Set_flag_value(destination,ATTEMPT,attempt);
			Update_destination(job);
		} else {
			attempt = Find_flag_value(&job->info,ATTEMPT);
			++attempt;
			Set_flag_value(&job->info,ATTEMPT,attempt);
		}
		DEBUG1( "Update_status: JFAIL - attempt %d, max %d",
			attempt, Send_try_DYN );
		Set_job_ticket_file( job, 0, fd );

		if( Send_try_DYN > 0 && attempt >= Send_try_DYN ){
			char buf[60];

			/* check to see what the failure action
			 *	should be - abort, failure; default is remove
			 */
			setstatus( job, _("job '%s', attempt %d, allowed %d"),
				id, attempt, Send_try_DYN );
			status = Decode_transfer_failure( attempt, job );
			switch( status ){
			case JSUCC:   strv = _("treating as successful"); break;
			case JFAIL:   strv = _("retrying job"); break;
			case JFAILNORETRY:   strv = _("no retry"); break;
			case JABORT:  strv = _("aborting server"); break;
			case JREMOVE: strv = _("removing job - status JREMOVE"); break;
			case JHOLD:   strv = _("holding job"); break;
			default:
				plp_snprintf( buf, sizeof(buf),
					_("unexpected status 0x%x"), status );
				strv = buf;
				status = JABORT;
				break;
			}
			setstatus( job, _("job '%s', %s"), id, strv );
		}
		if( status == JFAIL ){
			if( Send_try_DYN > 0 ){
				setstatus( job, _("job '%s' attempt %d, trying %d times"),
					id, attempt, Send_try_DYN );
			} else {
				setstatus( job, _("job '%s' attempt %d, trying indefinitely"),
					id, attempt);
			}
			if( destination ){
				Set_str_value(destination,ERROR,0);
				Set_flag_value(destination,ERROR_TIME,0);
				Set_str_value(destination,PRSTATUS,0);
			} else {
				Set_str_value(&job->info,ERROR,0);
				Set_flag_value(&job->info,ERROR_TIME,0);
				Set_str_value(&job->info,PRSTATUS,0);
			}
			Set_job_ticket_file( job, 0, fd );
		} else {
			goto again;
		}
		break;

	case JFAILNORETRY:	/* do not try again */
		plp_snprintf( buffer, sizeof(buffer), _("failed, no retry") );
		if( destination ){
			attempt = Find_flag_value(destination,ATTEMPT);
			++attempt;
			if( !Find_str_value(destination,ERROR) ){
				Set_str_value(destination,ERROR,buffer);
			}
			if( !Find_flag_value(destination,ERROR_TIME) ){
				Set_nz_flag_value(destination,ERROR_TIME,time(0));
			}
			Set_flag_value(destination,ATTEMPT,attempt);
			Update_destination(job);
			Set_job_ticket_file( job, 0, fd );
		} else {
			attempt = Find_flag_value(&job->info,ATTEMPT);
			++attempt;
			Set_flag_value(&job->info,ATTEMPT,attempt);
			if( !Find_str_value(&job->info,ERROR) ){
				Set_str_value(&job->info,ERROR,buffer);
			}
			if( !Find_flag_value(&job->info,ERROR_TIME) ){
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			}
			Set_nz_flag_value(&job->info,REMOVE_TIME, time( (void *)0) );
			Set_job_ticket_file( job, 0, fd );
			Sendmail_to_user( status, job );
			if( (Save_on_error_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
				setstatus( job, _("job '%s' saved"), id );
				++Done_count;
				if( !Done_time ) Done_time = time(0);
			} else {
				setstatus( job, _("removing job '%s' - JFAILNORETRY"), id);
				if( Remove_job( job ) ){
					setstatus( job, _("could not remove job '%s'"), id);
				} else {
					setstatus( job, _("job '%s' removed"), id );
				}
			}
		}
		break;

	default:
	case JABORT:	/* abort, do not try again */
		plp_snprintf(buffer,sizeof(buffer), _("aborting operations") );
		Set_flag_value(&job->info,PRIORITY_TIME,0);
		if( destination ){
			if( !Find_str_value(destination,ERROR) ){
				Set_str_value(destination,ERROR,buffer);
			}
			if( !Find_flag_value(destination,ERROR_TIME) ){
				Set_nz_flag_value(destination,ERROR_TIME,time(0));
			}
			strv = Find_str_value(destination,ERROR);
			Update_destination(job);
			setstatus( job, "job '%s', destination '%s', error '%s'",
				id,did,strv );
		} else {
			if( !Find_str_value(&job->info,ERROR) ){
				Set_str_value(&job->info,ERROR,buffer);
			}
			if( !Find_flag_value(&job->info,ERROR_TIME) ){
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			}
			strv = Find_str_value(&job->info,ERROR);
			setstatus( job, "job '%s' error '%s'",id, strv);
			Set_nz_flag_value(&job->info,REMOVE_TIME, time( (void *)0) );
			Set_job_ticket_file( job, 0, fd );
			Sendmail_to_user( status, job );
			if( (Save_on_error_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
				setstatus( job, _("job '%s' saved"), id );
				++Done_count;
				if( !Done_time ) Done_time = time(0);
			} else {
				setstatus( job, _("removing job '%s' - JABORT"), id);
				if( Remove_job( job ) ){
					setstatus( job, _("could not remove job '%s'"), id);
				} else {
					setstatus( job, _("job '%s' removed"), id );
				}
			}
		}
		if( Stop_on_abort_DYN ){
			setstatus( job, _("stopping printing on filter JABORT exit code") );
			Set_flag_value( &Spool_control,PRINTING_ABORTED,1 );
			Set_spool_control(0, Queue_control_file_DYN, &Spool_control);
		}
		break;

	case JREMOVE:	/* failed, remove job */
		if( destination ){
			if( !Find_str_value(destination,ERROR) ){
				plp_snprintf( buffer, sizeof(buffer),
					_("removing destination due to errors") );
				Set_str_value(destination,ERROR,buffer);
			}
			if( !Find_flag_value(destination,ERROR_TIME) ){
				Set_nz_flag_value(destination,ERROR_TIME,time(0));
			}
			Update_destination(job);
			Set_job_ticket_file( job, 0, fd );
		} else {
			if( !Find_str_value(&job->info,ERROR) ){
				plp_snprintf( buffer, sizeof(buffer),
					_("too many errors") );
				Set_str_value(&job->info,ERROR,buffer);
			}
			if( !Find_flag_value(&job->info,ERROR_TIME) ){
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
			}
			Set_nz_flag_value(&job->info,REMOVE_TIME, time( (void *)0) );
			Set_job_ticket_file( job, 0, fd );
			Sendmail_to_user( status, job );
			if( (Save_on_error_DYN || Done_jobs_DYN || Done_jobs_max_age_DYN) ){
				setstatus( job, _("job '%s' saved"), id );
				++Done_count;
				if( !Done_time ) Done_time = time(0);
			} else {
				setstatus( job, _("removing job '%s' - JREMOVE"), id);
				if( Remove_job( job ) ){
					setstatus( job, _("could not remove job '%s'"), id);
				} else {
					setstatus( job, _("job '%s' removed"), id );
				}
			}
		}
		break;
	}
	if(DEBUGL3)Dump_job("Update_status: exit result", job );
}

/***************************************************************************
 * int Check_print_perms
 *  check the printing permissions
 ***************************************************************************/

static int Check_print_perms( struct job *job )
{
	char *s;
	int permission;

	memset( &Perm_check, 0, sizeof(Perm_check) );
	Perm_check.service = 'P';
	Perm_check.printer = Printer_DYN;
	Perm_check.user = Find_str_value(&job->info,LOGNAME);
	Perm_check.remoteuser = Perm_check.user;
	Perm_check.authuser = Find_str_value(&job->info,AUTHUSER);
	Perm_check.authfrom = Find_str_value(&job->info,AUTHFROM);
	Perm_check.authtype = Find_str_value(&job->info,AUTHTYPE);
	Perm_check.authca = Find_str_value(&job->info,AUTHCA);
	s = Find_str_value(&job->info,FROMHOST);
	if( s && Find_fqdn( &PermHost_IP, s ) ){
		Perm_check.host = &PermHost_IP;
	}
	s = Find_str_value(&job->info,REMOTEHOST);
	if( s && Find_fqdn( &RemoteHost_IP, s ) ){
		Perm_check.remotehost = &RemoteHost_IP;
	} else {
		Perm_check.remotehost = Perm_check.host;
	}
	Perm_check.unix_socket = Find_flag_value(&job->info,UNIXSOCKET);
	Perm_check.port = Find_flag_value(&job->info,REMOTEPORT);
	permission = Perms_check( &Perm_line_list,&Perm_check, job, 1 );
	DEBUG3("Check_print_perms: permission '%s'", perm_str(permission) );
	return( permission );
}


static void Setup_user_reporting( struct job *job )
{
	char *host = Find_str_value(&job->info,MAILNAME);
	char *port = 0, *s;
	const char *protocol = "UDP";
	int prot_num = SOCK_DGRAM;
	char errmsg[SMALLBUFFER];


	DEBUG1("Setup_user_reporting: Allow_user_logging %d, host '%s'",
		Allow_user_logging_DYN, host );
	if( !Allow_user_logging_DYN || host==0
		|| safestrchr(host,'@') || !safestrchr(host,'%') ){
		return;
	}

	host = safestrdup(host,__FILE__,__LINE__);
	/* OK, we try to open a connection to the logger */
	if( (s = safestrchr( host, '%')) ){
		/* *s++ = 0; */
		port = s;
	}
	if( (s = safestrchr( port, ',')) ){
		*s++ = 0;
		protocol = s;
		if( safestrcasecmp( protocol, "TCP" ) == 0 ){
			protocol = "TCP";
			prot_num = SOCK_STREAM;
		}
	}
		
	DEBUG3("setup_logger_fd: host '%s', port '%s', protocol %d",
		host, port, prot_num );
	Mail_fd = Link_open_type(host, 10, prot_num, 0, 0, errmsg, sizeof(errmsg) );
	DEBUG3("Setup_user_reporting: Mail_fd '%d'", Mail_fd );

	if( Mail_fd > 0 && prot_num == SOCK_STREAM && Exit_linger_timeout_DYN > 0 ){
		Set_linger( Mail_fd, Exit_linger_timeout_DYN );
	}
	if( host ) free(host); host = 0;
}

void Service_worker( struct line_list *args, int param_fd UNUSED )
{
	int pid, unspooler_fd, destinations, attempt, n, lpd_bounce;
	struct line_list *destination;
	char *s, *path, *hf_name, *new_dest, *move_dest,
		*id, *did;
	struct stat statb;
	char buffer[SMALLBUFFER];
	struct job job;
	int fd = -1;

	Name="(Worker)";
	destination = 0;
	attempt = 0;

	Init_job(&job);

	Set_DYN(&Printer_DYN, Find_str_value(args,PRINTER));
	setproctitle( "lpd %s '%s'", Name, Printer_DYN );

	DEBUG1("Service_worker: begin");

	(void) plp_signal(SIGUSR1, (plp_sigfunc_t)cleanup_USR1);
	Errorcode = JABORT;

	/* you need to have a spool queue */
	if( Setup_printer( Printer_DYN, buffer, sizeof(buffer), 0 ) ){
		cleanup(0);
	}

	if(DEBUGL4){ int fd; fd = dup(0);
	LOGDEBUG("Service_worker: after Setup_printer next fd %d",fd); close(fd); };

	pid = getpid();
	DEBUG1( "Service_worker: pid %d", pid );
	path = Make_pathname( Spool_dir_DYN, Queue_unspooler_file_DYN );
	if( (unspooler_fd = Checkwrite( path, &statb, O_RDWR, 1, 0 )) < 0 ){
		logerr_die(LOG_ERR, _("Service_worker: cannot open lockfile '%s'"),
			path );
	}
	if(path) free(path); path = 0;
	Write_pid( unspooler_fd, pid, (char *)0 );
	close(unspooler_fd); unspooler_fd = -1;

	DEBUG3("Service_worker: checking path '%s'", path );

	hf_name = Find_str_value(args,HF_NAME);
	Get_job_ticket_file( &fd, &job, hf_name );
	if( !job.info.count ){
		DEBUG3("Service_worker: missing files");
		Errorcode = 0;
		cleanup(0);
	}

	Set_str_value(&job.info,NEW_DEST, Find_str_value(args,NEW_DEST));
	Set_str_value(&job.info,MOVE_DEST, Find_str_value(args,MOVE_DEST));
	Set_decimal_value(&job.info,SERVER,getpid());

	Free_line_list(args);

	n = Set_job_ticket_file( &job, 0, fd );
	if( n ){
		/* you cannot update job ticket file!! */
		setstatus( &job, _("cannot update job ticket file for '%s'"),
			hf_name );
		fatal(LOG_ERR,
			_("Service_worker: cannot update job ticket file for '%s'"), 
			hf_name );
	}
	if( fd > 0 ) close(fd); fd = -1;

	id = Find_str_value(&job.info,IDENTIFIER);
	if( !id ){
		fatal(LOG_ERR,
			_("Service_worker: no identifier for '%s'"),
			Find_str_value(&job.info,HF_NAME) );
	}

	if( (destinations = Find_flag_value(&job.info,DESTINATIONS)) ){
		did = Find_str_value(&job.info,DESTINATION );
		if( !Get_destination_by_name( &job, did ) ){
			destination = &job.destination;
			attempt = Find_flag_value(destination,ATTEMPT);
		}
	} else {
		attempt = Find_flag_value(&job.info,ATTEMPT);
	}
	DEBUG3("Service_worker: attempt %d", attempt );
	new_dest = Find_str_value(&job.info,NEW_DEST);
	move_dest = Find_str_value(&job.info,MOVE_DEST);
	lpd_bounce = Lpd_bounce_DYN;
	if( move_dest ){
		lpd_bounce = 0;
		new_dest = move_dest;
	}

	/*
	 * The following code is implementing job handling as follows.
	 *  if new_dest has a value then
	 *    new_dest has format 'pr' or 'pr@host'
	 *    if pr@host then
	 *       set RemotePrinter_DYN and RemoteHost_DYN
	 *   else
	 *       set RemotePrinter_DYN to pr
	 *       set RemoteHost_DYN to FQDNHost_FQDN
	 */

	if( new_dest ){
		Set_DYN( &RemoteHost_DYN, 0);
		Set_DYN( &RemotePrinter_DYN, 0);
		Set_DYN( &Lp_device_DYN, 0);

		Set_DYN( &RemotePrinter_DYN, new_dest );
		if( (s = safestrchr(RemotePrinter_DYN, '@')) ){
			*s++ = 0;
			Set_DYN( &RemoteHost_DYN, s );
			if( (s = safestrchr(s,'%')) ){
				*s++ = 0;
				Set_DYN( &Lpd_port_DYN,s );
			}
		}
		if( !RemoteHost_DYN ){
			Set_DYN( &RemoteHost_DYN, LOCALHOST);
		}
	}
	
	/* we put a timeout before each attempt */
	if( attempt > 0 ){
		n = 8;
		if( attempt < n ) n = attempt;
		n = Connect_interval_DYN * (1 << (n-1)) + Connect_grace_DYN;
		if( Max_connect_interval_DYN > 0 && n > Max_connect_interval_DYN ){
			n = Max_connect_interval_DYN;
		}
		DEBUG1("Service_worker: attempt %d, sleeping %d", attempt, n);
		if( n > 0 ){
			setstatus( &job, "attempt %d, sleeping %d before retry", attempt+1, n );
			plp_sleep(n);
		}
	}

	if( RemotePrinter_DYN ){
		Name = "(Worker - Remote)";
		DEBUG1( "Service_worker: sending '%s' to '%s@%s'",
			id, RemotePrinter_DYN, RemoteHost_DYN );
		setproctitle( "lpd %s '%s'", Name, Printer_DYN );
		if( Remote_support_DYN ) uppercase( Remote_support_DYN );
		if( safestrchr( Remote_support_DYN, 'R' ) ){
			Errorcode = Remote_job( &job, lpd_bounce, move_dest, id );
		} else {
			Errorcode = JABORT;
			setstatus( &job, "no remote support to `%s@%s'",
			RemotePrinter_DYN, RemoteHost_DYN );
		}
	} else {
		Name = "(Worker - Print)";
		DEBUG1( "Service_worker: printing '%s'", id );
		setproctitle( "lpd %s '%s'", Name, Printer_DYN );
		Errorcode = Local_job( &job, id );
	}
	cleanup(0);
}

/*
 * Filter all the files in the print job
 */
static void Filter_files_in_job( struct job *job, int outfd, char *user_filter )
{
	struct line_list *datafile;
	char *tempfile, *openname, *s, *filter, *id, *old_lp_value;
	const char *format;
	char filter_name[8], filter_title[64], msg[SMALLBUFFER],
		filtermsgbuffer[SMALLBUFFER];
	struct stat statb;
	int tempfd, fd, n, pid, count, if_error[2];
	struct line_list files;

	Init_line_list(&files);
	DEBUG1("Filter_files_in_job: starting, user_filter '%s'", user_filter);
    if(DEBUGL3){
		struct stat statb; int i;
        LOGDEBUG("Filter_files_in_job: START open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, (unsigned int)(statb.st_mode&S_IFMT));
            }
        }
    }
	Errorcode = 0;
	old_lp_value = safestrdup(Find_str_value( &PC_entry_line_list, "lp" ),
		__FILE__,__LINE__ );

	id = Find_str_value(&job->info,IDENTIFIER);
	tempfd = -1;
	for( count = 0; count < job->datafiles.count; ++count ){
		datafile = (void *)job->datafiles.list[count];
		if(DEBUGL4)Dump_line_list("Filter_files_in_job - datafile", datafile );

		openname = Find_str_value(datafile,OPENNAME);
		if( !openname ) openname = Find_str_value(datafile,DFTRANSFERNAME);
		format = Find_str_value(datafile,FORMAT);

		Set_str_value(&job->info,FORMAT,format);
		Set_str_value(&job->info,DF_NAME,openname);
		Set_str_value(&job->info,"N", Find_str_value(datafile,"N") );

		/*
		 * now we check to see if there is an input filter
		 */
		plp_snprintf(filter_name,sizeof(filter_name), "%s","if");
		filter_name[0] = cval(format);
		filter = user_filter;
		switch( cval(format) ){
			case 'p': case 'f': case 'l':
				filter_name[0] = 'i';
				if( !filter ) filter = IF_Filter_DYN;
				break;
			case 'a': case 'i': case 'o': case 's':
				setstatus(job, "bad data file format '%c', using 'f' format", cval(format) );
				filter_name[0] = 'i';
				format = "f";
				if( !filter ) filter = IF_Filter_DYN;
				break;
		}
		if( !filter ){
			filter = Find_str_value(&PC_entry_line_list, filter_name);
		}
		if( !filter){
			filter = Find_str_value(&Config_line_list,filter_name );
		}
		if( filter == 0 ) filter = Filter_DYN;
		if( filter == 0 ) filter = IF_Filter_DYN;
		DEBUG3("Filter_files_in_job: format '%s', filter '%s'", format, filter );

		if( filter == 0 ){
			continue;
		}

		uppercase(filter_name);
		if( filter ){
			s = filter;
			if( cval(s) == '(' ){
				++s;
				while( isspace(cval(s))) ++s;
			} else {
				if( !(s = strchr(filter,'/')) ) s = filter;
			}
			plp_snprintf(msg, sizeof(msg), "%s", s );
			if( (s = strpbrk(msg,Whitespace)) ) *s = 0;
			if( (s = strrchr(msg,'/')) ) memmove(msg,s+1,safestrlen(s+1)+1);
		}
		plp_snprintf(filter_title,sizeof(filter_title), "%s filter '%s'",
			filter_name, msg );

		if( (fd = Checkread( openname, &statb )) < 0 ){
			Errorcode = JFAIL;
			logmsg( LOG_ERR, "Filter_files_in_job: job '%s', cannot open data file '%s'",
				id, openname );
			goto end_of_job;
		}
		setstatus(job, "processing '%s', size %0.0f, format '%s', %s",
			openname, (double)statb.st_size, format, filter_title );
		if( cval(format) == 'p' ){
			DEBUG3("Filter_files_in_job: using 'p' formatter '%s'", Pr_program_DYN );
			setstatus(job, "format 'p' pretty printer '%s'", Pr_program_DYN);
			if( Pr_program_DYN == 0 ){
				setstatus(job, "no 'p' format filter available" );
				Errorcode = JABORT;
				goto end_of_job;
			}
			tempfd = Make_temp_fd(&tempfile);
			Set_str_value(datafile,OPENNAME,tempfile);
			n = Filter_file( Send_job_rw_timeout_DYN, fd, tempfd, "PR_PROGRAM",
				Pr_program_DYN, 0, job, 0, 1 );
			if( n ){
				Errorcode = JABORT;
				logerr(LOG_INFO, "Filter_files_in_job:  could not make '%s' process",
					Pr_program_DYN );
				goto end_of_job;
			}
			close(fd); fd = tempfd; tempfd = -1;
			if( fstat(fd, &statb ) == -1 ){
				Errorcode = JABORT;
				logerr(LOG_INFO, "Filter_files_in_job: fstat() failed");
			}
			setstatus(job, "data file '%s', size now %0.0f",
				openname, (double)statb.st_size );
			format = "f";
			Set_str_value(datafile,FORMAT,format);
		}
		if( filter ){
			DEBUG3("Filter_files_in_job: format '%s' starting filter '%s'",
				format, filter );
			DEBUG2("Filter_files_in_job: filter_stderr_to_status_file %d, ps '%s'",
				Filter_stderr_to_status_file_DYN, Status_file_DYN );
			if_error[0] = if_error[1] = -1;
			if( Filter_stderr_to_status_file_DYN && Status_file_DYN && *Status_file_DYN ){
				if_error[1] = Checkwrite( Status_file_DYN, &statb, O_WRONLY|O_APPEND, 0, 0 );
			} else if( pipe( if_error ) == -1 ){
				Errorcode = JFAIL;
				logerr(LOG_INFO, "Filter_files_in_job: pipe() failed");
				goto end_of_job;
			}
			Max_open(if_error[0]); Max_open(if_error[1]);
			DEBUG3("Filter_files_in_job: %s fd if_error[%d,%d]", filter_title,
				 if_error[0], if_error[1] );
			s = 0;
			if( Backwards_compatible_filter_DYN ) s = BK_filter_options_DYN;
			if( s == 0 ) s = Filter_options_DYN;

			Free_line_list(&files);
			Check_max(&files, 10 );
			files.list[files.count++] = Cast_int_to_voidstar(fd);		/* stdin */

			if( outfd > 0 ){
				files.list[files.count++] = Cast_int_to_voidstar(outfd);	/* stdout */
			} else {
				tempfd = Make_temp_fd(&tempfile);
				Set_str_value( &PC_entry_line_list, LP, tempfile );
				Set_str_value(datafile,OPENNAME,tempfile);
				files.list[files.count++] = Cast_int_to_voidstar(tempfd);	/* stdout */
			}

			files.list[files.count++] = Cast_int_to_voidstar(if_error[1]);	/* stderr */
			if( (pid = Make_passthrough( filter, s, &files, job, 0 )) < 0 ){
				Errorcode = JFAIL;
				logerr(LOG_INFO, "Filter_files_in_job:  could not make %s process",
					filter_title );
				goto end_of_job;
			}
			files.count = 0;
			Free_line_list(&files);

			if( fd > 0 ) close(fd); fd = -1;
			if( tempfd > 0 ) close(tempfd); tempfd = -1;
			if( (close(if_error[1]) == -1 ) ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO, "Filter_files_in_job: X5 close(%d) failed",
					if_error[1]);
			}
			if_error[1] = -1;
			Init_buf(&Outbuf, &Outmax, &Outlen );

			filtermsgbuffer[0] = 0;
			if( if_error[0] != -1 ){
				n = Get_status_from_OF(job,filter_title,pid,
					if_error[0], filtermsgbuffer, sizeof(filtermsgbuffer)-1,
					0, 0, 0, Status_file_DYN );
				if( filtermsgbuffer[0] ){
					setstatus(job, "%s filter msg - '%s'", filter_title, filtermsgbuffer );
				}
				if( n ){
					Errorcode = n;
					setstatus(job, "%s filter problems, error '%s'",
						filter_title, Server_status(n));
					goto end_of_job;
				}
				close(if_error[0]);
				if_error[0] = -1;
			}
			/* now we get the exit status for the filter */
			n = Wait_for_pid( pid, filter_title, 0, 0 );
			if( n ){
				Errorcode = n;
				setstatus(job, "%s filter exit status '%s'",
					filter_title, Server_status(n));
				goto end_of_job;
			}
			setstatus(job, "%s filter finished", filter_title );
			Fix_bq_format( cval(format), datafile );
		}
		DEBUG3("Filter_files_in_job: finished file");
	}
 end_of_job:
	if( old_lp_value ) free( old_lp_value ); old_lp_value = 0;
	Free_line_list(&files);
    if(DEBUGL3){
		struct stat statb; int i;
        LOGDEBUG("Filter_files_in_job: END open fd's");
        for( i = 0; i < 20; ++i ){
            if( fstat(i,&statb) == 0 ){
                LOGDEBUG("  fd %d (0%o)", i, (unsigned int)(statb.st_mode&S_IFMT));
            }
        }
    }
	if(DEBUGL3)Dump_job("Filter_files_in_job", job);
}

void Service_queue( struct line_list *args, int param_fd UNUSED )
{
	int subserver;

	Set_DYN(&Printer_DYN, Find_str_value(args, PRINTER) );
	subserver = Find_flag_value( args, SUBSERVER );

	Free_line_list(args);
	Do_queue_jobs( Printer_DYN, subserver );
	cleanup(0);
}


int Remove_done_jobs( void )
{
	struct job job;
	char *id;
	int removed = 0, last_remove = 0, remove_count = 0, fd;
	time_t tm;
	int job_index, info_index, pid, remove, error, done, incoming;
	struct line_list info;
	char tval[SMALLBUFFER];

	DEBUG3("Remove_done_jobs: save_when_done %d, save_on_error %d, done_jobs %d, d_j_max_age %d",
		Save_when_done_DYN, Save_on_error_DYN,
		Done_jobs_DYN, Done_jobs_max_age_DYN );
	if( Save_when_done_DYN || Save_on_error_DYN
		|| !(Done_jobs_DYN > 0 || Done_jobs_max_age_DYN > 0) ){
		return( 0 );
	}

	Init_line_list(&info);
	time( &tm );
	Init_job(&job);
	fd = -1;
	for( job_index = 0; job_index < Sort_order.count; ++job_index ){
		char *job_ticket_file = Sort_order.list[job_index];
		Free_job(&job);
		if( fd > 0 ) close(fd); fd = -1;
		if( ISNULL(job_ticket_file) ) continue;
		DEBUG3("Remove_done_jobs: done_jobs - job_index [%d] '%s'", job_index,
			job_ticket_file);
		Get_job_ticket_file( &fd, &job, job_ticket_file );
		if(DEBUGL4)Dump_job("Remove_done_jobs: done_jobs - job ",&job);
		if( job.info.count == 0 ) continue;
		/* get status from job ticket file */
		id = Find_str_value(&job.info,IDENTIFIER);
		done = Find_flag_value(&job.info,DONE_TIME);
		error = Find_flag_value(&job.info,ERROR_TIME);
		incoming = Find_flag_value(&job.info,INCOMING_TIME);
		pid = Find_flag_value(&job.info,INCOMING_PID);
		remove = Find_flag_value(&job.info,REMOVE_TIME);
		DEBUG3("Remove_done_jobs: remove 0x%x, done 0x%x, error 0x%x, incoming 0x%x",
			remove, done, error, incoming );
		if( incoming && pid && kill( pid, 0 ) ){
			/* we have a stale incoming job */
			Remove_job( &job );
			continue;
		}
		if( !(remove || (error && !Save_on_error_DYN)) ) continue;
		if( last_remove != remove ){
			remove_count = 0;
		}
		last_remove = remove;
		++remove_count;
		if( (pid = Find_flag_value(&job.info,SERVER)) && kill( pid, 0 ) == 0 ){
			DEBUG3("Remove_done_jobs: '%s' active %d", job_ticket_file, pid );
			continue;
		}
		if( Done_jobs_max_age_DYN > 0
			&& ( (error && (tm - error) > Done_jobs_max_age_DYN)
			   || (done && (tm - done) > Done_jobs_max_age_DYN) ) ){
			setstatus( &job, _("job '%s' removed- status expired"), id );
			/* Setup_cf_info( &job, 0 ); */
			Remove_job( &job );
		} else if( Done_jobs_DYN > 0 ){
			plp_snprintf(tval,sizeof(tval), "0x%06x.%03d", remove, remove_count );
			Set_str_value(&info, tval, job_ticket_file );
		}
	}
	if( fd > 0 ) close(fd); fd = -1;

	if(DEBUGL1)Dump_line_list("Remove_done_jobs - removal candidates",&info);
	DEBUG1( "Remove_done_jobs: checking for removal - remove_count %d", Done_jobs_DYN );

	for( info_index = 0; info_index < info.count - Done_jobs_DYN; ++info_index ){
		char *job_ticket_file = info.list[info_index];
		if( (job_ticket_file = safestrchr( job_ticket_file, '=' )) ){
			++job_ticket_file;
		} else {
			Errorcode = JABORT;
			fatal(LOG_ERR,"Remove_done_jobs: bad job ticket file format '%s'",
				info.list[info_index]);
		}
		DEBUG1( "Remove_done_jobs: [%d] job_ticket_file '%s'",
			info_index, job_ticket_file );
		Free_job(&job);
		Get_job_ticket_file( &fd, &job, job_ticket_file );
		Remove_job( &job );
		if( fd > 0 ) close(fd); fd = -1;
		removed = 1;
	}
	Free_job(&job);
	Free_line_list(&info);
	if( removed && Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN);
	}
	return( removed );
}

/*
 * move the job to a new spool queue
 *  This will only work if the queue/printer is on the same
 *  host.  We will set up a job in the new spool queue that
 *  appears to sent directly to the spool queue.
 */

static int Move_job(int fd, struct job *job, struct line_list *sp,
	char *errmsg, int errlen )
{
	/* we set up a copy of the job descriptor to use to make
		the job in the new directory */
	int job_ticket_file_fd = -1, fail = 0, i;
	struct job jcopy;
	struct line_list datafiles;
	char *transfername = 0;
	char *savename = 0, *sd, *pr, *id;

	Init_line_list(&datafiles);

	Init_job(&jcopy);
	Copy_job(&jcopy,job);

	Set_str_value(&jcopy.info,SERVER,0);
	Set_str_value(&jcopy.info,MOVE,0);
	Set_str_value(&jcopy.info,DONE_TIME,0);
	Set_str_value(&jcopy.info,HOLD_TIME,0);
	Set_str_value(&jcopy.info,PRIORITY_TIME, 0 );
	Set_str_value(&jcopy.info,ERROR_TIME, 0 );
	Set_str_value(&jcopy.info,ERROR, 0 );
	Set_str_value(&jcopy.info,DESTINATION, 0 );
	Set_str_value(&jcopy.info,DESTINATIONS, 0 );

	i = Find_flag_value(&jcopy.info,MOVE_COUNT);
	Set_flag_value(&jcopy.info,MOVE_COUNT,i+1);

	if(DEBUGL2)Dump_job("Move_job: use_subserver copy", &jcopy );

	sd = Find_str_value(sp,SPOOLDIR);
	pr = Find_str_value(sp,PRINTER);
	id = Find_str_value(&job->info,IDENTIFIER);

	DEBUG1("Move_job: subserver '%s', spool dir '%s' for job '%s'",
	   pr, sd, id );
	setstatus(job, "moving '%s' to subserver '%s'", id, pr ); 

	fail = 0;
	for( i = 0; i < jcopy.datafiles.count; ++i ){
		char *from;
		struct line_list * datafile = (void *)jcopy.datafiles.list[i];
		if(DEBUGL3)Dump_line_list("Move_job - copying datafiles",
			datafile);
		from = Find_str_value(datafile,DFTRANSFERNAME);
		Set_str_value(datafile,OTRANSFERNAME,from);
		if( !Find_str_value(&datafiles,from) ){
			char * path = Make_temp_copy( from, sd );
			DEBUG3("Move_job: sd '%s', from '%s', path '%s'",
				sd, from, path );
			if( path ){
				Set_str_value( &datafiles, from, path );
			} else {
				plp_snprintf(errmsg,errlen, "cannot copy '%s' to subserver '%s' queue directory '%s'",
					from?from:"<no DFTRANSDERNAME>", pr, sd );
				Set_str_value(&job->info,ERROR,errmsg);
				fail = 1;
				goto move_error;
			}
		}
	}

	/* set up the new context */
	savename = safestrdup( Printer_DYN,__FILE__,__LINE__);
	errmsg[0] = 0;

	/* we have to chdir to the destination directory */
	if( Setup_printer( pr, errmsg, errlen, 1 ) ){
		Errorcode = JABORT;
		fatal(LOG_ERR, "Move_job: subserver '%s' setup failed - %s'",
				pr, errmsg );
	}
	
	job_ticket_file_fd = Setup_temporary_job_ticket_file( &jcopy, 0,
		0, 0,  errmsg, errlen );
	if( job_ticket_file_fd <= 0 ){
		fail = 1;
		goto move_error;
	}
	if(DEBUGL2)Dump_job("Move_job: subserver after temp setup", &jcopy );
	/* get the job set up */
	transfername = Find_str_value(&jcopy.info,HF_NAME);
	fail = Check_for_missing_files( &jcopy, &datafiles,
			errmsg, errlen, 0, job_ticket_file_fd );
	if( fail ) unlink( transfername );

	/* now we switch back to the old context */
	if( Setup_printer( savename, errmsg, errlen, 1 ) ){
		Errorcode = JABORT;
		fatal(LOG_ERR, "Move_job: subserver '%s' setup failed '%s'",
			savename, errmsg );
	}

 move_error:
	/* if we fail to copy, make a note of it */
	Free_line_list( &datafiles );
	Free_job(&jcopy);
	Remove_tempfiles();
	if(savename) free(savename); savename = 0;
	if( job_ticket_file_fd > 0 ) close(job_ticket_file_fd); job_ticket_file_fd = -1;
	if( fail ){
		setstatus(job, "%s", errmsg);
		Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
		Update_status(fd, job, JFAIL);
	} else {
		/* now we deal with the job in the original queue */
		Update_status(fd, job, JSUCC);
		setstatus(job, "transfer '%s' to queue '%s' finished", id, pr );
		setmessage(job,STATE,"COPYTO %s",pr);
	}
	return( fail );
}
