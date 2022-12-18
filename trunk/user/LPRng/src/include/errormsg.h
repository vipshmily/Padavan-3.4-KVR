/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/

#ifndef _ERRORMSG_H_
#define _ERRORMSG_H_ 1

#define LOGDEBUG logDebug
#define DIEMSG Diemsg
#define WARNMSG Warnmsg
#define MESSAGE Message

/* PROTOTYPES */
#ifdef HAVE_STRERROR
#define Errormsg strerror
#else
const char * Errormsg ( int err );
#endif
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logmsg(int kind, const char *msg,...) PRINTFATTR(2,3)
#else
 void logmsg(va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void fatal (int kind, const char *msg,...) PRINTFATTR(2,3)
#else
 void fatal (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logerr (int kind, const char *msg,...) PRINTFATTR(2,3)
#else
 void logerr (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void logerr_die (int kind, const char *msg,...) PRINTFATTR(2,3)
#else
 void logerr_die (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Diemsg (const char *msg,...) PRINTFATTR(1,2)
#else
 void Diemsg (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Warnmsg (const char *msg,...) PRINTFATTR(1,2)
#else
 void Warnmsg (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void Message (const char *msg,...) PRINTFATTR(1,2)
#else
 void Message (va_alist) va_dcl
#endif
;
/* VARARGS1 */
#ifdef HAVE_STDARGS
 void logDebug (const char *msg,...) PRINTFATTR(1,2)
#else
 void logDebug (va_alist) va_dcl
#endif
;
const char *Sigstr (int n);
const char *Decode_status (plp_status_t *status);
const char *Server_status( int d );
struct job;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setstatus (struct job *job,const char *fmt,...) PRINTFATTR(2,3)
#else
 void setstatus (va_alist) va_dcl
#endif
;
/* VARARGS2 */
#ifdef HAVE_STDARGS
 void setmessage (struct job *job,const char *header, const char *fmt,...) PRINTFATTR(3,4)
#else
 void setmessage (va_alist) va_dcl
#endif
;

#endif
