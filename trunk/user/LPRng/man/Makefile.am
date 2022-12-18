MAINTAINERCLEANFILES = Makefile.in
man_MANS = cancel.1 checkpc.8 lp.1 lpbanner.1 lpc.8 lpd.8 lpd.conf.5 \
	 lpd.perms.5 lpf.1 lpq.1 lpr.1 lprm.1 lpstat.1 pclbanner.1 \
	 printcap.5 psbanner.1 lprng_certs.1 lprng_index_certs.1
CLEANFILES = $(man_MANS) monitor.1
EXTRA_DIST = cancel.n checkpc.n lp.n lpbanner.n lpc.n lpd.n lpd.conf.n \
	 lpd.perms.n lpf.n lpq.n lpr.n lprm.n lpstat.n monitor.n pclbanner.n \
	 printcap.n psbanner.n  lprng_certs.n lprng_index_certs.n

%.1 %.5 %.8: $(srcdir)/%.n Makefile
	rm -f $@
	sed \
		 -e "s!_PRINTCAP_PATH_!${PRINTCAP_PATH}!" \
		 -e "s!_FILTER_LD_PATH_!${FILTER_LD_PATH}!" \
		 -e "s!_FILTER_PATH_!${FILTER_PATH}!" \
		 -e "s!_LPD_PERMS_PATH_!${LPD_PERMS_PATH}!" \
		 -e "s!_LPD_CONF_PATH_!${LPD_CONF_PATH}!" \
		 -e "s!_LOCKFILE_!${LOCKFILE}!" \
		 -e "s!_SSL_CA_FILE_!${SSL_CA_FILE}!" \
		 -e "s!_SSL_CA_KEY_!${SSL_CA_KEY}!" \
		 -e "s!_SSL_CERTS_DIR_!${SSL_CERTS_DIR}!" \
		 -e "s!_SSL_SERVER_CERT_!${SSL_SERVER_CERT}!" \
		 -e "s!_SSL_SERVER_PASSWORD_!${SSL_SERVER_PASSWORD}!" \
		 -e "s!_LPD_LISTEN_PORT_!${LPD_LISTEN_PORT}!" \
		$< > $@

