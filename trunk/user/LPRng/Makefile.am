
# still to include: TESTSUPPORT DISTRIBUTIONS
SUBDIRS = UTILS po src man conf

CLEANFILES = a.out *.bak ? ?.* core *.old *~ autom4te
DISTCLEANFILES = *.orig configure.lineno config.cache config.log config.status
MAINTAINERCLEANFILES = configure Makefile.in \
		       config.rpath config.guess config.sub \
		       aclocal.m4 config.h.in \
		       depcomp install-sh ltmain.sh missing mkinstalldirs
EXTRA_DIST = ABOUT-NLS.LPRng CHANGES autogen.sh\
             KERBEROS_configuration STANDARD_configuration MIT_configure \
	     CONTRIBUTORS COPYRIGHT LICENSE README.SSL.SECURITY

update-po:
	(cd po && $(MAKE) $(AM_MAKEFLAGS) update-po) || exit 1

# ###############################################################################
# # Update the patch level when you make a new version
# # do this before you start changes
# # Don't even think about making this configurable, it is for
# # distribution and update purposes only!
# #  Patrick Powell
# ###############################################################################
#
# update:
# 	rm -f src/include/license.h src/include/copyright.h
# 	sed -e 's/"/\\"/g' -e 's/.*/"&",/' LICENSE >src/include/license.h
# 	sed -e 's/"/\\"/g' -e 's/.*/"&",/' COPYRIGHT >src/include/copyright.h
# 	for i in VERSION ./src/include/patchlevel.h configure.in ; do \
# 		rcs -l $$i; chmod +w $$i; \
# 	done;
# 	if [ -x /bin/pwd ] ; then DIR=`/bin/pwd`; fi ; \
# 	if [ -x /usr/bin/pwd ] ; then DIR=`/usr/bin/pwd`; fi ; \
# 	DIR=`echo $${DIR} | sed 's,.*/,,'`; \
# 	DIRVER=` echo $${DIR} | sed 's,.*-,,'`; \
# 	echo DIR $${DIR}, DIRVER $${DIRVER}; \
# 	echo "#define PATCHLEVEL \"$${DIR}\"" >./src/include/patchlevel.h; \
# 	echo $${DIR} >VERSION; \
# 	S=`echo *.sh | sed -e 's/\.sh//g'`; \
# 	perl -spi -e "s,=.*,=$${DIRVER}, if(/^VERSION=/ or /^#.* VERSION=/); \
# 		s,^DISTNAME=.*,DISTNAME=$${DIR},; \
# 		s,^PORTNAME=.*,PORTNAME=$(PACKAGE),; \
# 		s,^PORTVERSION=.*,PORTVERSION=$${DIRVER},; \
# 		s,package name \".*\",package name \"$${DIR}\",; \
# 		s,^SCRIPTS=.*,SCRIPTS=$$S,;" \
# 		configure.in lpd.perms.in \
# 		DISTRIBUTIONS/*/Makefile \
# 		po/Makefile.in.in printcap
# 	perl -spi -e 's,.*,"Project-Id-Version: $(PACKAGE) $(VERSION)\\n", if(/^"Project-Id/);' \
# 		po/*.po
