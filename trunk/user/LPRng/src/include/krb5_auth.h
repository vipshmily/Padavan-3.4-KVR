/***************************************************************************
 * LPRng - An Extended Print Spooler System
 *
 * Copyright 1988-2003, Patrick Powell, San Diego, CA
 *     papowell@lprng.com
 * See LICENSE for conditions of use.
 ***************************************************************************/



#ifndef _KRB5_AUTH_H
#define _KRB5_AUTH_H 1

#ifndef WITHPLUGINS
#include "user_auth.h"

#if defined(KERBEROS)
extern const struct security kerberos5_auth;
extern const struct security k5conn_auth;
#endif
#endif

#endif
