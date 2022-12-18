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
#include "linksupport.h"
#include "lockfile.h"
#include "stty.h"
#include "openprinter.h"

/***************************************************************************
 * int Printer_open: opens the Printer_DYN
 ***************************************************************************/

/*
 * int Printer_open(
 *   lp_device        - open this device
 *   char error, int errlen - record errors
 *   int max_attempts - max attempts to open device or socket
 *   int interval,    - interval between attempts
 *   int max_interval, - maximum interval
 *   int grace,       - minimum time between attempts
 *   int connect_timeout - time to wait for connection success
 *   int *pid         - if we have a filter program, return pid
 *  returns:
 *    file descriptor
 */

int Printer_open( char *lp_device, int *status_fd, struct job *job,
	int max_attempts, int interval, int max_interval, int grace,
	int connect_tmout, int *filterpid, int *poll_for_status )
{
	int attempt, err = 0, n, device_fd, c, in[2], pid, readable, mask;
	struct stat statb;
	time_t tm;
	char tm_str[32], errmsg[SMALLBUFFER];
	char *host, *port, *filter;
	struct line_list args;
	int errlen = sizeof(errmsg);

	errmsg[0] = 0;
	Init_line_list(&args);
	host = port = filter = 0;
	*filterpid = 0;
	DEBUG1( "Printer_open: device '%s', max_attempts %d, grace %d, interval %d, max_interval %d",
		lp_device, max_attempts, grace, interval, max_interval );
	time( &tm );
	tm_str[0] = 0;
	if( lp_device == 0 ){
		fatal(LOG_ERR, "Printer_open: printer '%s' missing lp_device value",
			Printer_DYN );
	}

	*status_fd = device_fd = -1;
	*poll_for_status = 0;
	/* we repeat until we get the device or exceed maximum attempts */
	for( attempt = 0; device_fd < 0 && (max_attempts <= 0 || attempt < max_attempts); ++attempt ){
		errmsg[0] = 0;
		if( grace ) plp_sleep(grace);
		c = lp_device[0];
		switch( c ){
		case '|':
#if !defined(HAVE_SOCKETPAIR)
			fatal(LOG_ERR, "Printer_open: requires socketpair() system call for output device to be filter");
#else
			if( socketpair( AF_UNIX, SOCK_STREAM, 0, in ) == -1 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO, "Printer_open: socketpair() for filter input failed");
			}
#endif
			Max_open(in[0]); Max_open(in[1]);
			DEBUG3("Printer_open: fd in[%d,%d]", in[0], in[1] );
			/* set up file descriptors */
			Free_line_list(&args);
			Check_max(&args,10);
			args.list[args.count++] = Cast_int_to_voidstar(in[0]);	/* stdin */
			args.list[args.count++] = Cast_int_to_voidstar(in[0]);	/* stdout */
			args.list[args.count++] = Cast_int_to_voidstar(in[0]);	/* stderr */
			if( (pid = Make_passthrough( lp_device, Filter_options_DYN, &args,
					job, 0 )) < 0 ){
				Errorcode = JFAIL;
				logerr_die(LOG_INFO,
					"Printer_open: could not create LP_FILTER process");
			}
			args.count = 0;
			Free_line_list(&args);

			*filterpid = pid;
			device_fd = in[1];
			*status_fd = in[1];
			if( (close( in[0] ) == -1 ) ){
				logerr_die(LOG_INFO, "Printer_open: close(%d) failed", in[0]);
			}
			break;

		case '/':
			DEBUG3( "Printer_open: Is_server %d, DaemonUID %ld, DaemonGID %ld, UID %ld, EUID %ld, GID %ld, EGID %ld",
			Is_server, (long)DaemonUID, (long)DaemonGID,
			(long)getuid(), (long)geteuid(), (long)getgid(), (long)getegid() );
			device_fd = Checkwrite_timeout( connect_tmout, lp_device, &statb, 
				(Read_write_DYN || Lock_it_DYN) ?(O_RDWR):(O_APPEND|O_WRONLY),
				0, Nonblocking_open_DYN );
			err = errno;
			if( device_fd > 0 ){
				if( Lock_it_DYN ){
					int status;
					/*
					 * lock the device so that multiple servers can
					 * use it
					 */
					status = 0;
					if( isatty( device_fd ) ){
						status = LockDevice( device_fd, 0 );
					} else if( S_ISREG(statb.st_mode ) ){
						status = Do_lock( device_fd, 0 );
					}
					if( status < 0 ){
						err = errno;
						setstatus( job,
							"lock '%s' failed - %s", lp_device, Errormsg(errno) );
						close( device_fd );
						device_fd = -1;
					}
				}
				if( device_fd > 0 && isatty( device_fd ) ){
					Do_stty( device_fd );
				}
				*status_fd = device_fd;
			}
			break;
		default:
			if( safestrchr( lp_device, '%' ) ){
				/* we have a host%port form */
				host = lp_device;
			} else {
				Errorcode = JABORT;
				fatal(LOG_ERR, "Printer_open: printer '%s', bad 'lp' entry '%s'",
					Printer_DYN, lp_device );
			}
			DEBUG1( "Printer_open: doing link open '%s'", lp_device );
			setstatus(job, "opening TCP/IP connection to %s", host );
			*status_fd = device_fd = Link_open( host, connect_tmout, 0, 0, errmsg, errlen );
            err = errno;
			break;
		}

		if( device_fd < 0 ){
			DEBUG1( "Printer_open: open '%s' failed, max_attempts %d, attempt %d '%s'",
				lp_device, max_attempts, attempt, Errormsg(err) );
			if( max_attempts && attempt <= max_attempts ){
				n = 8;
				if( attempt < n ) n = attempt;
				n = interval*( 1 << n );
				if( max_interval > 0 && n > max_interval ) n = max_interval;
				setstatus( job, "cannot open '%s' - '%s', attempt %d, sleeping %d",
						lp_device, errmsg[0]?errmsg:Errormsg( err), attempt+1, n );
				if( n > 0 ){
					plp_sleep(n);
				}
			} else {
				setstatus( job, "cannot open '%s' - '%s', attempt %d",
						lp_device, errmsg[0]?errmsg:Errormsg( err), attempt+1 );
			}
		}
	}
	if( device_fd >= 0 ){
		int fd = *status_fd;
		if( fstat( fd, &statb ) < 0 ) {
			logerr_die(LOG_INFO, "Printer_open: fstat() on status_fd %d failed", fd);
		}
		/* we can only read status from a device, fifo, or socket */
		if( (mask = fcntl( fd, F_GETFL, 0 )) == -1 ){
			Errorcode = JABORT;
			logerr_die(LOG_ERR, "Printer_open: cannot fcntl fd %d", fd );
		}
		DEBUG2( "Printer_open: status_fd %d fcntl 0%o", fd, mask );
		mask &= O_ACCMODE;
		/* first, check to see if we have RD or RW */
		readable = 1;
		switch( mask ){
		case O_WRONLY:
			readable = 0;
			if( fd == device_fd ){
				*status_fd = -1;
			} else {
				Errorcode = JABORT;
				fatal(LOG_ERR, "Printer_open: LOGIC ERROR: status_fd %d WRITE ONLY", fd );
			}
			break;
		}
		/* we handle the case where we have a device like a parallel port
		 *  or a USB port which does NOT support 'select()'
		 * AND where there may be some really strange status at the end
		 * of the printing operation
		 * AND we cannot close the connection UNTIL we get the status
		 *   This is really silly but we need to handle it.  Note that the
		 *   IFHP filter has to do exactly the same thing... Sigh...
		 */
		if( readable && S_ISCHR(statb.st_mode) && !isatty(device_fd) ){
			*poll_for_status = 1;
		}
	}

	DEBUG1 ("Printer_open: '%s' is fd %d", lp_device, device_fd);
	return( device_fd );
}

