/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPD_H_
#define _LPD_H_ 1


/*
 * file_info - information in log file
 */

struct file_info {
	int fd;
	int max_size;	/* maximum file size */ 
	char *outbuffer;	/* for writing */
	int outmax;	/* max buffer size */
	char *inbuffer;		/* buffer for IO */
	int  inmax;	/* buffer size */
	int start;			/* starting offset */
	int count;			/* total size of info */
};

union val{
	int v;
	char s[sizeof(int)];
};

EXTERN int Foreground_LPD;
EXTERN char *Worker_LPD;
EXTERN char *Logfile_LPD;
EXTERN volatile int Reread_config;
EXTERN int Started_server;

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
static void Setup_log(char *logfile );
static void Reinit(void);
static int Get_lpd_pid(void);
static void Set_lpd_pid(int lockfd);
static int Lock_lpd_pid(void);
static int Read_server_status( int fd );
static void usage(void);
static void Get_parms(int argc, char *argv[] );
static void Accept_connection( int sock );
static int Start_all( int first_scan, int *start_fd );
plp_signal_t sigchld_handler (int signo);
static void Setup_waitpid (void);
static void Setup_waitpid_break (void);
static void Fork_error( int last_fork_pid_value );

#endif
