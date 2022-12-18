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
#include "child.h"
#include "getqueue.h"
#include "linelist.h"
#include "lpd_worker.h"

/* this file contains code that was formerly in linelist.c but split
 * out as it is only needed in lpd and pulls much code with it */

static void Do_work( const char *name, struct line_list *args, WorkerProc *proc,
		int intern_logger, int intern_status, int intern_mail,
		int intern_lpd_rquest, int intern_fd ) NORETURN;

/*
 * Make_lpd_call - does the actual forking operation
 *  - sets up file descriptor for child, can close_on_exec()
 *  - does fork() as appropriate
 *
 *  returns: pid of child or -1 if fork failed.
 */

static pid_t Make_lpd_call( const char *name, WorkerProc *proc, int passfd_count, int *passfd, struct line_list *args, int intern_logger, int intern_status, int intern_mail, int intern_lpd_request, int param_fd )
{
	int pid, fd, i, n, newfd;
	struct line_list env;

	Init_line_list(&env);
	pid = dofork(1);
	if( pid ){
		return(pid);
	}
	Name = "LPD_CALL";

	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: name '%s', lpd path '%s'", name, Lpd_path_DYN );
		LOGDEBUG("Make_lpd_call: passfd count %d", passfd_count );
		for( i = 0; i < passfd_count; ++i ){
			LOGDEBUG(" [%d] %d", i, passfd[i]);
		}
		Dump_line_list("Make_lpd_call - args", args );
	}
	for( i = 0; i < passfd_count; ++i ){
		fd = passfd[i];
		if( fd < i  ){
			/* we have fd 3 -> 4, but 3 gets wiped out */
			do{
				newfd = dup(fd);
				Max_open(newfd);
				if( newfd < 0 ){
					Errorcode = JABORT;
					logerr_die(LOG_INFO, "Make_lpd_call: dup failed");
				}
				DEBUG4("Make_lpd_call: fd [%d] = %d, dup2 -> %d",
					i, fd, newfd );
				passfd[i] = newfd;
			} while( newfd < i );
		}
	}
	if(DEBUGL2){
		LOGDEBUG("Make_lpd_call: after fixing fd count %d", passfd_count);
		for( i = 0 ; i < passfd_count; ++i ){
			fd = passfd[i];
			LOGDEBUG("  [%d]=%d",i,fd);
		}
	}
	for( i = 0; i < passfd_count; ++i ){
		fd = passfd[i];
		DEBUG2("Make_lpd_call: fd %d -> %d",fd, i );
		if( dup2( fd, i ) == -1 ){
			Errorcode = JABORT;
			logerr_die(LOG_INFO, "Make_lpd_call: dup2(%d,%d) failed",
				fd, i );
		}
	}
	/* close other ones to simulate close_on_exec() */
	n = Max_fd+10;
	for( i = passfd_count ; i < n; ++i ){
		close(i);
	}
	Do_work( name, args, proc,
			intern_logger, intern_status, intern_mail,
			intern_lpd_request, param_fd );
	/* not reached: */
	return(0);
}

static void Do_work( const char *name, struct line_list *args, WorkerProc *proc,
		int intern_logger, int intern_status, int intern_mail,
		int intern_lpd_request, int param_fd )
{
	Logger_fd = intern_logger;
	Status_fd = intern_status;
	Mail_fd = intern_mail;
	Lpd_request = intern_lpd_request;
	/* undo the non-blocking IO */
	if( Lpd_request > 0 ){
		/* undo the non-blocking IO */
		Set_block_io( Lpd_request );
	}
	Debug= Find_flag_value( args, DEBUG );
	DbgFlag= Find_flag_value( args, DEBUGFV );
#ifdef DMALLOC
	{
		extern int dmalloc_outfile_fd;
		dmalloc_outfile_fd = intern_dmalloc;
	}
#endif
	DEBUG3("Do_work: '%s', proc 0x%lx ", name, Cast_ptr_to_long(proc) );
	(proc)(args, param_fd);
	cleanup(0);
}

/*
 * Start_worker - general purpose dispatch function
 *   - adds an input FD
 */

pid_t Start_worker( const char *name, WorkerProc *proc, struct line_list *parms, int fd )
{
	struct line_list args;
	int passfd[20];
	pid_t pid;
	int intern_fd = 0,
	    intern_logger = -1,
	    intern_status = -1,
	    intern_mail = -1,
	    intern_lpd = -1;
	int passfd_count = 0;

	Init_line_list(&args);
	passfd[passfd_count++] = 0;
	passfd[passfd_count++] = 1;
	passfd[passfd_count++] = 2;
	if( Mail_fd > 0 ){
		intern_mail = passfd_count;
		passfd[passfd_count++] = Mail_fd;
	}
	if( Status_fd > 0 ){
		intern_status = passfd_count;
		passfd[passfd_count++] = Status_fd;
	}
	if( Logger_fd > 0 ){
		intern_logger = passfd_count;
		passfd[passfd_count++] = Logger_fd;
	}
	if( Lpd_request > 0 ){
		intern_lpd = passfd_count;
		passfd[passfd_count++] = Lpd_request;
	}
	Set_flag_value(&args,DEBUG,Debug);
	Set_flag_value(&args,DEBUGFV,DbgFlag);
#ifdef DMALLOC
	{
		extern int dmalloc_outfile_fd;
		if( dmalloc_outfile_fd > 0 ){
			intern_dmalloc = passfd_count;
			passfd[passfd_count++] = dmalloc_outfile_fd;
		}
	}
#endif
	if(DEBUGL1){
		DEBUG1("Start_worker: '%s' fd %d", name, fd );
		Dump_line_list("Start_worker - parms", parms );
	}
	Merge_line_list( &args, parms, Hash_value_sep,1,1);
	Free_line_list( parms );
	if( fd ){
		intern_fd = passfd_count;
		passfd[passfd_count++] = fd;
	}

	pid = Make_lpd_call( name, proc, passfd_count, passfd, &args,
			intern_logger, intern_status, intern_mail, intern_lpd,
			intern_fd );
	Free_line_list( &args );
	return(pid);
}
