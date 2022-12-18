/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 * $Id: checkpc.h,v 1.74 2004/09/24 20:19:59 papowell Exp $
 ***************************************************************************/



#ifndef _CHECKPC_H_
#define _CHECKPC_H_ 1

/* PROTOTYPES */
int main( int argc, char *argv[], char *envp[] );
static void mkdir_path( char *path );
static void Scan_printer(struct line_list *spooldirs);
static void Check_executable_filter( const char *id, char *filter_str );
static void Make_write_file( char *file, char *printer );
static void usage(void);
static int getage( char *age );
static int getk( char *age );
static int Check_file( char  *path, int fix, int age, int rmflag );
static int Check_read_file( char  *path, int fix, int perms );
static int Fix_create_dir( char  *path, struct stat *statb );
static int Fix_owner( char *path );
static int Fix_perms( char *path, int perms );
static int Check_spool_dir( char *path );
static void Test_port(int ruid, int euid, char *serial_line );
static void Fix_clean( char *s, int no );
static int Check_path_list( char *plist, int allow_missing );

#endif
