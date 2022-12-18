/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _LPC_H_
#define _LPC_H_ 1

EXTERN int Auth;
extern char LPC_optstr[]; /* number of status lines */
EXTERN char *Server;
EXTERN int All_pc_printers; /* use the printers in the printcap */

/* PROTOTYPES */
int main(int argc, char *argv[], char *envp[]);
static void doaction( struct line_list *args );
static void Get_parms(int argc, char *argv[] );
static void use_msg(void);
static void usage(void);

#endif
