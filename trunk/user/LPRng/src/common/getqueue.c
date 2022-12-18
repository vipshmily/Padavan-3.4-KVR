/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

/***************************************************************************
 * Commentary
 * Patrick Powell Thu Apr 27 21:48:38 PDT 1995
 * 
 * The spool directory contains files and other information associated
 * with jobs.  Job files have names of the form cfXNNNhostname.
 * 
 * The Scan_queue routine will scan a spool directory, looking for new
 * or missing control files.  If one is found,  it will add it to
 * the control file list.  It will then sort the list of file names.
 * 
 * In order to prevent strange things with pointers,  you should not store
 * pointers to this list,  but use indexes instead.
 ***************************************************************************/

#include "lp.h"
#include "child.h"
#include "errorcodes.h"
#include "fileopen.h"
#include "linelist.h"
#include "getprinter.h"
#include "gethostinfo.h"
#include "getqueue.h"
#include "globmatch.h"
#include "permission.h"
#include "lockfile.h"
#include "merge.h"

#if defined(USER_INCLUDE)
# include USER_INCLUDE
#else
# if defined(ORDER_ROUTINE)
#   error No 'USER_INCLUDE' file with ORDER_ROUTINE function prototypes specified
    You need an include file with function prototypes
# endif
#endif

/**** ENDINCLUDE ****/

/* prototypes of functions only used internally */
static void Append_Z_value( struct job *job, char *s );
static void Set_job_ticket_datafile_info( struct job *job );
static int ordercomp(  const void *left, const void *right, const void *orderp);

/*
 * We make the following assumption:
 *   a job consists of a job ticket file and a set of data files.
 *   data files without a job ticket file or a job ticket file
 *   without a data files (unless marked as 'incoming') will be
 *   considered as a bad job.
 */

int Scan_queue( struct line_list *spool_control,
	struct line_list *sort_order, int *pprintable, int *pheld, int *pmove,
		int only_queue_process, int *perr, int *pdone,
		const char *remove_prefix, const char *remove_suffix )
{
	DIR *dir;						/* directory */
	struct dirent *d;				/* directory entry */
	char *job_ticket_name;
	int c, printable, held, move, error, done, p, h, m, e, dn;
	int remove_prefix_len = safestrlen( remove_prefix );
	int remove_suffix_len = safestrlen( remove_suffix );
	struct job job;

	c = printable = held = move = error = done = 0;
	Init_job( &job );
	if( pprintable ) *pprintable = 0;
	if( pheld ) *pheld = 0;
	if( pmove ) *pmove = 0;
	if( perr ) *perr = 0;
	if( pdone ) *pdone = 0;

	Free_line_list(sort_order);

	if( !(dir = opendir( "." )) ){
		logerr(LOG_INFO, "Scan_queue: cannot open '.'" );
		return( 1 );
	}

	job_ticket_name = 0;
	while( (d = readdir(dir)) ){
		job_ticket_name = d->d_name;
		DEBUG5("Scan_queue: found file '%s'", job_ticket_name );
		if(
			(remove_prefix_len && !strncmp( job_ticket_name, remove_prefix, remove_prefix_len ) )
			|| (remove_suffix_len 
				&& !strcmp( job_ticket_name+strlen(job_ticket_name)-remove_suffix_len, remove_suffix ))
		){
			DEBUG1("Scan_queue: removing file '%s'", job_ticket_name );
			unlink( job_ticket_name );
			continue;
		} else if(  !(   (cval(job_ticket_name+0) == 'h')
			&& (cval(job_ticket_name+1) == 'f')
			&& isalpha(cval(job_ticket_name+2))
			&& isdigit(cval(job_ticket_name+3))
			) ){
			continue;
		}

		DEBUG2("Scan_queue: processing file '%s'", job_ticket_name );

		Free_job( &job );

		/* read the hf file and get the information */
		Get_job_ticket_file( 0, &job, job_ticket_name );
		if(DEBUGL3)Dump_line_list("Scan_queue: hf", &job.info );
		if( job.info.count == 0 ){
			continue;
		}

		Job_printable(&job,spool_control, &p,&h,&m,&e,&dn);
		if( p ) ++printable;
		if( h ) ++held;
		if( m ) ++move;
		if( e ) ++error;
		if( dn ) ++done;

		/* now generate the sort key */
		DEBUG4("Scan_queue: p %d, m %d, e %d, dn %d, only_queue_process %d",
			p, m, e, dn, only_queue_process );
		if( sort_order ){
			if( !only_queue_process || (p || m || e || dn) ){
				if(DEBUGL4)Dump_job("Scan_queue - before Make_sort_key",&job);
				Make_sort_key( &job );
				DEBUG5("Scan_queue: sort key '%s'",job.sort_key);
				Set_str_value(sort_order,job.sort_key,job_ticket_name);
			}
		}
	}
	closedir(dir);

	Free_job(&job);

	if(DEBUGL5){
		LOGDEBUG("Scan_queue: final values" );
		Dump_line_list_sub(SORT_KEY,sort_order);
	}
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	if( perr ) *perr = error;
	if( pdone ) *pdone = done;
	DEBUG3("Scan_queue: final printable %d, held %d, move %d, error %d, done %d",
		printable, held, move, error, done );
	return(0);
}

/*
 * char *Get_fd_image( int fd, char *file )
 *  Get an image of a file from an fd
 */

char *Get_fd_image( int fd, off_t maxsize )
{
	char *s = 0;
	struct stat statb;
	char buffer[LARGEBUFFER];
	int n;
	off_t len;

	DEBUG3("Get_fd_image: fd %d", fd );

	if( lseek(fd, 0, SEEK_SET) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Get_fd_image: lseek failed" );
	}
	if( maxsize && fstat(fd, &statb) == 0
		&& maxsize< statb.st_size/1024 ){
		lseek(fd, -maxsize*1024, SEEK_END);
	}
	
	len = 0;
	while( (n = ok_read(fd,buffer,sizeof(buffer))) > 0 ){
		s = realloc_or_die(s,len+n+1,__FILE__,__LINE__);
		memcpy(s+len,buffer,n);
		len += n;
		s[len] = 0;
	}
	if(DEBUGL3){
		plp_snprintf(buffer,32, "%s",s);
		logDebug("Get_fd_image: len %d '%s'", s?safestrlen(s):0, buffer );
	}
	return(s);
}

/*
 * char *Get_file_image( char *dir, char *file )
 *  Get an image of a file
 */

char *Get_file_image( const char *file, off_t maxsize )
{
	char *s = 0;
	struct stat statb;
	int fd;

	if( file == 0 ) return(0);
	DEBUG3("Get_file_image: '%s', maxsize %ld", file, (long)maxsize );
	if( (fd = Checkread( file, &statb )) >= 0 ){
		s = Get_fd_image( fd, maxsize );
		close(fd);
	}
	return(s);
}

/*
 * char *Get_fd_image_and_split
 *  Get an image of a file
 */

int Get_fd_image_and_split( int fd,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image )
{
	char *s = 0;
	if( return_image ) *return_image = 0;
	s = Get_fd_image( fd, maxsize );
	if( s == 0 ) return 1;
	if( clean ) Clean_meta(s);
	Split( l, s, sep, sort, keysep, uniq, trim, nocomments ,0);
	if( return_image ){
		*return_image = s;
	} else {
		if( s ) free(s); s = 0;
	}
	return(0);
}


/*
 * char *Get_file_image_and_split
 *  Get an image of a file
 */

int Get_file_image_and_split( const char *file,
	int maxsize, int clean,
	struct line_list *l, const char *sep,
	int sort, const char *keysep, int uniq, int trim, int nocomments,
	char **return_image )
{
	char *s = 0;
	if( return_image ) *return_image = 0;
	if( !ISNULL(file) ) s = Get_file_image( file, maxsize );
	if( s == 0 ) return 1;
	if( clean ) Clean_meta(s);
	Split( l, s, sep, sort, keysep, uniq, trim, nocomments ,0);
	if( return_image ){
		*return_image = s;
	} else {
		if( s ) free(s); s = 0;
	}
	return(0);
}

/*
 * Set up a job data structure with information from the
 *   file images
 *  - Check_for_hold -
 *     checks to see if the job is held by class or by
 *     command
 *  Setup_job - gets the job information and updates it
 *     from the spool queue and control information
 */


void Check_for_hold( struct job *job, struct line_list *spool_control )
{
	int held, i;
	/* get the hold class of the job */
	held = Get_hold_class(&job->info,spool_control);
	Set_flag_value(&job->info,HOLD_CLASS,held);

	/* see if we need to hold this job by time */
	if( !Find_exists_value(&job->info,HOLD_TIME,Hash_value_sep) ){
		if( Find_flag_value(spool_control,HOLD_TIME) ){
			i = time((void *)0);
		} else {
			i = 0;
		}
		Set_flag_value( &job->info, HOLD_TIME, i );
	}
	held = Find_flag_value(&job->info,HOLD_TIME);
	Set_flag_value(&job->info,HELD,held);
}

/* Get_hold_class( spool_control, job )
 *  check to see if the spool class and the job class are compatible
 *  returns:  non-zero if held, 0 if not held
 *   i.e.- cmpare(spoolclass,jobclass)
 */

int Get_hold_class( struct line_list *info, struct line_list *sq )
{
	int held, i;
	char *s, *t;
	struct line_list l;

	Init_line_list(&l);
	held = 0;
	if( (s = Clsses(sq))
		&& (t = Find_str_value(info,CLASS)) ){
		held = 1;
		Free_line_list(&l);
		Split(&l,s,File_sep,0,0,0,0,0,0);
		for( i = 0; held && i < l.count; ++i ){
			held = Globmatch( l.list[i], t );
		}
		Free_line_list(&l);
	}
	return(held);
}

/*
 * Extract the control file and data file information from the
 * control file image
 *
 * Note: we also handle HP extensions here.
 * 
 *  1. 4-Digit Job Numbers
 *  ----------------------
 *        
 *  HP preserves the System V-style 4-digit sequence number, or job number, in
 *  file names and attributes, while BSD uses 3-digit job numbers.
 * 
 *  
 *  2. Control and Data File Naming Conventions
 *  -------------------------------------------
 *     
 *  Control files are named in the following format:
 *        
 *     cA<seqn><host>
 *        
 *     <seqn> is the 4-digit sequence number (aka job number).
 *     <host> is the originating host name.
 * 
 *  The data file naming sequence format is:
 * 
 *      dA<seqn><host>   through   dZ<seqn><host>     followed by...
 *      da<seqn><host>   through   dz<seqn><host>     followed by...
 *      eA<seqn><host>   through   eZ<seqn><host>     followed by...
 *      ea<seqn><host>   through   ez<seqn><host>     ... etc. ...
 * 
 * 
 *  So the first data file name in a request begins with "dA", the second with
 *  "dB", the 27th with "da", the 28th with "db", and so forth.
 * 
 *
 * 3. HP-Specific BSD Job Attributes (Control File Lines)
 *  ------------------------------------------------------
 * 
 *  The following control file lines are extensions of RFC-1179:
 * 
 *     R<login>
 * 
 *        Write to the named login's terminal when the job is complete.  This is
 *        an alternate to the RFC-1179-style e-mail completion notification.
 *        This notification is selected via the lp "-w" option.
 * 
 *     A<priority>
 * 
 *        Specifies the System V-style priority of the request, a single digit
 *        from 0-7.
 * 
 *     N B<banner>
 * 
 *        Note that this line begins with an "N", a space, and then a "B".  The
 *        argument is the banner page title requested via the lp "-t" option.  If
 *        that option was not given then the argument is null.
 * 
 *     N O<options>
 * 
 *        Note that this line begins with an "N", a space, and then an "O".  The
 *        argument contains the System V-style "-o" options specified in the lp
 *        command line.  The option names appear without a leading "-o".  The
 *        first option name begins in the fourth character of the line; each
 *        option is separated by a blank.  If no "-o" options were given then the
 *        argument is null.
 *
 */

void Append_Z_value( struct job *job, char *s )
{
	char *t;

	/* check for empty string */
	if( !s || !*s ) return;
	t = Find_str_value(&job->info,"Z");
	if( t && *t ){
		t = safestrdup3(t,",",s,__FILE__,__LINE__);
		Set_str_value(&job->info,"Z",t);
		if( t ) free(t); t = 0;
	} else {
		Set_str_value(&job->info,"Z",s);
	}
}

int Set_job_ticket_from_cf_info( struct job *job, char *cf_file_image, int read_cf_file )
{
	char *s;
	int i, c, n, copies = 0, last_format = 0;
	struct line_list cf_line_list;
	struct line_list *datafile = 0;
	char buffer[SMALLBUFFER], *t;
	const char *file_found, *priority;
	char *names = 0;
	int returnstatus = 0;
	int hpformat;

	Init_line_list(&cf_line_list);
	names = 0;

	hpformat = Find_flag_value(&job->info,HPFORMAT);
	if( read_cf_file ){
		s = Find_str_value(&job->info,OPENNAME);
		DEBUG3("Set_job_ticket_from_cf_info: control file '%s', hpformat '%d'",
			s, hpformat );
		if( s &&  Get_file_image_and_split(s,0,0, &cf_line_list, Line_ends,0,0,0,0,0,0) ){
				DEBUG3("Set_job_ticket_from_cf_info: missing or empty control file '%s'", s );
				plp_snprintf(buffer,sizeof(buffer),
					"no control file %s - %s", s, Errormsg(errno) );
				Set_str_value(&job->info,ERROR,buffer);
				Set_nz_flag_value(&job->info,ERROR_TIME,time(0));
				returnstatus = 1;
				goto done;
		}
	}
	if( cf_file_image ){
		Split( &cf_line_list, cf_file_image, Line_ends, 0, 0, 0, 0, 0 ,0);
	}

	Free_listof_line_list( &job->datafiles );

	file_found = 0;
	datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);

	memset(datafile,0,sizeof(datafile[0]));
	for( i = 0; i < cf_line_list.count; ++i ){
		s = cf_line_list.list[i];
		Clean_meta(s);
		c = cval(s);
		DEBUG3("Set_job_ticket_from_cf_info: doing line '%s'", s );
		if( islower(c) ){
			t = s;
			while( (t = strpbrk(t," \t")) ) *t++ = '_';
			if( file_found && (safestrcmp(file_found,s+1) || last_format != c) ){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}

			last_format = c;
			buffer[0] = c; buffer[1] = 0;
			Set_str_value(datafile,FORMAT,buffer);

			++copies;
			Set_flag_value(datafile,COPIES,copies);

			Set_str_value(datafile,OTRANSFERNAME,s+1);
			file_found = Find_str_value(datafile,OTRANSFERNAME);
			DEBUG4("Set_job_ticket_from_cf_info: doing file '%s', format '%c', copies %d",
				file_found, last_format, copies );
		} else if( c == 'N' ){
			if( hpformat && cval(s+1) == ' '){
				/* this is an HP Format option */
				/* N B<banner> -> 'T' line */
				/* N Ooption option option-> prefix to Z */
				c = cval(s+2);
				if( c == 'B' ){
					if( s[3] ) Set_str_value(&job->info,"T",s+3);
				} else if( c == 'O' ){
					s = s+3;
					if( safestrlen(s) ){
						for( t = s; (t = strpbrk(t," ")); ++t ){
							*t = ',';
						}
						Append_Z_value(job,s);
					}
				}
				continue;
			}
			/* if we have a file name AND an 'N' for it, then set up a new file */
			if( file_found && (t = Find_str_value(datafile,"N"))
				/* && safestrcmp(t,s+1) */ ){
				Check_max(&job->datafiles,1);
				job->datafiles.list[job->datafiles.count++] = (void *)datafile;
				copies = 0;
				file_found = 0;
				datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
				memset(datafile,0,sizeof(datafile[0]));
			}
			Set_str_value(datafile,"N",s+1);
			if( !names ){
				names = safestrdup(s+1,__FILE__,__LINE__);
			} else {
				names =  safeextend3(names,",",s+1,__FILE__,__LINE__);
			}
		} else if( c == 'U' ){
			/* Set_str_value(datafile,"U",s+1)*/ ;
		} else {
			/*
			 * HP UX has some  VERY odd problems
			 */
			if( hpformat && c == 'Z' ){
				/* Multiple Z lines */
				Append_Z_value( job, s+1 );
			} else if( hpformat && c == 'R' ){
				/* Uses R and not M */
				Set_str_value(&job->info,"M",s+1);
			} else if( hpformat && c == 'A' ){
				/* Uses Annn for priority */
				n = strtol( s+1,0,10);
				if( n >= 0 && n <=10){
					c = n + 'A';
					buffer[0] = n + 'A';
					buffer[1] = 0;
					Set_str_value(&job->info,PRIORITY,buffer);
				}
			} else if( isupper(c) ){
				buffer[0] = c; buffer[1] = 0;
				DEBUG4("Set_job_ticket_from_cf_info: control '%s'='%s'", buffer, s+1 );
				Set_str_value(&job->info,buffer,s+1);
			}
		}
	}
	if( file_found ){
		Check_max(&job->datafiles,1);
		job->datafiles.list[job->datafiles.count++] = (void *)datafile;
		datafile = 0;
	}

	Set_str_value(&job->info,FILENAMES,names);

	/*
	 * now fix up priority using
     *  ignore requested user priority
     *    ignore_requested_user_priority=0
	 *  do not set priority from class name
     *     break_classname_priority_link=0
	 */

	priority = 0;
	if( !Ignore_requested_user_priority_DYN &&
		!Break_classname_priority_link_DYN ) priority = Find_str_value( &job->info,CLASS);
	if( ISNULL(priority) ) priority = Default_priority_DYN;
	if( ISNULL(priority) ) priority = "A";
	buffer[0] = toupper(cval(priority)); buffer[1] = 0;
	if( cval(buffer) < 'A' || cval(buffer) > 'Z' ){
		priority = "A";
	} else {
		priority = buffer;
	}

	Set_str_value(&job->info,PRIORITY,priority);
	priority = Find_str_value(&job->info,PRIORITY);

	if( !Find_str_value(&job->info,CLASS) ){
		Set_str_value(&job->info,CLASS,priority);
	}

 done:

	if( datafile ) Free_line_list( datafile );
	if( datafile ) free(datafile); datafile=0;
	if( names )	free(names); names=0;
	Free_line_list( &cf_line_list );
	if(DEBUGL4)Dump_job("Set_job_ticket_from_cf_info - final",job);
	return(returnstatus);
}

/*
 * Set the job ticket file datafile information in the HFDATAFILES line
 *  This is the information that is PERMANENT, i.e. - after the
 *  job has been transferred.
 *  We also generate a DATAFILES line which has the format
 *   permanent_name[=currentname]
 *  i.e. - we have a mapping between the file names in the
 *  HFDATAFILES line and the current file names they go by.
 */

void Set_job_ticket_datafile_info( struct job *job )
{
	int linecount, i, len;
	char *s, *t, *dataline, *datafiles;
	struct line_list *lp;
	struct line_list dups;

	datafiles = dataline = 0;

	Init_line_list(&dups);
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		lp = (void *)job->datafiles.list[linecount];
		if(DEBUGL4)Dump_line_list("Set_job_ticket_datafile_info - info", lp );
		for( i = 0; i < lp->count; ++i ){
			s = lp->list[i];

			if( !strncmp(s,"openname", 8 ) ) continue;
			if( !strncmp(s,"otransfername", 13 ) ) continue;
			dataline = safeextend3(dataline, s, "\002",__FILE__,__LINE__);
		}
		t = Find_str_value(lp,OPENNAME);
		s = Find_str_value(lp,DFTRANSFERNAME);
		if( !ISNULL(s) && !Find_flag_value(&dups,s) ){
			if( t ){
				datafiles = safeextend5(datafiles,s, "=", t, " ",__FILE__,__LINE__);
			} else {
				datafiles = safeextend3(datafiles, s, " ",__FILE__,__LINE__);
			}
			Set_flag_value(&dups,s,1);
		}
		len = strlen(dataline);
		if( len ){
			dataline[len-1] = '\001';
		}
	}
	Set_str_value(&job->info,HFDATAFILES,dataline);
	Set_str_value(&job->info,DATAFILES,datafiles);
	if( dataline ) free(dataline); dataline = 0;
	if( datafiles ) free(datafiles); datafiles = 0;
}


char *Make_job_ticket_image( struct job *job )
{
	char *outstr, *s;
	int i;
	int len = safestrlen(OPENNAME);

	outstr = 0;
	Set_job_ticket_datafile_info( job );
	for( i = 0; i < job->info.count; ++i ){
		s = job->info.list[i];
		if( !ISNULL(s) && !safestrpbrk(s,Line_ends) &&
			safestrncasecmp(OPENNAME,s,len) ){
			outstr = safeextend3(outstr,s,"\n",__FILE__,__LINE__);
		}
	}
	return( outstr );
}

/*
 * Write a job ticket file
 */

int Set_job_ticket_file( struct job *job, struct line_list *perm_check, int opened_fd )
{
	char *job_ticket_name, *outstr;
	int status;
	int fd = opened_fd;
	struct stat statb;

	status = 0;
	outstr = 0;

	Set_job_ticket_datafile_info( job );
	if(DEBUGL4)Dump_job("Set_job_ticket_file - init",job);
	Set_str_value(&job->info,UPDATE_TIME,Time_str(0,0));
	outstr = Make_job_ticket_image( job );
	DEBUG4("Set_job_ticket_file: '%s'", outstr );
	if( !(job_ticket_name = Find_str_value(&job->info,HF_NAME)) ){
		Errorcode = JABORT;
		fatal(LOG_ERR, "Set_job_ticket_file: LOGIC ERROR- no HF_NAME in job information - %s", outstr);
	}
	if( opened_fd <= 0 ){
		if( (fd = Checkwrite( job_ticket_name, &statb, O_RDWR, 0, 0 )) < 0 ){
			logerr(LOG_INFO, "Set_job_ticket_file: cannot open '%s'", job_ticket_name );
		} else if( Do_lock(fd, 1 ) ){
			logerr(LOG_INFO, "Set_job_ticket_file: cannot lcok '%s'", job_ticket_name );
			close(fd); fd = -1;
		}
	}
	if( fd > 0 ){
		if( lseek( fd, 0, SEEK_SET ) == -1 ){
			logerr_die(LOG_ERR, "Set_job_ticket_file: lseek failed" );
		}
		if( ftruncate( fd, 0 ) ){
			logerr_die(LOG_ERR, "Set_job_ticket_file: ftruncate failed" );
		}
		if( Write_fd_str(fd, outstr) < 0 ){
			logerr(LOG_INFO, "Set_job_ticket_file: write to '%s' failed", job_ticket_name );
			status = 1;
		}
		if( opened_fd <= 0 ){
			close(fd); fd = -1;
		}
	}

	if( Lpq_status_file_DYN ){
		unlink(Lpq_status_file_DYN );
	}

	/* we do this when we have a logger */
	if( status == 0 && Logger_fd > 0 ){
		char *t, *u;
		if( perm_check ){
			u = Join_line_list( perm_check, "\n" );
			t = Escape(u,1);
			outstr = safeextend5(outstr,"\n",LPC,"=",u,__FILE__,__LINE__);
			if(u) free(u); u = 0;
			if(t) free(t); t = 0;
		}
		send_to_logger(-1, -1, job,UPDATE,outstr);
	}
	if( outstr ) free( outstr ); outstr = 0;
	return( status );
}

/*
 * Get_job_ticket_file( struct job *job, char *job_ticket_name )
 *
 *  get job ticket file contents and initialize job->info hash with them
 */

void Get_job_ticket_file( int *lock_fd, struct job *job, char *job_ticket_name )
{
	char *s;
	struct stat statb;
	int fd = -1;
	if( (s = safestrchr(job_ticket_name, '=')) ){
		job_ticket_name = s+1;
	}
	DEBUG1("Get_job_ticket_file: checking on '%s'", job_ticket_name );
	if( lock_fd ) fd  = *lock_fd;
	if( fd <= 0 ){
		if( (fd = Checkwrite( job_ticket_name, &statb, O_RDWR, 0, 0 )) > 0
			&& !Do_lock(fd, 1 ) ){
			Get_fd_image_and_split( fd, 0, 0,
				&job->info, Line_ends, 1, Option_value_sep,1,1,1,0);
			if( lock_fd ){
				*lock_fd = fd;
				fd = -1;
			}
		}
		if( fd > 0 ) close(fd);
		fd = -1;
	} else {
		Get_fd_image_and_split( fd, 0, 0,
			&job->info, Line_ends, 1, Option_value_sep,1,1,1,0);
	}
	if( job->info.count ) {
		struct line_list cf_line_list, *datafile;
		int i;
		char *s;

		Init_line_list(&cf_line_list);

		if( (s = Find_str_value(&job->info,HFDATAFILES)) ){
			Split(&cf_line_list,s,"\001",0,0,0,0,0,0);
		}
		Free_listof_line_list( &job->datafiles );
		Check_max(&job->datafiles,cf_line_list.count);
		for( i = 0; i < cf_line_list.count; ++i ){
			s = cf_line_list.list[i];
			DEBUG3("Get_job_ticket_file: doing line '%s'", s );
			datafile = malloc_or_die(sizeof(datafile[0]),__FILE__,__LINE__);
			memset(datafile,0,sizeof(datafile[0]));
			job->datafiles.list[job->datafiles.count++] = (void *)datafile;
			Split(datafile,s,"\002",1,Option_value_sep,1,1,1,0);
		}
		Free_line_list( &cf_line_list );
	}
	if(DEBUGL2)Dump_job("Get_job_ticket_file",job);
}

/*
 * Get Spool Control Information
 *  - simply read the file
 */

void Get_spool_control( const char *file, struct line_list *info )
{
	Free_line_list(info);
	DEBUG2("Get_spool_control:  file '%s'", file );
	Get_file_image_and_split( file, 0, 0,
			info,Line_ends,1,Option_value_sep,1,1,1,0);
	if(DEBUGL4)Dump_line_list("Get_spool_control- info", info );
}

/*
 * Set Spool Control Information
 *  - simply write the file
 */

void Set_spool_control( struct line_list *perm_check, const char *file,
	struct line_list *info )
{
	char *s, *t, *tempfile;
	struct line_list l;
	int fd;

	s = t = tempfile = 0;
	Init_line_list(&l);
	fd = Make_temp_fd( &tempfile );
	DEBUG2("Set_spool_control: file '%s', tempfile '%s'",
		file, tempfile );
	if(DEBUGL4)Dump_line_list("Set_spool_control- info", info );
	s = Join_line_list(info,"\n");
	if( Write_fd_str(fd, s) < 0 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Set_spool_control: cannot write tempfile '%s'",
			tempfile );
	}
	close(fd);
	if( rename( tempfile, file ) == -1 ){
		Errorcode = JFAIL;
		logerr_die(LOG_INFO, "Set_spool_control: rename of '%s' to '%s' failed",
			tempfile, file );
	}
	/* force and update of the cached status */

	if( Lpq_status_file_DYN ){
		/* do not check to see if this works */
		unlink(Lpq_status_file_DYN);
	}

	if( Logger_fd ){
		/* log the spool control file changes */
		t = Escape(s,1);
		Set_str_value(&l,QUEUE,t);
		if(s) free(s); s = 0;
		if(t) free(t); t = 0;

		if( perm_check ){
			s = Join_line_list( perm_check, "\n" );
			t = Escape(s,1);
			Set_str_value(&l,LPC,t);
			if(s) free(s); s = 0;
			if(t) free(t); t = 0;
		}
		t = Join_line_list( &l, "\n");

		send_to_logger(-1,-1,0,QUEUE,t);
	}

	Free_line_list(&l);
	if(s) free(s); s = 0;
	if(t) free(t); t = 0;
}

void intval( const char *key, struct line_list *list, struct job *job )
{
	int i = Find_flag_value(list,key);
	int len = safestrlen(job->sort_key);
	plp_snprintf(job->sort_key+len,sizeof(job->sort_key)-len,
    "|%s.0x%08x",key,i&0xffffffff);
	DEBUG5("intval: '%s'", job->sort_key );
}

void revintval( const char *key, struct line_list *list, struct job *job )
{
	int i = Find_flag_value(list,key);
	int len = safestrlen(job->sort_key);
	plp_snprintf(job->sort_key+len,sizeof(job->sort_key)-len,
	"|%s.0x%08x",key,(~i)&0xffffffff);
	DEBUG5("revintval: '%s'", job->sort_key );
}

void strzval( const char *key, struct line_list *list, struct job *job )
{
	char *s = Find_str_value(list,key);
	int len = safestrlen(job->sort_key);
	plp_snprintf(job->sort_key+len,sizeof(job->sort_key)-len,
	"|%s.%d",key,s!=0);
	DEBUG5("strzval: '%s'", job->sort_key );
}

void strnzval( const char *key, struct line_list *list, struct job *job )
{
	char *s = Find_str_value(list,key);
	int len = safestrlen(job->sort_key);
	plp_snprintf(job->sort_key+len,sizeof(job->sort_key)-len,
	"|%s.%d",key,(s==0 || *s == 0));
	DEBUG5("strnzval: '%s'", job->sort_key );
}

void strval( const char *key, struct line_list *list, struct job *job,
	int reverse )
{
	char *s = Find_str_value(list,key);
	int len = safestrlen(job->sort_key);
	int c = 0;

	if(s) c = cval(s);
	if( reverse ) c = -c;
	c = 0xFF & (-c);
	plp_snprintf(job->sort_key+len,sizeof(job->sort_key)-len,
	"|%s.%02x",key,c);
	DEBUG5("strval: '%s'", job->sort_key );
}


/*
 * Make_sort_key
 *   Make a sort key from the image information
 */
void Make_sort_key( struct job *job )
{
	job->sort_key[0] = 0;
	if( Order_routine_DYN ){
#if defined(ORDER_ROUTINE)
		extern char *ORDER_ROUTINE( struct job *job );
		ORDER_ROUTINE( &job );
#else
		Errorcode = JABORT;
		fatal(LOG_ERR, "Make_sort_key: order_routine requested and ORDER_ROUTINE undefined");
#endif
	} else {
		/* first key is REMOVE_TIME - remove jobs come last */
		intval(REMOVE_TIME,&job->info,job);
#if 0
		/* first key is DONE_TIME - done jobs come last */
		intval(DONE_TIME,&job->info,job);
		intval(INCOMING_TIME,&job->info,job);
		/* next key is ERROR - error jobs jobs come before removed */
		intval(ERROR_TIME,&job->info,job);
#endif
		/* next key is HOLD - before the error jobs  */
		intval(HOLD_CLASS,&job->info,job);
		intval(HOLD_TIME,&job->info,job);
		/* next key is MOVE - before the held jobs  */
		strnzval(MOVE,&job->info,job);
		/* now by priority */
		if( Ignore_requested_user_priority_DYN == 0 ){
			strval(PRIORITY,&job->info,job,Reverse_priority_order_DYN);
		}
		/* now we do TOPQ */
		revintval(PRIORITY_TIME,&job->info,job);
		/* now we do FirstIn, FirstOut */
		intval(JOB_TIME,&job->info,job);
		intval(JOB_TIME_USEC,&job->info,job);
		/* now we do by job number if two at same time (very unlikely) */
		intval(NUMBER,&job->info,job);
	}
}

/*
 * Set up printer
 *  1. reset configuration information
 *  2. check the printer name
 *  3. get the printcap information
 *  4. set the configuration variables
 *  5. If run on the server,  then check for the Lp_device_DYN
 *     being set.  If it is set, then clear the RemotePrinter_DYN
 *     and RemoteHost_DYN.
 */

int Setup_printer( char *prname, char *error, int errlen, int subserver )
{
	char *s;
	int status = 0;
	char name[SMALLBUFFER];
	struct stat statb;

	DEBUG3( "Setup_printer: checking printer '%s'", prname );

	/* reset the configuration information, just in case
	 * this is a subserver or being used to get status
	 */
	safestrncpy(name,prname);
	if( error ) error[0] = 0;
	if( (s = Is_clean_name( name )) ){
		plp_snprintf( error, errlen,
			"printer '%s' has illegal char at '%s' in name", name, s );
		status = 1;
		goto error;
	}
	lowercase(name);
	if( !subserver && Status_fd > 0 ){
		close( Status_fd );
		Status_fd = -1;
	}
	Set_DYN(&Printer_DYN,name);
	Fix_Rm_Rp_info(0,0);

	if( Spool_dir_DYN == 0 || *Spool_dir_DYN == 0 || stat(Spool_dir_DYN, &statb) ){
		plp_snprintf( error, errlen,
"spool queue for '%s' does not exist on server %s\n"
"check for correct printer name or you may need to run\n"
"'checkpc -f' to create queue",
			name, FQDNHost_FQDN );
		status = 2;
		goto error;
	}

	if( chdir( Spool_dir_DYN ) < 0 ){
		plp_snprintf( error, errlen,
			"printer '%s', chdir to '%s' failed '%s'",
				name, Spool_dir_DYN, Errormsg( errno ) );
		status = 2;
		goto error;
	}

	/*
	 * get the override information from the control/spooling
	 * directory
	 */

	Get_spool_control( Queue_control_file_DYN, &Spool_control );

	if( Perm_filters_line_list.count ){
		Free_line_list(&Perm_line_list);
		Merge_line_list(&Perm_line_list,&RawPerm_line_list,0,0,0);
		Filterprintcap( &Perm_line_list, &Perm_filters_line_list,
			Printer_DYN );
	}

	DEBUG1("Setup_printer: printer now '%s', spool dir '%s'",
		Printer_DYN, Spool_dir_DYN );
	if(DEBUGL3){
		Dump_parms("Setup_printer - vars",Pc_var_list);
		Dump_line_list("Setup_printer - spool control", &Spool_control );
	}

 error:
	DEBUG3("Setup_printer: status '%d', error '%s'", status, error );
	return( status );
}

/**************************************************************************
 * Read_pid( int fd )
 *   - Read the pid from a file
 **************************************************************************/
pid_t Read_pid( int fd )
{
	char str[LINEBUFFER];
	long n;

	if( lseek( fd, 0, SEEK_SET ) == -1 ){
		logerr_die(LOG_ERR, "Read_pid: lseek failed" );
	}

	str[0] = 0;
	if( (n = read( fd, str, sizeof(str)-1 ) ) < 0 ){
		logerr_die(LOG_ERR, "Read_pid: read failed" );
	}
	str[n] = 0;
	n = atol( str );
	DEBUG3( "Read_pid: %ld", n );
	return( n );
}

pid_t Read_pid_from_file( const char *filename ) {
	struct stat statb;
	pid_t pid = -1;
	int fd;

	fd = Checkread( filename, &statb );
	if( fd >= 0 ) {
		pid = Read_pid( fd);
		close( fd );
	}
	return pid;
}

/**************************************************************************
 * Write_pid( int fd )
 *   - Write the pid to a file
 **************************************************************************/
int Write_pid( int fd, int pid, char *str )
{
	char line[LINEBUFFER];

	if( lseek( fd, 0, SEEK_SET ) == -1 ){
		logerr(LOG_ERR, "Write_pid: lseek failed" );
		return -1;
	}
	if( ftruncate( fd, 0 ) ){
		logerr(LOG_ERR, "Write_pid: ftruncate failed" );
		return -1;
	}

	if( str == 0 ){
		plp_snprintf( line, sizeof(line), "%d\n", pid );
	} else {
		plp_snprintf( line, sizeof(line), "%s\n", str );
	}
	DEBUG3( "Write_pid: pid %d, str '%s'", pid, str );
	if( Write_fd_str( fd, line ) < 0 ){
		logerr(LOG_ERR, "Write_pid: write failed" );
		return -1;
	}
	return 0;
}

/***************************************************************************
 * int Patselect( struct line_list *tokens, struct line_list *cf );
 *    check to see that the token value matches one of the following
 *    in the control file:
 *  token is INTEGER: then matches the job number
 *  token is string: then matches either the user name or host name
 *    then try glob matching job ID
 *  return:
 *   0 if match found
 *   nonzero if not match found
 ***************************************************************************/

int Patselect( struct line_list *token, struct line_list *cf, int starting )
{
	int match = 1;
	int i, n, val;
	char *key, *s, *end;
	
	if(DEBUGL3)Dump_line_list("Patselect- tokens", token );
	if(DEBUGL3)Dump_line_list("Patselect- info", cf );
	for( i = starting; match && i < token->count; ++i ){
		key = token->list[i];
		DEBUG3("Patselect: key '%s'", key );
		/* handle wildcard match */
		if( !(match = safestrcasecmp( key, "all" ))){
			break;
		}
		end = key;
		val = strtol( key, &end, 10 );
		if( *end == 0 ){
			n = Find_decimal_value(cf,NUMBER);
			/* we check job number */
			DEBUG3("Patselect: job number check '%d' to job %d",
				val, n );
			match = (val != n);
		} else {
			/* now we check to see if we have a name match */
			if( (s = Find_str_value(cf,LOGNAME))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
			if( (s = Find_str_value(cf,IDENTIFIER))
				&& !(match = Globmatch(key,s)) ){
				break;
			}
		}
	}
	DEBUG3("Patselect: returning %d", match);
	return(match);
}

/***************************************************************************
 * char * Check_format( int type, char *name, struct control_file *job )
 * Check to see that the file name has the correct format
 * name[0] == 'c' or 'd' (type)
 * name[1] = 'f'
 * name[2] = A-Za-z
 * name[3-5] = NNN
 * name[6-end] = only alphanumeric and ., _, or - chars
 * RETURNS: 0 if OK, error message (string) if not
 *
 * Summary of HP's Extensions to RFC-1179
 * 1. 4-Digit Job Numbers
 *  ----------------------
 *  HP preserves the System V-style 4-digit sequence number, or job number, in
 *  file names and attributes, while BSD uses 3-digit job numbers.
 *  2. Control and Data File Naming Conventions
 *  -------------------------------------------
 *  Control files are named in the following format:
 *     cA<seqn><host>
 *     <seqn> is the 4-digit sequence number (aka job number).
 *     <host> is the originating host name.
 *  The data file naming sequence format is:
 *      dA<seqn><host>   through   dZ<seqn><host>     followed by...
 *      da<seqn><host>   through   dz<seqn><host>     followed by...
 *      eA<seqn><host>   through   eZ<seqn><host>     followed by...
 *      ea<seqn><host>   through   ez<seqn><host>     ... etc. ...
 *  So the first data file name in a request begins with "dA", the second with
 *  "dB", the 27th with "da", the 28th with "db", and so forth.

 ***************************************************************************/
int Check_format( int type, const char *name, struct job *job )
{
	int n, c, hpformat;
	const char *s;
	char *t;
	char msg[SMALLBUFFER];

	DEBUG4("Check_format: type %d, name '%s'", type, name ); 
	msg[0] = 0;
	hpformat = 0;
	n = cval(name);
	switch( type ){
	case DATA_FILE:
		if( n != 'd' ){
			plp_snprintf(msg, sizeof(msg),
				"data file does not start with 'd' - '%s'",
				name );
			goto error;
		}
		break;
	case CONTROL_FILE:
		if( n != 'c' ){
			plp_snprintf(msg, sizeof(msg),
				"control file does not start with 'c' - '%s'",
				name );
			goto error;
		}
		break;
	default:
		plp_snprintf(msg, sizeof(msg),
			"bad file type '%c' - '%s' ", type,
			name );
		goto error;
	}
	/* check for second letter */
	n = cval(name+1);
	if( n == 'A' ){
		/* HP format */
		hpformat = 1;
	} else if( n != 'f' ){
		plp_snprintf(msg, sizeof(msg),
			"second letter must be f not '%c' - '%s' ", n, name );
		goto error;
	} else {
		n = cval(name+2);
		if( !isalpha( n ) ){
			plp_snprintf(msg, sizeof(msg),
				"third letter must be letter not '%c' - '%s' ", n, name );
			goto error;
		}
    }
	if( type == CONTROL_FILE ){
		plp_snprintf(msg,sizeof(msg), "%c",n);
		Set_str_value(&job->info,PRIORITY,msg);
		msg[0] = 0;
	}
	/*
		we now enter the wonderful world of 'conventions'
		cfAnnnHostname
              ^^^^ starts with letter (number len = 0, 1, 2, 3)
		cfAnnnIPV4.Add.ress  (number len = 4, 5, ... )
              ^^^^ starts with number or is a 3com type thing
		cfAnnnnnnHostName    (len = 6)
                 ^^^^ starts with letter
		cfAnnnnnnIPV4.Add.ress  (len = 7, ... )
                 ^^^^ starts with number
	*/
   
	if( hpformat ){
		/* we have four digits */
		safestrncpy(msg,&name[2]);
		t = 0;
		n = strtol(msg,&t,10);
	} else {
		safestrncpy(msg,&name[3]);
		for( t = msg; isdigit(cval(t)); ++t );
		c = t - msg;
		switch( c ){
		case 0: case 1: case 2: case 3:
			break;
		case 4: case 5:
			c = 3;
			break;
		default:
			if( cval(msg+6) == '.' ) c = 3;
			else c = 6;
			break;
		}
		/* get the number */
		t = &msg[c];
		c = *t;
		*t = 0;
		n = strtol(msg,0,10);
		*t = c;
    }

	DEBUG1("Check_format: name '%s', number %d, file '%s'", name, n, t ); 
	if( Find_str_value( &job->info,NUMBER) ){
		c = Find_decimal_value( &job->info,NUMBER);
		if( c != n ){
			plp_snprintf(msg, sizeof(msg),
				"job numbers differ '%s', old %d and new %d",
					name, c, n );
			goto error;
		}
	} else {
		Fix_job_number( job, n );
	}
	Clean_name(t);
	if( (s = Find_str_value( &job->info,FILE_HOSTNAME)) ){
		if( safestrcasecmp(s,t) ){
			plp_snprintf(msg, sizeof(msg),
				"bad hostname '%s' - '%s' ", t, name );
			goto error;
		}
	} else {
		Set_str_value(&job->info,FILE_HOSTNAME,t);
	}
	/* clear out error message */
	msg[0] = 0;

 error:
	if( hpformat ){
		Set_flag_value(&job->info,HPFORMAT,hpformat);
	}
	if( msg[0] ){
		DEBUG1("Check_format: %s", msg ); 
		Set_str_value(&job->info,FORMAT_ERROR,msg);
	}
	return( msg[0] != 0 );
}

char *Frwarding(struct line_list *l)
{
	return( Find_str_value(l,FORWARDING) );
}
int Pr_disabled(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_DISABLED) );
}
int Sp_disabled(struct line_list *l)
{
	return( Find_flag_value(l,SPOOLING_DISABLED) );
}
int Pr_aborted(struct line_list *l)
{
	return( Find_flag_value(l,PRINTING_ABORTED) );
}
int Hld_all(struct line_list *l)
{
	return( Find_flag_value(l,HOLD_ALL) );
}
char *Clsses(struct line_list *l)
{
	return( Find_str_value(l,CLASS) );
}
char *Cntrol_debug(struct line_list *l)
{
	return( Find_str_value(l,DEBUG) );
}
char *Srver_order(struct line_list *l)
{
	return( Find_str_value(l,SERVER_ORDER) );
}

/*
 * Job datastructure management
 */

void Init_job( struct job *job )
{
	memset(job,0,sizeof(job[0]) );
}

void Free_job( struct job *job )
{
	Free_line_list( &job->info );
	Free_listof_line_list( &job->datafiles );
	Free_line_list( &job->destination );
}

void Copy_job( struct job *dest, struct job *src )
{
	Merge_line_list( &dest->info, &src->info, 0,0,0 );
	Merge_listof_line_list( &dest->datafiles, &src->datafiles);
	Merge_line_list( &dest->destination, &src->destination, 0,0,0 );
}

/**************************************************************************
 * static int Fix_job_number();
 * - fixes the job number range and value
 **************************************************************************/

char *Fix_job_number( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	int len = 3, max = 1000;

	if( n == 0 ){
		n = Find_decimal_value( &job->info, NUMBER );
	}
	if( Long_number_DYN && !Backwards_compatible_DYN ){
		len = 6;
		max = 1000000;
	}
	plp_snprintf(buffer,sizeof(buffer), "%0*d",len, n % max );
	Set_str_value(&job->info,NUMBER,buffer);
	return( Find_str_value(&job->info,NUMBER) );
}

/************************************************************************
 * Make_identifier - add an identifier field to the job
 *  the identifier has the format name@host%id
 *  It is put in the 'A' field on the name.
 * 
 ************************************************************************/

char *Make_identifier( struct job *job )
{
	const char *user, *host;
	char *s, *id;
	char number[32];
	int n;

	if( !(s = Find_str_value( &job->info,IDENTIFIER )) ){
		if( !(user = Find_str_value( &job->info,"P" ))){
			user = "nobody";
		}
		if( !(host= Find_str_value( &job->info,"H" ))){
			host = "unknown";
		}
		n = Find_decimal_value( &job->info,NUMBER );
		plp_snprintf(number,sizeof(number), "%d",n);
		if( (s = safestrchr( host, '.' )) ) *s = 0;
		id = safestrdup5(user,"@",host,"+",number,__FILE__,__LINE__);
		if( s ) *s = '.';
		Set_str_value(&job->info,IDENTIFIER,id);
		if( id ) free(id); id = 0;
		s = Find_str_value(&job->info,IDENTIFIER);
	}
	return(s);
}

void Dump_job( const char *title, struct job *job )
{
	int i;
	struct line_list *lp;
	if( title ) LOGDEBUG( "*** Job %s *** - 0x%lx", title, Cast_ptr_to_long(job));
	Dump_line_list_sub( "info",&job->info);
	LOGDEBUG("  datafiles - count %d", job->datafiles.count );
	for( i = 0; i < job->datafiles.count; ++i ){
		char buffer[SMALLBUFFER];
		plp_snprintf(buffer,sizeof(buffer), "  datafile[%d]", i );
		lp = (void *)job->datafiles.list[i];
		Dump_line_list_sub(buffer,lp);
	}
	Dump_line_list_sub( "destination",&job->destination);
	if( title ) LOGDEBUG( "*** end ***" );
}


void Job_printable( struct job *job, struct line_list *spool_control,
	int *pprintable, int *pheld, int *pmove, int *perr, int *pdone )
{
	char *s;
	char buffer[SMALLBUFFER];
	char destbuffer[SMALLBUFFER];
	struct stat statb; 
	int n, printable = 0, held = 0, move = 0, error = 0, done = 0,destination, destinations;

	if(DEBUGL4)Dump_job("Job_printable - job info",job);
	if(DEBUGL4)Dump_line_list("Job_printable - spool control",spool_control);

	buffer[0] = 0;
	if( job->info.count == 0 ){
		plp_snprintf(buffer,sizeof(buffer), "removed" );
	} else if( Find_flag_value(&job->info,INCOMING_TIME) ){
		int pid = Find_flag_value(&job->info,INCOMING_PID);
		if( !pid || kill( pid, 0 ) ){
			plp_snprintf(buffer,sizeof(buffer), "incoming (orphan)" );
		} else {
			plp_snprintf(buffer,sizeof(buffer), "incoming" );
		}
	} else if( (error = Find_flag_value(&job->info,ERROR_TIME)) ){
		plp_snprintf(buffer,sizeof(buffer), "error" );
	} else if( Find_flag_value(&job->info,HOLD_TIME) ){
		plp_snprintf(buffer,sizeof(buffer), "hold" );
		held = 1;
	} else if( (done = Find_flag_value(&job->info,DONE_TIME)) ){
		plp_snprintf(buffer,sizeof(buffer), "done" );
	} else if( (n = Find_flag_value(&job->info,SERVER))
		&& kill( n, 0 ) == 0 ){
		int delta;
		time_t now = time((void *)0);
		time_t last_change = Find_flag_value(&job->info,START_TIME);
		if( !ISNULL(Status_file_DYN) && !stat( Status_file_DYN, &statb )
			&& last_change < statb.st_mtime ){
			last_change = statb.st_mtime;
		}
		if( !ISNULL(Log_file_DYN) && !stat( Log_file_DYN, &statb )
			&& last_change < statb.st_mtime ){
			last_change = statb.st_mtime;
		}
		delta = now - last_change;
		if( Stalled_time_DYN && delta > Stalled_time_DYN ){
			plp_snprintf( buffer, sizeof(buffer),
				"stalled(%dsec)", delta );
		} else {
			n = Find_flag_value(&job->info,ATTEMPT);
			plp_snprintf(buffer,sizeof(buffer), "active" );
			if( n > 0 ){
				plp_snprintf( buffer, sizeof(buffer),
					"active(attempt-%d)", n+1 );
			}
		}
		printable = 1;
	} else if((s = Find_str_value(&job->info,MOVE)) ){
		plp_snprintf(buffer,sizeof(buffer), "moved->%s", s );
		move = 1;
	} else if( Get_hold_class(&job->info, spool_control ) ){
		plp_snprintf(buffer,sizeof(buffer), "holdclass" );
		held = 1;
	} else {
		printable = 1;
	}
	if( (destinations = Find_flag_value(&job->info,DESTINATIONS)) ){
		printable = 0;
		for( destination = 0; destination < destinations; ++destination ){
			Get_destination(job,destination);
			if(DEBUGL4)Dump_job("Job_destination_printable - job",job);
			destbuffer[0] = 0;
			if( Find_flag_value(&job->destination,ERROR_TIME) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "error" );
			} else if( Find_flag_value(&job->destination,HOLD_TIME) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "hold" );
				held += 1;
			} else if( Find_flag_value(&job->destination,DONE_TIME) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "done" );
			} else if( (n = Find_flag_value(&job->destination,SERVER))
				&& kill( n, 0 ) == 0 ){
				int delta;
				n = Find_flag_value(&job->destination,START_TIME);
				delta = time((void *)0) - n;
				if( Stalled_time_DYN && delta > Stalled_time_DYN ){
					plp_snprintf( destbuffer, sizeof(destbuffer),
						"stalled(%dsec)", delta );
				} else {
					n = Find_flag_value(&job->destination,ATTEMPT);
					plp_snprintf(destbuffer,sizeof(destbuffer), "active" );
					if( n > 0 ){
						plp_snprintf( destbuffer, sizeof(destbuffer),
							"active(attempt-%d)", n+1 );
					}
				}
				printable += 1;
			} else if((s = Find_str_value(&job->destination,MOVE)) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "moved->%s", s );
				move += 1;
			} else if( Get_hold_class(&job->destination, spool_control ) ){
				plp_snprintf(destbuffer,sizeof(destbuffer), "holdclass" );
				held += 1;
			} else {
				printable += 1;
			}
			Set_str_value(&job->destination,PRSTATUS,destbuffer);
			Set_flag_value(&job->destination,PRINTABLE,printable);
			Set_flag_value(&job->destination,HELD,held);
			Update_destination(job);
		}
	}

	Set_str_value(&job->info,PRSTATUS,buffer);
	Set_flag_value(&job->info,PRINTABLE,printable);
	Set_flag_value(&job->info,HELD,held);
	if( pprintable ) *pprintable = printable;
	if( pheld ) *pheld = held;
	if( pmove ) *pmove = move;
	if( perr ) *perr = error;
	if( pdone ) *pdone = done;
	DEBUG3("Job_printable: printable %d, held %d, move '%d', error '%d', done '%d', status '%s'",
		printable, held, move, error, done, buffer );
}

int Server_active( char *file )
{
	struct stat statb;
	int serverpid = 0;
	int fd = Checkread( file, &statb );
	if( fd >= 0 ){
		serverpid = Read_pid( fd );
		close(fd);
		DEBUG5("Server_active: checking file %s, serverpid %d", file, serverpid );
		if( serverpid > 0 && kill(serverpid,0) ){
			serverpid = 0;
		}
	}
	DEBUG3("Server_active: file %s, serverpid %d", file, serverpid );
	return( serverpid );
}

/*
 * Destination Information
 *   The destination information is stored in the control file
 * as lines of the form:
 * NNN=.....   where NNN is the destination number
 *                   .... is the escaped destination information
 * During normal printing or other activity,  the destination information
 * is unpacked into the job->destination line list.
 */

/*
 * Update_destination updates the information with the values in the
 *  job->destination line list.
 */
void Update_destination( struct job *job )
{
	char *s, *t, buffer[SMALLBUFFER];
	int id;
	id = Find_flag_value(&job->destination,DESTINATION);
	plp_snprintf(buffer,sizeof(buffer), "DEST%d",id);
	s = Join_line_list(&job->destination,"\n");
	t = Escape(s,1);
	Set_str_value(&job->info,buffer,t);
	free(s);
	free(t);
	if(DEBUGL4)Dump_job("Update_destination",job);
}

/*
 * Get_destination puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination( struct job *job, int n )
{
	char buffer[SMALLBUFFER];
	char *s;
	int result = 1;

	plp_snprintf(buffer,sizeof(buffer), "DEST%d", n );

	Free_line_list(&job->destination);
	if( (s = Find_str_value(&job->info,buffer)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Option_value_sep,1,1,1,0);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Get_destination_by_name puts the requested information into the
 *  job->destination structure if it is available.
 *  returns: 1 if not found, 0 if found;
 */

int Get_destination_by_name( struct job *job, char *name )
{
	int result = 1;
	char *s;

	Free_line_list(&job->destination);
	if( name && (s = Find_str_value(&job->info,name)) ){
		s = safestrdup(s,__FILE__,__LINE__);
		Unescape(s);
		Split(&job->destination,s,Line_ends,1,Option_value_sep,1,1,1,0);
		if(s) free( s ); s = 0;
		result = 0;
	}
	return( result );
}

/*
 * Trim_status_file - trim a status file to an acceptible length
 */

int Trim_status_file( int status_fd, char *file, int max, int min )
{
	int tempfd, status;
	char buffer[LARGEBUFFER];
	struct stat statb;
	char *tempfile, *s;
	int count;

	tempfd = status = -1;

	DEBUG1("Trim_status_file: file '%s' max %d, min %d", file, max, min);

	/* if the file does not exist, do not create it */
	if( ISNULL(file) ) return( status_fd );
	if( stat( file, &statb ) == 0 ){
		DEBUG1("Trim_status_file: '%s' max %d, min %d, size %ld", file, max, min, 
			(long)(statb.st_size) );
		if( max > 0 && statb.st_size/1024 > max ){
			status = Checkwrite( file, &statb,O_RDWR,0,0);
			tempfd = Make_temp_fd(&tempfile);
			if( min > max || min == 0 ){
				min = max/4;
			}
			if( min == 0 ) min = 1;
			DEBUG1("Trim_status_file: trimming to %d K", min);
			lseek( status, 0, SEEK_SET );
			lseek( status, -min*1024, SEEK_END );
			while( (count = ok_read( status, buffer, sizeof(buffer) - 1 ) ) > 0 ){
				buffer[count] = 0;
				if( (s = safestrchr(buffer,'\n')) ){
					*s++ = 0;
					Write_fd_str( tempfd, s );
					break;
				}
			}
			while( (count = ok_read( status, buffer, sizeof(buffer) ) ) > 0 ){
				if( write( tempfd, buffer, count) < 0 ){
					Errorcode = JABORT;
					logerr_die(LOG_ERR, "Trim_status_file: cannot write tempfile" );
				}
			}
			lseek( tempfd, 0, SEEK_SET );
			lseek( status, 0, SEEK_SET );
			ftruncate( status, 0 );
			while( (count = ok_read( tempfd, buffer, sizeof(buffer) ) ) > 0 ){
				if( write( status, buffer, count) < 0 ){
					Errorcode = JABORT;
					logerr_die(LOG_ERR, "Trim_status_file: cannot write tempfile" );
				}
			}
			unlink(tempfile);
			close(status);
		}
		close( tempfd );
		if( status_fd > 0 ) close( status_fd );
		status_fd = Checkwrite( file, &statb,0,0,0);
	}
	return( status_fd );
}

/********************************************************************
 * BSD and LPRng order
 * We use these values to determine the order of jobs in the file
 * The order of the characters determines the order of the options
 *  in the control file.  A * puts all unspecified options there
 ********************************************************************/

 static char BSD_order[] = "HPJCLIMWT1234" ;
 static char LPRng_order[] = "HPJCLIMWT1234*" ;

char *Fix_datafile_infox( struct job *job, const char *number, const char *suffix,
	const char *xlate_format, int update_df_names )
{
	int i, copies, linecount, count, jobcopies, copy, group, offset;
	char *s, *Nline, *transfername, *dataline;
	struct line_list *lp, outfiles;
	char prefix[8];
	char fmt[2];
	
	Init_line_list(&outfiles);
	transfername = dataline = Nline = 0;
	if(DEBUGL4)Dump_job("Fix_datafile_info - starting", job );

	/* now we find the number of different data files */

	count = 0;
	/* we look through the data file list, looking for jobs with the name
	 * OTRANSFERNAME.  If we find a new one, we create the correct form
	 * of the job datafile
	 */
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		char *openname;
		lp = (void *)job->datafiles.list[linecount];
		transfername = Find_str_value(lp,OTRANSFERNAME);
		if( ! transfername ) transfername = Find_str_value(lp,DFTRANSFERNAME);
		Set_str_value(lp,NTRANSFERNAME,transfername);
		openname = Find_str_value(lp,OPENNAME);
		if( ISNULL(openname) ) Set_str_value(lp,OPENNAME,transfername);
		if( !(s = Find_casekey_str_value(&outfiles,transfername,Hash_value_sep)) ){
			/* we add the entry */
			offset = count % 52;
			group = count / 52;
			++count;
			if( (group >= 52) ){
				fatal(LOG_INFO, "Fix_datafile_info: too many data files");
			}
			plp_snprintf(prefix,sizeof(prefix), "d%c%c",
			("fghijklmnopqrstuvwxyzabcde" "ABCDEFGHIJKLMNOPQRSTUVWXYZ" )[group],
			("ABCDEFGHIJKLMNOPQRSTUVWXYZ" "abcdefghijklmnopqrstuvwxyz")[offset] );
			s = safestrdup3(prefix,number,suffix,__FILE__,__LINE__);
			if( transfername ) Set_casekey_str_value(&outfiles,transfername,s);
			Set_str_value(lp,NTRANSFERNAME,s);
			if(s) free(s); s = 0;
		} else {
			Set_str_value(lp,NTRANSFERNAME,s);
		}
	}
	Free_line_list(&outfiles);
	Set_decimal_value(&job->info,DATAFILE_COUNT,count);

	if(DEBUGL4)Dump_job("Fix_datafile_info - after finding duplicates", job );

	jobcopies = Find_flag_value(&job->info,COPIES);
	if( !jobcopies ) jobcopies = 1;
	fmt[0] = 'f'; fmt[1] = 0;
	DEBUG4("Fix_datafile_info: jobcopies %d", jobcopies );
	for(copy = 0; copy < jobcopies; ++copy ){
		for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
			lp = (void *)job->datafiles.list[linecount];
			if(DEBUGL5)Dump_line_list("Fix_datafile_info - info", lp  );
			transfername = Find_str_value(lp,NTRANSFERNAME);
			Nline = Find_str_value(lp,"N");
			fmt[0] = 'f';
			if( (s = Find_str_value(lp,FORMAT)) ){
				fmt[0] = *s;
			}
			if( xlate_format ){
				int l = safestrlen(xlate_format);
				for( i = 0; i+1 < l; i+= 2 ){
					if( (xlate_format[i] == fmt[0])
						|| (xlate_format[i] == '*') ){
						fmt[0] = xlate_format[i+1];
						break;
					}
				}
			}
			copies = Find_flag_value(lp,COPIES);
			if( copies == 0 ) copies = 1;
			for(i = 0; i < copies; ++i ){
				if( Nline && !Nline_after_file_DYN ){
					dataline = safeextend4(dataline,"N",Nline,"\n",__FILE__,__LINE__);
				}
				dataline = safeextend4(dataline,fmt,transfername,"\n",__FILE__,__LINE__);
				if( Nline && Nline_after_file_DYN ){
					dataline = safeextend4(dataline,"N",Nline,"\n",__FILE__,__LINE__);
				}
			}
			DEBUG4("Fix_datafile_info: file [%d], dataline '%s'",
				linecount, dataline);
		}
	}
	DEBUG4("Fix_datafile_info: adding remove lines" );
	for( linecount = 0; linecount < job->datafiles.count; ++linecount ){
		lp = (void *)job->datafiles.list[linecount];
		if(DEBUGL4)Dump_line_list("Fix_datafile_info - info", lp );
		transfername = Find_str_value(lp,NTRANSFERNAME);
		if( update_df_names ){
			transfername = Find_str_value(lp,NTRANSFERNAME);
			Set_str_value(lp,DFTRANSFERNAME,transfername);
			Set_str_value(lp,OTRANSFERNAME,0);
		}
		if( !Find_casekey_str_value(&outfiles,transfername,Hash_value_sep) ){
			dataline = safeextend4(dataline,"U",transfername,"\n",__FILE__,__LINE__);
			Set_casekey_str_value(&outfiles,transfername,"YES");
		}
		DEBUG4("Fix_datafile_info: file [%d], dataline '%s'",
			linecount, dataline);
		Set_str_value(lp,NTRANSFERNAME,0);
	}
	Free_line_list(&outfiles);
	return(dataline);
}

int ordercomp(  const void *left, const void *right, const void *orderp)
{
	const char *lpos, *rpos, *wildcard, *order;
	int cmp;

	order = (const char *)orderp;

	/* blank lines always come last */
	if( (wildcard = safestrchr( order, '*' )) ){
		wildcard = order + safestrlen(order);
	}
	lpos = *((const char **)left);
	if( lpos == 0 || *lpos == 0 ){
		lpos = order+safestrlen(order);
	} else if( !(lpos = safestrchr( order, *lpos )) ){
		lpos = wildcard;
	}
	rpos = *((const char **)right);
	if( rpos == 0 || *rpos == 0 ){
		rpos = order+safestrlen(order);
	} else if( !(rpos = safestrchr( order, *rpos )) ){
		rpos = wildcard;
	}
	cmp = lpos - rpos;
	DEBUG4("ordercomp '%s' to '%s' -> %d",
		*((const char **)left), *((const char **)right), cmp );
	return( cmp );
}
 struct maxlen{
	int c, len;
 } maxclen[] = {
	{ 'A', 131 }, { 'C', 31 }, { 'D', 1024 }, { 'H', 31 }, { 'I', 31 },
	{ 'J', 99 }, { 'L', 31 }, { 'N', 131 }, { 'M', 131 }, { 'N', 131 },
	{ 'P', 31 }, { 'Q', 131 }, { 'R', 131 }, { 'S', 131 }, { 'T', 79 },
	{ 'U', 131 }, { 'W', 31 }, { 'Z', 1024 }, { '1', 131 }, { '2', 131 },
	{ '3', 131 }, { '4', 131 },
	{0,0}
	};


/********************************************************************
 * int Fix_control( struct job *job, char *filexter,
 * char *xlate_format, char *update_df_names )
 *   fix the order of lines in the control file so that they
 *   are in the order of the letters in the order string.
 * Lines are checked for metacharacters and other trashy stuff
 *   that might have crept in by user efforts
 *
 * job - control file area in memory
 * order - order of options
 *
 *  order string: Letter - relative position in file
 *                * matches any character not in string
 *                  can have only one wildcard in string
 *   Berkeley-            HPJCLIMWT1234
 *   PLP-                 HPJCLIMWT1234*
 *
 * RETURNS: 0 if fixed correctly
 *          non-zero if there is something wrong with this file and it should
 *          be rejected out of hand
 *
 *  Fix up the control file,  setting the various entries
 *  to be compatible with transfer to the remote location
 * 1. info will have fromhost, priority, and number information
 *   if not, you will need to add it.
 *
 ************************************************************************/

void Fix_control( struct job *job, char *filter, char *xlate_format,
	int update_df_names )
{
	char *s, *t;
	const char *file_hostname, *number, *priority, *order;
	char buffer[SMALLBUFFER], pr[2];
	int tempfd, tempfc;
	int i, n, j, cccc, wildcard, len;
	struct line_list controlfile;

	Init_line_list(&controlfile);

	if(DEBUGL3) Dump_job( "Fix_control: starting", job );

	/* we set the control file with the single letter values in the
	   job ticket file
	 */
	for( i = 0; i < job->info.count; ++i ){
		int c;
		s = job->info.list[i];
		if( s && (c = s[0]) && isupper(c) && c != 'N' && c != 'U'
			&& s[1] == '=' ){
			/* remove banner from control file */
			if( c == 'L' && Suppress_header_DYN && !Always_banner_DYN ) continue;
			s[1] = 0;
			Set_str_value(&controlfile,s,s+2);
			s[1] = '=';
		}
	}

	if(DEBUGL3) Dump_line_list( "Fix_control: control file", &controlfile );

	n = j = 0;
	n = Find_decimal_value( &job->info,NUMBER);
	j = Find_decimal_value( &job->info,SEQUENCE);

	number = Fix_job_number(job, n+j);
	
	if( !(priority = Find_str_value( &job->destination,PRIORITY))
		&& !(priority = Find_str_value( &job->info,PRIORITY))
		&& !(priority = Default_priority_DYN) ){
		priority = "A";
	}
	pr[0] = *priority;
	pr[1] = 0;

	file_hostname = Find_str_value(&job->info,FILE_HOSTNAME);
	if( !file_hostname ){
		file_hostname = Find_str_value(&job->info,FROMHOST);
		if( file_hostname == 0 || *file_hostname == 0 ){
			file_hostname = FQDNHost_FQDN;
		}
		Set_str_value(&job->info,FILE_HOSTNAME,file_hostname);
		file_hostname = Find_str_value(&job->info,FILE_HOSTNAME);
	}

	t = 0;
	if( (Backwards_compatible_DYN || Use_shorthost_DYN)
		&& (t = safestrchr( file_hostname, '.' )) ){
		*t = 0;
	}

	/* fix control file name */
	s = safestrdup4("cf",pr,number,file_hostname,__FILE__,__LINE__);
	Set_str_value(&job->info,XXCFTRANSFERNAME,s);
	if(s) free(s); s = 0;
	if(t) *t = '.';

	/* fix control file contents */

	s = Make_identifier( job );

	if( job->destination.count == 0 ){
		Set_str_value(&controlfile,IDENTIFIER,s);
	} else {
		s = Find_str_value(&job->destination,IDENTIFIER);
		cccc = Find_flag_value(&job->destination,COPIES);
		n = Find_flag_value(&job->destination,COPY_DONE);
		if( cccc > 1 ){
			plp_snprintf(buffer,sizeof(buffer), "C%d",n+1);
			s = safestrdup2(s,buffer,__FILE__,__LINE__);
			Set_str_value(&controlfile,IDENTIFIER,s);
			if(s) free(s); s = 0;
		} else {
			Set_str_value(&controlfile,IDENTIFIER,s);
		}
	}
	if( !Find_str_value(&controlfile,DATE) ){
		Set_str_value(&controlfile,DATE, Time_str( 0, 0 ) );
	}
	if( (Use_queuename_DYN || Force_queuename_DYN) &&
		!Find_str_value(&controlfile,QUEUENAME) ){
		s = Force_queuename_DYN;
		if( s == 0 ) s = Queue_name_DYN;
		if( s == 0 ) s = Printer_DYN;
		Set_str_value(&controlfile,QUEUENAME, s );
	}

	/* fix up the control file lines overrided by routing */
	buffer[1] = 0;
	for( i = 0; i < job->destination.count; ++i ){
		s = job->destination.list[i];
		cccc = cval(s);
		if( isupper(cccc) && cval(s+1) == '=' ){
			buffer[0] = cccc;
			Set_str_value( &controlfile,buffer,s+2 );
		}
	}

	order = Control_file_line_order_DYN;
    if( !order && Backwards_compatible_DYN ){
        order = BSD_order;
	} else if( !order ){
		order = LPRng_order;
	}
	wildcard = (safestrchr( order,'*') != 0);

	/*
	 * remove any line not required and fix up line metacharacters
	 */

	buffer[1] = 0;
	for( i = 0; i < controlfile.count; ){
		/* get line and first character on line */
		s = controlfile.list[i];
		cccc = *s;
		buffer[0] = cccc;
		/* remove any non-listed options */
		if( (!isupper(cccc) && !isdigit(cccc)) || (!safestrchr(order, cccc) && !wildcard) ){
			Set_str_value( &controlfile,buffer,0);
		} else {
			if( Backwards_compatible_DYN ){
				for( j = 0; maxclen[j].c && cccc != maxclen[j].c ; ++j );
				if( (len = maxclen[j].len) && safestrlen(s+1) > len ){
					s[len+1] = 0;
				}
			}
			++i;
		}
	}

	/*
	 * we check to see if order is correct - we need to check to
	 * see if allowed options in file first.
	 */

	if(DEBUGL3)Dump_line_list( "Fix_control: before sorting", &controlfile );
	n = Mergesort( controlfile.list,
		controlfile.count, sizeof( char *), ordercomp, order );
	if( n ){
		Errorcode = JABORT;
		logerr_die(LOG_ERR, "Fix_control: Mergesort failed" );
	}

	if(DEBUGL3) Dump_job( "Fix_control: after sorting", job );
	for( i = 0; i < controlfile.count; ++i ){
		s = controlfile.list[i];
		memmove(s+1,s+2,safestrlen(s+2)+1);
	}
	s = 0;

	{
		char *datalines;
		char *temp = Join_line_list(&controlfile,"\n");
		DEBUG3( "Fix_control: control info '%s'", temp );

		datalines = Fix_datafile_infox( job, number, file_hostname,
			xlate_format, update_df_names );
		DEBUG3( "Fix_control: data info '%s'", datalines );
		temp = safeextend2(temp,datalines,__FILE__,__LINE__);
		Set_str_value(&job->info,CF_OUT_IMAGE,temp);
		if( temp ) free(temp); temp = 0;
		if( datalines ) free(datalines); datalines = 0;
	}
	
	if( filter ){
		char *f_name = 0, *c_name = 0;
		DEBUG3("Fix_control: filter '%s'", filter );

		tempfd = Make_temp_fd( &f_name );
		tempfc = Make_temp_fd( &c_name );
		s = Find_str_value(&job->info,CF_OUT_IMAGE );
		if( Write_fd_str( tempfc, s ) < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Fix_control: write to tempfile failed" );
		}
		if( lseek( tempfc, 0, SEEK_SET ) == -1 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Fix_control: lseek failed" );
		}
		if( (n = Filter_file( Send_query_rw_timeout_DYN, tempfc, tempfd, "CONTROL_FILTER",
			filter, Filter_options_DYN, job, 0, 1 )) ){
			Errorcode = n;
			logerr_die(LOG_ERR, "Fix_control: control filter failed with status '%s'",
				Server_status(n) );
		}
		s = 0;
		if( n < 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Fix_control: read from tempfd failed" );
		}
		s = Get_fd_image( tempfd, 0 );
		if( s == 0 || *s == 0 ){
			Errorcode = JFAIL;
			logerr_die(LOG_INFO, "Fix_control: zero length control filter output" );
		}
		DEBUG4("Fix_control: control filter output '%s'", s);
		Set_str_value(&job->info,CF_OUT_IMAGE,s);
		if(s) free(s); s = 0;
		if( f_name ) unlink(f_name); f_name = 0;
		if( c_name ) unlink(c_name); c_name = 0;
		close( tempfc ); tempfc = -1;
		close( tempfd ); tempfd = -1;
	}
}


/*
 * Buffer management
 *  Set up and put values into an output buffer for
 *  transmission at a later time
 */
void Init_buf(char **buf, int *max, int *len)
{
	DEBUG4("Init_buf: buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len );
	if( *max <= 0 ) *max = LARGEBUFFER;
	if( *buf == 0 ) *buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
	*len = 0;
	(*buf)[0] = 0;
}

void Put_buf_len( const char *s, int cnt, char **buf, int *max, int *len )
{
	DEBUG4("Put_buf_len: starting- buf 0x%lx, max %d, len %d, adding %d",
		Cast_ptr_to_long(*buf), *max, *len, cnt );
	if( s == 0 || cnt <= 0 ) return;
	if( *max - *len <= cnt ){
		*max += ((LARGEBUFFER + cnt )/1024)*1024;
		*buf = realloc_or_die( *buf, *max+1,__FILE__,__LINE__);
		DEBUG4("Put_buf_len: update- buf 0x%lx, max %d, len %d",
		Cast_ptr_to_long(*buf), *max, *len);
	}
	memcpy( *buf+*len, s, cnt );
	*len += cnt;
	(*buf)[*len] = 0;
}

void Put_buf_str( const char *s, char **buf, int *max, int *len )
{
	if( s && *s ) Put_buf_len( s, safestrlen(s), buf, max, len );
}
