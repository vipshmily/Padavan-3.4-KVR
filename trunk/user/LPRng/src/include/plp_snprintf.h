/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _PLP_SNPRINTF_
#define _PLP_SNPRINTF_

/* PRO TO TYPES */
/* VARARGS3 */
 int plp_vsnprintf(char *str, size_t count, const char *fmt, va_list args) PRINTFATTR(3,0);
/* VARARGS3 */
 int plp_unsafe_vsnprintf(char *str, size_t count, const char *fmt, va_list args) PRINTFATTR(3,0);
/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_snprintf (char *str,size_t count,const char *fmt,...) PRINTFATTR(3,4)
#else
 int plp_snprintf (va_alist) va_dcl
#endif
;
/* VARARGS3 */
#ifdef HAVE_STDARGS
 int plp_unsafe_snprintf (char *str,size_t count,const char *fmt,...) PRINTFATTR(3,4)
#else
 int plp_unsafe_snprintf (va_alist) va_dcl
#endif
;

#endif
