MAINTAINERCLEANFILES = Makefile.in

CONFIGURE_GENERATED_FILES = LPRng.pm accounting.pl decode_args_with_perl decode_args_with_sh fixid fixupdate lpq_in_perl lpr_in_perl lprm_in_perl make_lpd_conf make_printcap_use makeinc read_conf remote_active test_read update_z.pl

DISTCLEANFILES = $(CONFIGURE_GENERATED_FILES)
EXTRA_DIST = $(patsubst %,%.in,$(CONFIGURE_GENERATED_FILES)) $(UNCHECKEDSTUFF)

# TODO: decide which to include, which to remove:
UNCHECKEDSTUFF = atalkprint chooser.in extract_pjl freefs.c linetest.c ncpprint one.pcl one.pjl ps.draft README.ForKerberosHackers set_file_time.c smbprint tcpsend.c termcap.c testpr test_rw_pipe.c VeryFlexibleChooser.pl xlate.c
