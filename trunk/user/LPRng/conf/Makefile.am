# This directory contains the sample config files
# (except lpd.conf, which in generated in src/ so it
#  is in sync with the actual defaults compiled into
#  the programs)
# It also contains the init.* files, which are examples for
# init scripts to start LPRng at boot time.

# run make install with SAMPLESUFFIX="" to force files in their final position
# (which will overwrite everything else you have there, you have been warned)
SAMPLESUFFIX = .sample
# run make install with INSTALLCONFIGEXAMPLES=No to not install them at all
INSTALLCONFIGEXAMPLES = Yes

MAINTAINERCLEANFILES = Makefile.in
EXTRA_DIST = lpd.perms.in printcap \
	     init.freebsd init.generic init.linuxsb init.linux \
	     init.redhat init.solaris init.debian

install-data-hook: lpd.perms printcap
	$(mkinstalldirs) $(DESTDIR)$(configdir)
	if [ "$(INSTALLCONFIGEXAMPLES)" = "Yes" ] ; then \
	$(INSTALL_DATA) lpd.perms $(DESTDIR)$(LPD_PERMS_PATH)$(SAMPLESUFFIX) ; \
	$(INSTALL_DATA) $(srcdir)/printcap $(DESTDIR)$(PRINTCAP_PATH)$(SAMPLESUFFIX) ; \
	fi

# only uninstall if it is .sample, not otherwise to avoid surprises...
uninstall-hook:
	@if [ "$(INSTALLCONFIGEXAMPLES)" = "Yes" ] && [ "$(SAMPLESUFFIX)" = ".sample" ] ; then \
		echo "rm -f $(DESTDIR)$(LPD_PERMS_PATH)$(SAMPLESUFFIX) $(DESTDIR)$(PRINTCAP_PATH)$(SAMPLESUFFIX)" ; \
		rm -f "$(DESTDIR)$(LPD_PERMS_PATH)$(SAMPLESUFFIX)" "$(DESTDIR)$(PRINTCAP_PATH)$(SAMPLESUFFIX)" ; \
	fi
