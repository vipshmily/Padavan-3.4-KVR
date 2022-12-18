/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

/*************************************************************
 * void Get_debug_parm(int argc, char *argv[], struct keywords *list)
 * Scan the command line for -D debugparms
 *  debugparms has the format value,key=value,key,key@...
 *  1. if the value is an integer,  then we treat it as a value for DEBUG
 *  2. if a key is present,  then we scan the list and find the
 *     match for the key.  We then convert according to the type of
 *     option expected.
 *************************************************************/

#include "lp.h"
#include "errorcodes.h"
#include "getopt.h"
#include "child.h"
/**** ENDINCLUDE ****/

 struct keywords debug_vars[]		/* debugging variables */
 = {
#if !defined(NODEBUG)
    { "print",  0,FLAG_K,(void *)&Debug,1, 0,0},
    { "print+1",0,FLAG_K,(void *)&Debug,1, 0,0},
    { "print+2",0,FLAG_K,(void *)&Debug,2, 0,0},
    { "print+3",0,FLAG_K,(void *)&Debug,3, 0,0},
    { "print+4",0,FLAG_K,(void *)&Debug,4, 0,0},
    { "lpr",0,FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK,0},
    { "lpr+1",0,FLAG_K,(void *)&DbgFlag,DRECV1, DRECVMASK,0},
    { "lpr+2",0,FLAG_K,(void *)&DbgFlag,DRECV2|DRECV1, DRECVMASK,0},
    { "lpr+3",0,FLAG_K,(void *)&DbgFlag,DRECV3|DRECV2|DRECV1, DRECVMASK,0},
    { "lpr+4",0,FLAG_K,(void *)&DbgFlag,DRECV4|DRECV3|DRECV2|DRECV1, DRECVMASK,0},
    { "lpc",0,FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK,0},
    { "lpc+1",0,FLAG_K,(void *)&DbgFlag,DCTRL1, DCTRLMASK,0},
    { "lpc+2",0,FLAG_K,(void *)&DbgFlag,DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lpc+3",0,FLAG_K,(void *)&DbgFlag,DCTRL3|DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lpc+4",0,FLAG_K,(void *)&DbgFlag,DCTRL4|DCTRL3|DCTRL2|DCTRL1, DCTRLMASK,0},
    { "lprm",0,FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK,0},
    { "lprm+1",0,FLAG_K,(void *)&DbgFlag,DLPRM1, DLPRMMASK,0},
    { "lprm+2",0,FLAG_K,(void *)&DbgFlag,DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lprm+3",0,FLAG_K,(void *)&DbgFlag,DLPRM3|DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lprm+4",0,FLAG_K,(void *)&DbgFlag,DLPRM4|DLPRM3|DLPRM2|DLPRM1, DLPRMMASK,0},
    { "lpq",0,FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK,0},
    { "lpq+1",0,FLAG_K,(void *)&DbgFlag,DLPQ1, DLPQMASK,0},
    { "lpq+2",0,FLAG_K,(void *)&DbgFlag,DLPQ2|DLPQ1, DLPQMASK,0},
    { "lpq+3",0,FLAG_K,(void *)&DbgFlag,DLPQ3|DLPQ2|DLPQ1, DLPQMASK,0},
    { "lpq+4",0,FLAG_K,(void *)&DbgFlag,DLPQ4|DLPQ3|DLPQ2|DLPQ1, DLPQMASK,0},
    { "network",0,FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK,0},
    { "network+1",0,FLAG_K,(void *)&DbgFlag,DNW1, DNWMASK,0},
    { "network+2",0,FLAG_K,(void *)&DbgFlag,DNW2|DNW1, DNWMASK,0},
    { "network+3",0,FLAG_K,(void *)&DbgFlag,DNW3|DNW2|DNW1, DNWMASK,0},
    { "network+4",0,FLAG_K,(void *)&DbgFlag,DNW4|DNW3|DNW2|DNW1, DNWMASK,0},
    { "database",0,FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK,0},
    { "database+1",0,FLAG_K,(void *)&DbgFlag,DDB1, DDBMASK,0},
    { "database+2",0,FLAG_K,(void *)&DbgFlag,DDB2|DDB1, DDBMASK,0},
    { "database+3",0,FLAG_K,(void *)&DbgFlag,DDB3|DDB2|DDB1, DDBMASK,0},
    { "database+4",0,FLAG_K,(void *)&DbgFlag,DDB4|DDB3|DDB2|DDB1, DDBMASK,0},
    { "database+4",0,FLAG_K,(void *)&DbgFlag,DDB4, DDBMASK,0},
    { "log",0,FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK,0},
    { "log+1",0,FLAG_K,(void *)&DbgFlag,DLOG1, DLOGMASK,0},
    { "log+2",0,FLAG_K,(void *)&DbgFlag,DLOG2|DLOG1, DLOGMASK,0},
    { "log+3",0,FLAG_K,(void *)&DbgFlag,DLOG3|DLOG2|DLOG1, DLOGMASK,0},
    { "log+4",0,FLAG_K,(void *)&DbgFlag,DLOG4|DLOG3|DLOG2|DLOG1, DLOGMASK,0},
    { "test",0,INTEGER_K,(void *)&DbgTest,0,0,0},
#endif
    { 0,0,0,0,0,0,0 }
};

/*

 Parse_debug (char *dbgstr, struct keywords *list, int interactive );
 Input string:  value,key=value,flag+n

 1. crack the input line at the ','
 2. crack each option at = 
 3. search for key words
 4. assign value to variable

*/

	static const char *guide[] = {
	" use on command line, or in printcap :db=... entry", 
	" for server:",
	"   print:     show queue (printing) actions, larger number, more information",
	"     NUMBER     same as print+NUMBER",
	"   lpr:       show servicing lpr actions",
	"   lpq:       show servicing lpq actions",
	"   lprm:      show servicing lprm actions",
	"   network:   show low level network actions",
	"   database:  show low level database actions",
	"   log:       Testing.  Don't use this unless you read the code.",
	"   test:      Testing.  don't use this unless you read the code.",
	" for clients (lpr, lpq, etc):",
	"   print:     show client actions, larger number, more information",
	"     NUMBER     same as print+NUMBER",
	"   network:   show low level network actions.",
	"   database:  show low level database actions.",
		0
	};
void Parse_debug (const char *dbgstr, int interactive )
{
#if !defined(NODEBUG)
	char *key, *end;
	const char *convert;
	int i, n, found, count;
	struct keywords *list = debug_vars;
	struct line_list l;

	Init_line_list(&l);
	Split(&l,dbgstr,File_sep,0,0,0,0,0,0);

	for( count = 0; count < l.count; ++count ){
		found = 0;
		end = key = l.list[count];
		n = strtol(key,&end,0);
		if( *end == 0 ){
			Debug = n;
			if( n == 0 )DbgFlag = 0;
			found = 1;
		} else {
			if( (end = safestrchr(key,'=')) ){
				*end++ = 0;
				n = strtol(end,0,0);
			}
			/* search the keyword list */
			for (i = 0;
				(convert = list[i].keyword) && safestrcasecmp( convert, key );
				++i );
			if( convert != 0 ){
				switch( list[i].type ){
				case INTEGER_K:
					*(int *)list[i].variable = n;
					found = 1;
					break;
				case FLAG_K:
					*(int *)list[i].variable |= list[i].maxval;
					/*
						DEBUG1("Parse_debug: key '%s', val 0x%x, DbgFlag 0x%x",
						key, list[i].maxval, DbgFlag );
					 */
					found = 1;
					break;
				default:
					break;
				}
			}
		}
		if(!found && interactive ){
		    int i;
		    int lastflag = 0;
		    FPRINTF (STDERR, "debug flag format: num | flag[+num] | flag=str\n");
		    FPRINTF (STDERR, "  flag names:");
		    for (i = 0; list[i].keyword; i++) {
				if( safestrchr( list[i].keyword, '+' ) ) continue;
				if( lastflag ){
					FPRINTF( STDERR, ", " );
					if( !(lastflag % 4) ) FPRINTF( STDERR, "\n   " );
				} else {
					FPRINTF( STDERR, " " );
				}
				switch( list[i].type ){
				case INTEGER_K:
					FPRINTF (STDERR, "%s=num", list[i].keyword);
					break;
				case STRING_K:
					FPRINTF (STDERR, "%s=str", list[i].keyword);
					break;
				case FLAG_K:
					FPRINTF (STDERR, "%s[+N]", list[i].keyword );
					break;
				default:
					break;
				}
				++lastflag;
			}
		    FPRINTF (STDERR, "\n");
		    for(i = 0; guide[i]; ++i ){
				FPRINTF (STDERR, "%s\n", guide[i]);
		    }

			Errorcode = JABORT;
			if( interactive > 0 ) cleanup(0);
		}
	}
	Free_line_list(&l);
#endif
	/* LOGDEBUG("Parse_debug: Debug %d, DbgFlag 0x%x", Debug, DbgFlag ); */
}
