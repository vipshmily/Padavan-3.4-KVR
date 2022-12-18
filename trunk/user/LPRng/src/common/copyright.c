/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 *
 ***************************************************************************/

#include "lp.h"
/**** ENDINCLUDE ****/

const char *Copyright[] = {
 PACKAGE_NAME "-" PACKAGE_VERSION
#if defined(KERBEROS)
 ", Kerberos5"
#endif
", Copyright 1988-2003 Patrick Powell, <papowell@lprng.com>",

"",
"locking uses: "
#ifdef HAVE_FCNTL
		"fcntl (preferred)"
#else
#ifdef HAVE_LOCKF
            "lockf"
#else
            "flock (does NOT work over NFS)"
#endif
#endif
,
"stty uses: "
#if USE_STTY == SGTTYB
            "sgttyb"
#endif
#if USE_STTY == TERMIO
            "termio"
#endif
#if USE_STTY == TERMIOS
            "termios"
#endif
,
#ifdef SSL_ENABLE
"with SSL"
#else
"without SSL"
#endif
,
"",
#include "license.h"
#include "copyright.h"
0 };
