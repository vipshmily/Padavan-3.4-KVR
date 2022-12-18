AC_INIT(LPRng,3.8.C,lprng-devel@lists.sf.net)
AC_CONFIG_SRCDIR(src/common/lpr.c)
AM_CONFIG_HEADER(config.h)
AM_INIT_AUTOMAKE([foreign])

AM_MAINTAINER_MODE

AC_PREFIX_DEFAULT(/usr/local)

mysaved_CFLAGSset="${CFLAGS+set}"
AC_PROG_CC
if test "$ac_cv_prog_gcc" = yes; then
	if test "$mysaved_CFLAGSset" != set ; then
		CFLAGS="-g -W -Wall -Wno-unused -Wstrict-prototypes -Wmissing-prototypes"
	fi
fi;
AC_SYS_LARGEFILE

AC_PROG_AWK
AC_SUBST(AWK)
AC_PATH_PROG(SED,sed)dnl
AC_SUBST(SED)dnl
AC_PATH_PROG(PERL,perl)dnl
AC_SUBST(PERL)dnl
dnl -- TODO:
dnl this is used in checkpc, why not just use "chown" there?
dnl same with chgrp
dnl --
AC_PATH_PROG(CHOWN,chown)dnl
AC_DEFINE_UNQUOTED(CHOWN, "$CHOWN",[full path to chown])
AC_PATH_PROG(CHGRP,chgrp)dnl
AC_DEFINE_UNQUOTED(CHGRP, "$CHGRP",[full path to chgrp])
AC_PATH_PROG(PRUTIL,pr)dnl
AC_DEFINE_UNQUOTED(PRUTIL, "$PRUTIL",[full path to prutil])
AC_PATH_PROG(OPENSSL,openssl)dnl
AC_SUBST(OPENSSL)dnl
AC_DEFINE_UNQUOTED(OPENSSL, "$OPENSSL",[full path to openssl])

dnl dnl check to see if setuid is suppressed
dnl AC_ARG_ENABLE( setuid,
dnl AS_HELP_STRING([--disable-setuid],[do not install client executables setuid root]),
dnl [
dnl if test "$enableval" = "yes" ; then
dnl 	PERMS=SUID_ROOT_PERMS
dnl else
dnl 	PERMS=NORM_PERMS
dnl fi
dnl ],
dnl [ PERMS=SUID_ROOT_PERMS ],
dnl )
dnl AC_MSG_NOTICE([installing client with $PERMS])
dnl AC_SUBST(PERMS)

dnl check to see if priv ports required
ENABLE_BOOLEAN(priv_ports,
[--enable-priv_ports],[require connections from privileged ports],
"no",
[require connections from priviledged ports: $v],
[PRIV_PORTS=""],[PRIV_PORTS="#"])
AC_SUBST(PRIV_PORTS)

dnl check if only accept from local host
ENABLE_BOOLEAN(remote,
[--disable-remote],[do not accept remote jobs by default],
"yes",
[listen for remote connections by default  : $v],
[NOREMOTE="#"
LPD_LISTEN_PORT='"=515"'
],[NOREMOTE=""
LPD_LISTEN_PORT='"=off"'
])
AC_SUBST(LPD_LISTEN_PORT)
AC_SUBST(NOREMOTE)
AC_DEFINE_UNQUOTED(LPD_LISTEN_PORT, $LPD_LISTEN_PORT,[default value for lpd_listen_port (0 means default port ("=515"), "=off" means to not listen at all])

ENABLE_BOOLEAN(lpd.conf.local,
[--enable-lpd.conf.local],[include lpd.conf.local in lpd.conf],
"no",
[include lpd.conf.local in lpd.conf        : $v],
[INCLUDELPDCONFLOCAL=""
],[INCLUDELPDCONFLOCAL="# "
])
AC_SUBST(INCLUDELPDCONFLOCAL)

dnl check to see if force_localhost is suppressed
ENABLE_BOOLEAN(force_localhost,
[--disable-force_localhost],[disable force_localhost default],
"yes",
[force_localhost is default                : $v],
[FORCE_LOCALHOST=1],[FORCE_LOCALHOST=0])
AC_DEFINE_UNQUOTED(FORCE_LOCALHOST,"$FORCE_LOCALHOST",[Force Localhost (force_localhost)])

dnl check to see if clients require lpd.conf and printcap
ENABLE_BOOLEAN(require_configfiles,
[--disable-require_configfiles],[client programs require lpd.conf, printcap],
"yes",
[client programs require lpd.conf, printcap: $v],
[REQUIRE_CONFIGFILES=1], [REQUIRE_CONFIGFILES=0])
AC_DEFINE_UNQUOTED(REQUIRE_CONFIGFILES, "$REQUIRE_CONFIGFILES",[require configfiles])

dnl check to see if kerberos is disabled
AH_TEMPLATE(KERBEROS,[enable Kerberos support])
ENABLE_BOOLEAN(kerberos,
[--enable-kerberos],[enable kerberos support],
"no",
[enable kerberos support                   : $v],
[KERBEROS=1;
AC_DEFINE_UNQUOTED(KERBEROS,"1")],
[KERBEROS=""])
AM_CONDITIONAL(WITHKERBEROS, test $v = yes)
AC_SUBST(KERBEROS)

dnl find kerberos libraries
ENABLE_BOOLEAN(kerberos_checks,
[--disable-kerberos_checks],[disable kerberos library location and checking for support],
"yes",
[locate Kerberos libraries and check       : $v],
[KERBEROS_CHECKS=1],[KERBEROS_CHECKS=""])

WITH_DIR(lpddir,
[--with-lpddir=DIR],[lpd executable directory (default ${sbindir})],
[lpdbindir],['${sbindir}'],
[directory to place lpd executable into    : $lpdbindir])

AC_ARG_WITH(config_subdir,
AS_HELP_STRING([--with-config_subdir=SUBDIR],[configuration subdirectory (default 'lpd')]),
CONFIGSUBDIR=$withval,CONFIGSUBDIR=lpd
)
configdir=\${sysconfdir}/${CONFIGSUBDIR}
AC_MSG_NOTICE([configuration directory          configdir= $configdir])
AC_SUBST(configdir)

WITH_DIR(lpd_conf_path,
[--with-lpd_conf_path=PATH],[path of lpd.conf (default: ${configdir}/lpd.conf)],
[LPD_CONF_PATH],['${configdir}/lpd.conf'],
[lpd.conf location                         : $LPD_CONF_PATH])
AC_SUBST(LPD_CONF_PATH)

WITH_DIR(lpd_perms_path,
[--with-lpd_perms_path=PATH],[path of lpd.perms (default: ${configdir}/lpd.perms)],
[LPD_PERMS_PATH],['${configdir}/lpd.perms'],
[lpd.perms location                        : $LPD_PERMS_PATH])
AC_SUBST(LPD_PERMS_PATH)

WITH_DIR(printcap_path,
[--with-printcap_path=PATH],[path of printcap (default ${sysconfdir}/printcap)],
[PRINTCAP_PATH],['${sysconfdir}/printcap'],
[printcap location                         : $PRINTCAP_PATH])
AC_SUBST(PRINTCAP_PATH)

WITH_DIR(lpd_printcap_path,
[--with-lpd_printcap_path=PATH],[path of lpd_printcap (default ${configdir}/lpd_printcap)],
[LPD_PRINTCAP_PATH],['${configdir}/lpd_printcap'],
[lpd_printcap location                     : $LPD_PRINTCAP_PATH])

WITH_DIR(localedir,
[--with-localedir=PATH],[specify locale information directory],
[localedir],['${datadir}/locale'],
[locale information directory is           : $localedir])
AC_SUBST(localedir)

WITH_DIR(unix_socket_path,
[--with-unix_socket_path=DIR],[unix socket path (default /var/run/lprng)],
[UNIXSOCKETPATH],[/var/run/lprng],
[unix socket path is                       : $UNIXSOCKETPATH])

WITH_DIR(lockfile,
[--with-lockfile=PATH],[lockfile PATH, default /var/run/lpd ],
[LOCKFILE],['/var/run/lpd'],
[lockfile directory                        : $LOCKFILE])

WITH_DIR(spooldir,
[--with-spooldir=PATH],[default spool directory, default /var/spool/lpd ],
[SD_DEFAULT],['/var/spool/lpd'],
[default spool directory (:sd=SPOOLDIR/%P) : $SD_DEFAULT])

WITH_DIR(plugindir,
[--with-plugindir=DIR],[dynamic loadable plugin directory (default ${libdir}/lprng/plugins)],
[plugindir],['${libdir}/lprng/plugins'],
[plugin directory                plugindir = $plugindir])

WITH_DIR(filterdir,
[--with-filterdir=DIR],[filter directory (default ${libexecdir}/filters)],
[filterdir],['${libexecdir}/filters'],
[filter directory                filterdir = $filterdir])

dnl  FILTER_LD_PATH (LD_LIBRARY_PATH for filters) value
dnl
AC_ARG_WITH(ld_library_path,
AS_HELP_STRING([--with-ld_library_path=PATH],[FILTER_LD_PATH value, default empty (for Solaris you might need /lib:/usr/lib:/usr/local/lib)]),
FILTER_LD_PATH="$withval",
FILTER_LD_PATH=""
)
AC_MSG_NOTICE([filter LD_LIBRARY_PATH:    FILTER_LD_PATH = $FILTER_LD_PATH])
AC_SUBST(FILTER_LD_PATH)
AC_DEFINE_UNQUOTED(FILTER_LD_PATH, "$FILTER_LD_PATH",[default LD_LIBRARY_PATH for filters])

dnl  filter PATH value
dnl
AC_ARG_WITH(filter-path,
AS_HELP_STRING([--with-filter-path=PATH],[filter PATH value, default /bin:/usr/bin:/usr/local/bin ]),
FILTER_PATH="$withval",
FILTER_PATH="/bin:/usr/bin:/usr/local/bin"
)
AC_MSG_NOTICE([filter PATH:                  FILTER_PATH = $FILTER_PATH])
AC_SUBST(FILTER_PATH)
AC_DEFINE_UNQUOTED(FILTER_PATH, "$FILTER_PATH",[default PATH for filters])

dnl  user name
dnl
AC_ARG_WITH(userid,
AS_HELP_STRING([--with-userid=NAME],[run LPRng software as this userid, default daemon ]),
USERID="$withval", USERID="daemon",
)
AC_MSG_NOTICE([userID to run lpd as                      : $USERID])
AC_DEFINE_UNQUOTED(USERID, "$USERID",[userid to run LPRng as, default daemon])

dnl  group value
dnl
AC_ARG_WITH(groupid,
AS_HELP_STRING([--with-groupid=NAME],[run LPRng software as this groupid, default daemon ]),
GROUPID="$withval", GROUPID="daemon",
)
AC_MSG_NOTICE([groupID to run lpd as                     : $GROUPID])
AC_DEFINE_UNQUOTED(GROUPID, "$GROUPID",[groupid to run LPRng as, default daemon])

dnl  done_jobs value
dnl
AC_ARG_WITH(done_jobs,
AS_HELP_STRING([--with-done_jobs=N],[retain last N job status, default 1]),
DONE_JOBS="$withval", DONE_JOBS="1",
)
AC_MSG_NOTICE([number of job status to retain            : $DONE_JOBS])
AC_DEFINE_UNQUOTED(DONE_JOBS, "$DONE_JOBS",[number of jobs to retain status of])

dnl  done_jobs_max_age value
dnl
AC_ARG_WITH(done_jobs_max_age,
AS_HELP_STRING([--with-done_jobs_max_age=N],[retain job status N seconds, default 0 - no expiry]),
DONE_JOBS_MAX_AGE="$withval", DONE_JOBS_MAX_AGE="0",
)
AC_MSG_NOTICE([max age to retain job status (0=no expiry): $DONE_JOBS_MAX_AGE])
AC_DEFINE_UNQUOTED(DONE_JOBS_MAX_AGE, "$DONE_JOBS_MAX_AGE",[number of seconds to retain job status])

dnl  clear program
dnl
AC_MSG_CHECKING(terminal screen clear program)
if test -z "$CLEAR" ; then
  AC_PATH_PROG(CLEAR,clear)dnl
fi
if test -z "$CLEAR" ; then
 AC_MSG_WARN([Program 'clear' is not found. Set environment CLEAR=no if you do not want to use it]);
 exit 1
fi
if test "$CLEAR" = "no" ; then CLEAR= ; fi
if test -n "$CLEAR" ; then
	AC_DEFINE_UNQUOTED(CLEAR,"$CLEAR",[screen clear program])
fi

dnl ----------------------------------------------------------------------------
dnl -----------   C compiler checks    -----------------------------------------

AC_C_CONST
AC_C_INLINE
AC_C_VOLATILE

AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIME

dnl typedefs:
AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_TYPE_SIZE_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T

AC_CHECK_TYPES( fd_set,,,[
#include <sys/types.h>
#include <sys/time.h>
#include <sys/unistd.h>
] )

AC_CHECK_TYPES( socklen_t,,,[
#include <sys/types.h>
#include <sys/socket.h>
] )

checklibs=

dnl ----------------------------------------------------------------------------
dnl headers:

AC_CHECK_HEADERS(arpa/inet.h arpa/nameser.h assert.h com_err.h compat.h ctype.h ctypes.h dirent.h errno.h fcntl.h filehdr.h grp.h limits.h locale.h machine/vmparam.h malloc.h memory.h ndir.h netdb.h netinet/in.h pwd.h resolv.h select.h setjmp.h sgtty.h signal.h stab.h stdarg.h stdio.h stdlib.h string.h strings.h sys/dir.h sys/exec.h sys/fcntl.h sys/file.h sys/ioctl.h sys/mount.h sys/ndir.h sys/param.h sys/pstat.h sys/resource.h sys/select.h sys/signal.h sys/socket.h sys/stat.h sys/statfs.h sys/statvfs.h sys/syslog.h sys/systeminfo.h sys/termio.h sys/termiox.h sys/time.h sys/ttold.h sys/ttycom.h sys/types.h sys/utsname.h sys/vfs.h sys/wait.h syslog.h term.h termcap.h termio.h termios.h time.h unistd.h utsname.h varargs.h vmparam.h endian.h stdint.h)

dnl ----------------------------------------------------------------------------
dnl libraries:

dnl it might be nice to put them to some SOCKETLIBS or stuff like that instead,
dnl but given it is not needed on Linux, this will both seldom tested and
dnl seldom be useful.

dnl A/UX uses this...
AC_SEARCH_LIBS(getpwent, posix)

dnl TODO: error out if not there?

AC_SEARCH_LIBS(gethostbyaddr, [nsl_s nsl net_s net])

AC_SEARCH_LIBS(socket, [socket_s socket])

	dnl BIND library may be needed,  need to force this first
	if test -z "$no_resolv_lib"; then
	  AC_CHECK_FUNC(inet_ntop,name2=yes)
	  if test -z "$name2" ; then
		AC_CHECK_LIB(resolv, inet_ntop, [LIBS="$LIBS -lresolv";name2=yes])
	  fi;
	fi

dnl ----------------------------------------------------------------------------
dnl function checks:

dnl BSDs have this:
AC_CHECK_LIB(util, setproctitle, [LIBS="-lutil $LIBS"])

AC_CHECK_FUNCS(_res cfsetispeed fcntl flock gethostbyname2 getdtablesize gethostname getrlimit inet_aton inet_ntop inet_pton innetgr initgroups killpg lockf mkstemp mktemp openlog putenv random rand setenv seteuid setgroups setlocale setpgid setproctitle setresuid setreuid setruid setsid sigaction sigprocmask siglongjmp socketpair strcasecmp strchr strdup strerror strncasecmp sysconf sysinfo tcdrain tcflush tcsetattr uname unsetenv wait3 waitpid)

if test ! "$ac_cv_func_setreuid" = yes -a ! "$ac_cv_func_seteuid" = yes -a ! "$ac_cv_func_setresuid" = yes; then
	AC_MSG_WARN([missing setreuid(), seteuid(), and setresuid()])
fi

AC_FUNC_VFORK
AC_FUNC_VPRINTF


dnl ----------------------------------------------------------------------------
dnl special system checks

AC_CACHE_CHECK(how to manipulate tty attributes,
ac_cv_struct_term,
[

if test "$ac_cv_header_termios_h" = yes; then
	ac_cv_struct_term=termios
fi
if test "$ac_cv_header_sys_termios_h" = yes; then
	ac_cv_struct_term=termios
fi

dnl test to see if we need to compile
if test -z "$ac_cv_struct_term" ; then
AC_TRY_COMPILE([
#ifdef HAVE_TERMIO_H
#include <termio.h>
#endif
#ifdef HAVE_SYS_TERMIO_H
#include <sys/termio.h>
#endif],[struct termio t;t.c_iflag = 0],
ac_cv_struct_term=termio)
fi

dnl now you have determined if you have termio
if test -z "$ac_cv_struct_term" ; then
    AC_TRY_COMPILE([#include <sgtty.h>],[
    struct sgttyb s;s.sg_flags = 0],
    ac_cv_struct_term=sgttyb)
fi
if test -z "$ac_cv_struct_term" ; then
	ac_cv_struct_term=UNDEFINED
fi
])
if test "$ac_cv_struct_term" = "sgttyb"; then
	AC_DEFINE(USE_SGTTYB,1,[use sgttyb])
	AC_DEFINE(USE_STTY,SGTTYB,[use sgttyb])
fi
if test "$ac_cv_struct_term" = "termio"; then
	AC_DEFINE(USE_TERMIO,1,[use termio])
	AC_DEFINE(USE_STTY,TERMIO,[use termio])
fi
if test "$ac_cv_struct_term" = "termios"; then
	AC_DEFINE(USE_TERMIOS,1,[use termios])
	AC_DEFINE(USE_STTY,TERMIOS,[use termios])
	if test "$ac_cv_header_sys_termiox_h" = yes; then
		AC_DEFINE(USE_TERMIOX,1,[use termiox])
	fi
fi

dnl ----------------------------------------------------------------------------

dnl test to see if lseek has a prototype - you make it get an error

AC_CACHE_CHECK(checking for lseek prototype,
ac_cv_lseek_proto,
[
AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_UNISTD_H
#include <sys/unistd.h>
#endif
],[off_t x; x = lseek(0,"test",SEEK_SET);],
ac_cv_lseek_proto=no, ac_cv_lseek_proto=yes)
])
if test "$ac_cv_lseek_proto" = yes; then
  AC_DEFINE(HAVE_LSEEK_PROTO,1,[have lseek definition])
fi

dnl ----------------------------------------------------------------------------

AC_CACHE_CHECK(how to get filesystem free space,
ac_cv_struct_fstype,
[
fstype=

dnl do this check if statvfs is a valid function
if test "$ac_cv_func_statvfs" != no ; then		#{
  AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif],[struct statvfs s; statvfs ("/", &s); return(s.f_bavail+s.f_bsize)],
  fstype=statvfs)
fi							#}

dnl do these checks if statfs is a valid function

if test "$ac_cv_func_statfs" != no ; then		#{
  if test -z "$fstype" ; then				#{
    AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STATFS_H
# include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_STATVFS_H
# include <sys/statvfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
# include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
# include <sys/vfs.h>
#endif],[struct fs_data s; return(s.fd_bfree+s.fd_bsize)],
    fstype=Ultrix-statfs)
  fi							#}

  if test -z "$fstype" ; then				#{
    AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif],[struct statfs s; return(s.f_bavail+s.f_bsize)],
    fstype=statfs)
  fi							# }

  if test -z "$fstype" ; then				# {
    AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/param.h>
#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif],[struct statfs s; return(s.f_bfree+s.f_bsize)],
    fstype=SVR3-statfs)
  fi							# }
fi							# }

if test -z "$fstype" ; then
    echo "cannot find a valid statfs-like structure!"
	fstype=UNKNOWN
fi

ac_cv_struct_fstype=$fstype

])

if test "$ac_cv_struct_fstype" = SVR3-statfs; then
	AC_DEFINE(USE_STATFS_TYPE,SVR3_STATFS,[svr3 statfs])
fi
if test "$ac_cv_struct_fstype" = Ultrix-statfs; then
	AC_DEFINE(USE_STATFS_TYPE,ULTRIX_STATFS,[ultix statfs])
fi
if test "$ac_cv_struct_fstype" = statfs; then
	AC_DEFINE(USE_STATFS_TYPE,STATFS,[plain statfs])
fi
if test "$ac_cv_struct_fstype" = statvfs; then
	AC_DEFINE(USE_STATFS_TYPE,STATVFS,[statvfs statfs])
fi

dnl ----------------------------------------------------------------------------
dnl

AC_CACHE_CHECK(for setproctitle declaration,
ac_cv_decl_setproctitle_def,
[AC_TRY_COMPILE([
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
],[setproctitle(0);],
ac_cv_decl_setproctitle_def=no, ac_cv_decl_setproctitle_def=yes)
])
if test "$ac_cv_decl_setproctitle_def" = yes; then
    AC_DEFINE(HAVE_SETPROCTITLE_DEF,1,[setproctitle defined])
fi


dnl ----------------------------------------------------------------------------
dnl sys_siglist array (list of signals)



AC_CACHE_CHECK(for sys_siglist array,
ac_cv_sys_siglist,
[AC_TRY_LINK([
#include <stdio.h>],
[extern int sys_siglist; printf("%d",sys_siglist);],
ac_cv_sys_siglist=yes, ac_cv_sys_siglist=no)
])
if test "$ac_cv_sys_siglist" = yes; then
    AC_DEFINE(HAVE_SYS_SIGLIST,1,[have sys_syslist])
	AC_CACHE_CHECK(for sys_siglist declaration,
	ac_cv_decl_sys_siglist_def,
	[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <signal.h>],
	[printf("%s",sys_siglist[0]);],
	ac_cv_decl_sys_siglist_def=yes, ac_cv_decl_sys_siglist_def=no)
	])
	if test "$ac_cv_decl_sys_siglist_def" = yes; then
	    AC_DEFINE(HAVE_SYS_SIGLIST_DEF,1,[sys_siglist defined])
		ac_cv_sys_siglist=yes
	fi
else
AC_CACHE_CHECK(for _sys_siglist array,
ac_cv__sys_siglist,
[AC_TRY_LINK([
#include <stdio.h>],
[extern int _sys_siglist; printf("%d",_sys_siglist);],
ac_cv__sys_siglist=yes, ac_cv__sys_siglist=no)
])
if test "$ac_cv__sys_siglist" = yes; then
    AC_DEFINE(HAVE__SYS_SIGLIST,1,[have _sys_siglist])
	AC_CACHE_CHECK(for _sys_siglist declaration,
	ac_cv_decl__sys_siglist_def,
	[AC_TRY_COMPILE([
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SIGNAL_H
#include <sys/signal.h>
#endif
#include <signal.h>],
	[printf("%s",_sys_siglist[0]);],
	ac_cv_decl__sys_siglist_def=yes, ac_cv_decl__sys_siglist_def=no)
	])
	if test "$ac_cv_decl__sys_siglist_def" = yes; then
	    AC_DEFINE(HAVE__SYS_SIGLIST_DEF,1,[_sys_siglist defined])
		ac_cv__sys_siglist=yes
	fi
fi
fi

dnl ----------------------------------------------------------------------------
dnl just for (really) backwards compatibility
dnl we really try not to use union wait -- it's heinously unportable.
dnl nicked this check from Tcl as well. ;
dnl
dnl The check below checks whether <sys/wait.h> defines the type
dnl "union wait" correctly.  It's needed because of weirdness in
dnl HP-UX where "union wait" is defined in both the BSD and SYS-V
dnl environments.  Checking the usability of WIFEXITED seems to do
dnl the trick.

AC_CACHE_CHECK(for obsolete union wait compatibility,
ac_cv_unionwait,
[
AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/wait.h>], [union wait x;WIFEXITED(x);],
ac_cv_unionwait=yes, ac_cv_unionwait=no)
])
if test "$ac_cv_unionwait" = yes; then
	AC_DEFINE(HAVE_UNION_WAIT,1,[have union wait])
fi

dnl ----------------------------------------------------------------------------
dnl Would you believe the gethostname declarations are broken on some machines
dnl ----------------------------------------------------------------------------
AC_CACHE_CHECK(for gethostname declaration,
ac_cv_decl_gethostname_def,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif],[gethostname(1);],
ac_cv_decl_gethostname_def=no, ac_cv_decl_gethostname_def=yes)
])
if test "$ac_cv_decl_gethostname_def" = yes; then
    AC_DEFINE(HAVE_GETHOSTNAME_DEF,1,[have gethostname definition])
fi

dnl ----------------------------------------------------------------------------
dnl innetgr() declarations missing
dnl ----------------------------------------------------------------------------
AC_CACHE_CHECK(for innetgr declaration,
ac_cv_decl_innetgr_def,
[
AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif],[printf("%d", innetgr(1));],
ac_cv_decl_innetgr_def=no, ac_cv_decl_innetgr_def=yes )
]
)
if test "$ac_cv_decl_innetgr_def" = yes; then
  AC_DEFINE(HAVE_INNETGR_DEF,1,[use innetgr(1)])
fi

dnl ----------------------------------------------------------------------------
dnl openlog() declarations missing
dnl ----------------------------------------------------------------------------
AC_CACHE_CHECK(for openlog declaration,
ac_cv_decl_openlog_def,
[AC_TRY_COMPILE([
#include <stdio.h>
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif],[printf("%d",openlog);],
ac_cv_decl_openlog_def=yes, ac_cv_decl_openlog_def=no)
])
if test "$ac_cv_decl_openlog_def" = yes; then
    AC_DEFINE(HAVE_OPENLOG_DEF,1,[have openlog definition])
fi


dnl ----------------------------------------------------------------------------
dnl syslog() declarations missing
dnl ----------------------------------------------------------------------------
AC_CACHE_CHECK(for syslog declaration,
ac_cv_decl_syslog_def,
[AC_TRY_COMPILE([
#include <stdio.h>
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif],[printf("%d",syslog);],
ac_cv_decl_syslog_def=yes, ac_cv_decl_syslog_def=no)
])
if test "$ac_cv_decl_syslog_def" = yes; then
    AC_DEFINE(HAVE_SYSLOG_DEF,1,[have syslog definition])
fi

dnl ----------------------------------------------------------------------------
dnl IPV6 - check for structure declarations
dnl ----------------------------------------------------------------------------

AC_CACHE_CHECK(for struct in6_addr declaration,
ac_cv_decl_in6_addr_def,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>],[struct in6_addr v;],
ac_cv_decl_in6_addr_def=yes, ac_cv_decl_in6_addr_def=no)
])
if test "$ac_cv_decl_in6_addr_def" = yes; then
    AC_DEFINE(IN6_ADDR,1,[in6_addr defined])
    AC_DEFINE(IPV6,1,[ipV6])
fi


AC_CACHE_CHECK([for struct in_addr6 declaration (LINUX)],
ac_cv_decl_in_addr6_def,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>],[struct in_addr6 v;],
ac_cv_decl_in_addr6_def=yes, ac_cv_decl_in_addr6_def=no)
])
if test "$ac_cv_decl_in_addr6_def" = yes; then
    AC_DEFINE(IN_ADDR6,1,[in_addr6 defined])
    AC_DEFINE(IPV6,1,[ipV6])
fi

dnl ----------------------------------------------------------------------------
dnl struct stat can have a st_mtime_nsec field
dnl ----------------------------------------------------------------------------


AC_CACHE_CHECK(for struct stat has st_mtimespec.tv_nsec,
ac_cv_decl_st_mtimespec_tv_nsec,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>],[struct stat statb; statb.st_mtimespec.tv_nsec;],
ac_cv_decl_st_mtimespec_tv_nsec=yes,
ac_cv_decl_st_mtimespec_tv_nsec=no)
])
if test "$ac_cv_decl_st_mtimespec_tv_nsec" = yes; then
    AC_DEFINE_UNQUOTED(ST_MTIMESPEC_TV_NSEC,1,[stat st_mtimespec.tv_nsec present])
fi

AC_CACHE_CHECK(for struct stat has st_mtimensec,
ac_cv_decl_st_mtimensec,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#include <sys/types.h>
#if defined(HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif
#include <sys/stat.h>],[struct stat statb; statb.st_mtimensec;],
ac_cv_decl_st_mtimensec=yes,
ac_cv_decl_st_mtimensec=no)
])
if test "$ac_cv_decl_st_mtimensec" = yes; then
    AC_DEFINE_UNQUOTED(ST_MTIMENSEC,1,[struct st_mtimensec present])
fi

dnl ----------------------------------------------------------------------------

AC_CACHE_CHECK(for strcasecmp definition,
ac_cv_decl_strcasecmp,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if defined(HAVE_STRING_H)
#include <string.h>
#endif
#if defined(HAVE_STRINGS_H)
#include <strings.h>
#endif
],[strcasecmp(1)],
ac_cv_decl_strcasecmp=no,
ac_cv_decl_strcasecmp=yes)
])
if test "$ac_cv_decl_strcasecmp" = yes; then
    AC_DEFINE_UNQUOTED(HAVE_STRCASECMP_DEF,1,[have strcasecmp definition])
fi

dnl ----------------------------------------------------------------------------

AC_CACHE_CHECK(for flock definition,
ac_cv_decl_flock,
[AC_TRY_COMPILE([
#ifdef HAVE_CTYPES_H
#include <ctypes.h>
#endif
#if defined(HAVE_STDLIB_H)
#include <stdlib.h>
#endif
#if defined(HAVE_SYS_FILE_H)
#include <sys/file.h>
#endif
#if defined(HAVE_FCNTL_H)
#include <fcntl.h>
#endif
],[flock(1)],
ac_cv_decl_flock=no,
ac_cv_decl_flock=yes)
])
if test "$ac_cv_decl_flock" = yes; then
    AC_DEFINE_UNQUOTED(HAVE_FLOCK_DEF,1,[have flock definition])
fi

dnl ----------------------------------------------------------------------------

AC_PROG_INSTALL
AC_CHECK_PROG(INSTALL_MAN,auxman,auxman,[$INSTALL -m 644])

AC_PROG_MAKE_SET

dnl ----------------- force this to be Bourne Shell for now ---------------
AC_MSG_CHECKING(for shell)
SHELL=/bin/sh
AC_MSG_RESULT(using $SHELL (FORCED))

MY_GETTEXT

dnl ----------------------------------------------------------------------------
dnl ----------------- END OF GENERAL CONFIGURATION   ---------------------------

dnl check to see if tcp wrappers required
AC_MSG_CHECKING(use tcp wrappers)
AC_ARG_ENABLE( tcpwrappers,
AS_HELP_STRING([--enable-tcpwrappers],[use tcp wrappers (-lwrap)]),
[
if test "$enableval" = "yes" ; then
 v=yes
 AC_CHECK_HEADERS(tcpd.h,
  [AC_DEFINE(HAVE_TCPD_H,1,[have tcpd])],
  [AC_MSG_ERROR(tcpd.h not found);
   v=no;
  ]
 )
 AC_CHECK_LIB(wrap, request_init,
  [LIBS="-lwrap $LIBS"],
  [AC_MSG_ERROR(request_init not found in -wrap library);
   v=no;
  ]
 )
else
 v=no;
fi
],
[
 v=no;
]
)

if test "$v" = "yes" ; then
	 AC_DEFINE(TCPWRAPPERS,1,[tcpwrappers])
fi
AC_MSG_RESULT($v);dnl

dnl check to see if ssl is disabled
SSL_EXPLICITLY_ENABLED=""
AC_MSG_CHECKING(if ssl authentication is disabled)
AC_ARG_ENABLE( ssl,
AS_HELP_STRING([--disable-ssl],[disable ssl support]),
[
if test "$enableval" = "yes" ; then
	v=enabled;  SSL_ENABLE=1;
	SSL_EXPLICITLY_ENABLED=1;
else
	v=disabled; SSL_ENABLE=;
fi
],
[
	v=enabled;  SSL_ENABLE=1;
],
)
AC_MSG_RESULT($v);dnl

dnl This does far too much by hand, try to just check for
dnl the headers and libs...

if test "$SSL_ENABLE" != ""; then
	AC_CHECK_HEADER([openssl/ssl.h], [true], [SSL_ENABLE=""])
fi

SSL_LDADD=""
if test "$SSL_ENABLE" != ""; then
	AC_CHECK_LIB([crypto], [RC4_set_key], [true], [SSL_ENABLE=""])
fi
if test "$SSL_ENABLE" != ""; then
	AC_CHECK_LIB([ssl], [SSL_load_error_strings], [true], [SSL_ENABLE=""], [-lcrypto])
fi
if test "$SSL_ENABLE" != ""; then
	SSL_LDADD="-lssl -lcrypto"
elif test "$SSL_EXPLICITLY_ENABLED" != ""; then
	AC_MSG_ERROR([Unable to use OpenSSL])
fi
AC_SUBST(SSL_LDADD)
AM_CONDITIONAL(WITHSSL, test "$SSL_ENABLE" != "")
test "$SSL_ENABLE" != "" && AC_DEFINE(SSL_ENABLE,1,[ssl enabled])


dnl  ssl certificate authority CERT file
dnl
AC_MSG_CHECKING(ssl Certificate Authority CERT file)
AC_ARG_WITH(ssl_ca_file,
AS_HELP_STRING([--with-ssl_ca_file=FILE],[ssl Certificate Authority CERT file (default ${configdir}/ssl.ca/ca.crt)]),
SSL_CA_FILE=$withval,
SSL_CA_FILE=\${configdir}/ssl.ca/ca.crt,
)
AC_MSG_RESULT($SSL_CA_FILE)
AC_SUBST(SSL_CA_FILE)

dnl  ssl certificate authority private key file
dnl
AC_MSG_CHECKING(ssl Certificate Authority private key file)
AC_ARG_WITH(ssl_ca_key,
AS_HELP_STRING([--with-ssl_ca_key=KEY],[ssl Certificate Authority private key file (default ${configdir}/ssl.ca/ca.key)]),
SSL_CA_KEY=$withval,
SSL_CA_KEY=\${configdir}/ssl.ca/ca.key,
)
AC_MSG_RESULT($SSL_CA_KEY)
AC_SUBST(SSL_CA_KEY)

dnl  ssl certificate authority certs working directory
dnl
AC_MSG_CHECKING(ssl Certificate Authority certs working directory)
AC_ARG_WITH(ssl_certs_dir,
AS_HELP_STRING([--with-ssl_certs_dir=DIR],[ssl Certificate Authority certs working directory (default ${configdir}/ssl.certs/)]),
SSL_CERTS_DIR=$withval,
SSL_CERTS_DIR=\${configdir}/ssl.certs,
)
AC_MSG_RESULT($SSL_CERTS_DIR)
AC_SUBST(SSL_CERTS_DIR)

dnl  ssl certificate revocation list
dnl
AC_MSG_CHECKING(ssl Certificate Revocation List (CRL) file)
AC_ARG_WITH(ssl_crl_file,
AS_HELP_STRING([--with-ssl_crl_file=PATH],[ssl Certificate Revocation List File (default ${configdir}/ssl.crl/ssl.crl)]),
SSL_CRL_FILE=$withval,
SSL_CRL_FILE=\${configdir}/ssl.crl/ssl.crl,
)
AC_MSG_RESULT($SSL_CRL_FILE)
AC_SUBST(SSL_CRL_FILE)

dnl  ssl server certificate file
dnl
AC_MSG_CHECKING(ssl server certificate file)
AC_ARG_WITH(ssl_server_cert,
AS_HELP_STRING([--with-ssl_server_cert=FILE],[ssl server certificate file (default ${configdir}/ssl.server/server.crt)]),
SSL_SERVER_CERT=$withval,
SSL_SERVER_CERT=\${configdir}/ssl.server/server.crt,
)
AC_MSG_RESULT($SSL_SERVER_CERT)
AC_SUBST(SSL_SERVER_CERT)

dnl  ssl server password file for private key file
dnl
AC_MSG_CHECKING(ssl server password file for private key file)
AC_ARG_WITH(ssl_server_password_file,
AS_HELP_STRING([--with-ssl_server_password_file=FILE],[ssl server private key in password file (default ${configdir}/ssl.server/server.pwd)]),
SSL_SERVER_PASSWORD_FILE=$withval,
SSL_SERVER_PASSWORD_FILE=\${configdir}/ssl.server/server.pwd,
)
AC_MSG_RESULT($SSL_SERVER_PASSWORD_FILE)
AC_SUBST(SSL_SERVER_PASSWORD_FILE)

dnl ----------------------------------------------------------------------------
dnl ---------------- START OF KERBEROS -----------------------------------------

dnl Kerberos stuff again

KRB_LIBS=""
SAVELIBS="$LIBS"


if test -n "$KERBEROS" ; then
	echo "Kerberos checks with CFLAGS '$CFLAGS', CPPFLAGS '$CPPFLAGS',  LDFLAGS '$LDFLAGS"
	if test -d /usr/kerberos/include; then
		CPPFLAGS="$CPPFLAGS -I/usr/kerberos/include"
	fi
	if test -d /usr/kerberos/lib; then
		LDFLAGS="-L/usr/kerberos/lib $LDFLAGS"
	fi
	dnl Kerberos 5 - release 1.1.1
	found=
	AC_CHECK_HEADERS(krb5.h,found=yes)
    if test "$found" != "yes" ; then
		  AC_MSG_ERROR([
Kerberos 5 support wanted and cannot find krb5.h include file
use configure --disable-kerberos-checks or check your Kerberos 5 installation
If you have installed kerberos in /usr/local/{lib,include} then use
configure --enable-kerberos CPPFLAGS=-I/usr/local/include LDFLAGS=-L/usr/local/lib
])
	fi

	if test "$KERBEROS_CHECKS" = 1 ; then
		dnl we have the Kerberos 5 package, lets try for the rest
		AC_TRY_COMPILE([
		#include <stdio.h>
		#include <krb5.h>
		], [printf("%d",sizeof(HostAddress));],
		heimdal=yes, heimdal=no)
		if test $heimdal = yes; then
		    AC_MSG_WARN(compiling with CFLAGS $CFLAGS, CPPFLAGS $CPPFLAGS)
			AC_MSG_ERROR(You appear to be using the Heimdal Kerberos krb5.h file. You must use MIT Kerberos with LPRng)
		fi

		dnl we check for the library we need to use
		dnl courtesy of mandrake linux and friends.
		dnl  Christian Zoffoli <czoffoli@linux-mandrake.com>

		found=no
		if test "$found" "!=" "yes" ; then
			found=yes
			KRB_LIBS="-lkrb5 -lk5crypto -lcom_err"
			LIBS=" $KRB_LIBS $SAVELIBS"
			AC_CHECKING(for krb5_init_context in $LIBS )
			AC_TRY_LINK_FUNC(krb5_init_context,,found="no")
			AC_CHECKING(for krb5_encrypt_size in $LIBS )
			AC_TRY_LINK_FUNC(krb5_encrypt_size,,found="no")
		fi;
		if test "$found" "!=" "yes" ; then
			found=yes
			KRB_LIBS="-lkrb5 -lcrypto -lcom_err"
			LIBS=" $KRB_LIBS $SAVELIBS"
			AC_CHECKING(for krb5_init_context in $LIBS )
			AC_TRY_LINK_FUNC(krb5_init_context,,found="no")
			AC_CHECKING(for krb5_encrypt_size in $LIBS )
			AC_TRY_LINK_FUNC(krb5_encrypt_size,,found="no")
		fi;
		if test "$found" "!=" "yes" ; then
			AC_PATH_PROG(KRB5CONFIG, krb5-config)
			found=yes
			KRB_LIBS="$($KRB5CONFIG --libs krb5)"
			LIBS=" $KRB_LIBS $SAVELIBS"
			AC_CHECKING(for krb5_init_context in $LIBS )
			AC_TRY_LINK_FUNC(krb5_init_context,,found="no")
			AC_CHECKING(for krb5_encrypt_size in $LIBS )
			AC_TRY_LINK_FUNC(krb5_encrypt_size,,found="no")
		fi;
		if test "$found" "!=" "yes" ; then
			  AC_MSG_WARN(Kerberos 5 library does not have krb5_init_context and krb5_encrypt_size or )
			  AC_MSG_WARN(use configure --disable-kerberos or check your Kerberos 5 installation)
			  exit 1
        fi
		AC_MSG_RESULT(found in $LIBS)

		AC_CHECK_FUNC(krb5_read_message,AC_DEFINE_UNQUOTED(HAVE_KRB5_READ_MESSAGE,1,[have krb5_read_message]),[AC_MSG_WARN(Kerberos 5 library does not have krb5_read_message)])

	fi

	AC_CHECK_FUNCS(krb5_free_data_contents)

	AC_CACHE_CHECK(for krb5_xfree,
	ac_cv_krb5_xfree,
	[AC_TRY_LINK([
	# define KRB5_DEPRECATED 1
	#if defined(HAVE_KRB5_H)
	#include <krb5.h>
	#endif
	],[krb5_xfree((void *)0);],
	ac_cv_krb5_xfree=yes,
	ac_cv_krb5_xfree=no)
	])
	if test "$ac_cv_krb5_xfree" = yes; then
		AC_DEFINE_UNQUOTED(HAVE_KRB5_XFREE,1,[have krb5_xfree])
	fi

	AC_CACHE_CHECK(for krb_xfree,
	ac_cv_krb_xfree,
	[AC_TRY_LINK([
	# define KRB5_DEPRECATED 1
	#if defined(HAVE_KRB5_H)
	#include <krb5.h>
	#endif
	],[krb_xfree((void *)0);],
	ac_cv_krb_xfree=yes,
	ac_cv_krb_xfree=no)
	])
	if test "$ac_cv_krb_xfree" = yes; then
		AC_DEFINE_UNQUOTED(HAVE_KRB_XFREE,1,[have krb_xfree])
	fi

	LIBS="$SAVELIBS"
fi;
AC_SUBST(KRB_LIBS)

dnl ----------------------------------------------------------------------------
dnl ---------------- END OF KERBEROS -----------------------------------------

CHECK_PLUGINS

dnl ----------------------------------------------------------------------------
dnl ----------------- START OF OUTPUT ------------------------------------------

AC_OUTPUT(
[
Makefile
UTILS/LPRng.pm
UTILS/Makefile
UTILS/accounting.pl
UTILS/decode_args_with_perl
UTILS/decode_args_with_sh
UTILS/fixid
UTILS/fixupdate
UTILS/lpq_in_perl
UTILS/lpr_in_perl
UTILS/lprm_in_perl
UTILS/make_lpd_conf
UTILS/make_printcap_use
UTILS/makeinc
UTILS/read_conf
UTILS/remote_active
UTILS/test_read
UTILS/update_z.pl
man/Makefile
src/Makefile
src/pclbanner
src/psbanner
po/Makefile
conf/Makefile
conf/lpd.perms
],
[
for i in  \
UTILS/LPRng.pm \
UTILS/accounting.pl \
UTILS/decode_args_with_perl \
UTILS/decode_args_with_sh \
UTILS/fixid \
UTILS/fixupdate \
UTILS/lpq_in_perl \
UTILS/lpr_in_perl \
UTILS/lprm_in_perl \
UTILS/make_lpd_conf \
UTILS/make_printcap_use \
UTILS/makeinc \
UTILS/read_conf \
UTILS/remote_active \
UTILS/test_read \
UTILS/update_z.pl \
; do chmod +x $i ; done
]

)

dnl ----------------------------------------------------------------------------
dnl ----------------- END OF OUTPUT --------------------------------------------
