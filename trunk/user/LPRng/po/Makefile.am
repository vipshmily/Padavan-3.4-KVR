# po/Makefile.am for LPRng by Bernhard R. Link in 2007
# this file is based on gettext-0.14.4' Makefile.in.in which states:
# # Makefile for PO directory in any package using GNU gettext.
# # Copyright (C) 1995-1997, 2000-2005 by Ulrich Drepper <drepper@gnu.ai.mit.edu>
#
# This file can be copied and used freely without restrictions.  It can
# be used in projects which are not available under the GNU General Public
# License but which still want to provide support for the GNU gettext
# functionality.
# Please note that the actual code of GNU gettext is covered by the GNU
# General Public License and is *not* in the public domain.

LINGUAS = de fr pl vi
# what is in the POTFILES.in file in gettextize-style:
# i.e. all files that include N_() or _()
# if a file is added to this list, touch it so this Makefile sees it.
POTFILES.in = \
	src/common/accounting.c\
	src/common/controlword.c\
	src/common/linelist.c\
	src/common/lpc.c\
	src/common/lpd.c\
	src/common/lpd_control.c\
	src/common/lpd_jobs.c\
	src/common/lpd_rcvjob.c\
	src/common/lpd_remove.c\
	src/common/lpd_secure.c\
	src/common/lpd_status.c\
	src/common/lpd_dispatch.c\
	src/common/lpq.c\
	src/common/lpstat.c\
	src/common/lpr.c\
	src/common/lprm.c\
	src/common/sendjob.c\
	src/common/sendmail.c\
	src/common/sendreq.c\
	src/common/getopt.c\
	src/vars.c
COPYRIGHT_HOLDER = Patrick Powell at al.
# This is the email address or URL to which the translators shall report
# bugs in the untranslated strings
# It can be your email address, or a mailing list address where translators
# can write to without being subscribed, or the URL of a web page through
# which the translators can contact you.
MSGID_BUGS_ADDRESS = $(PACKAGE_BUGREPORT)

DOMAIN = $(PACKAGE_NAME)
XGETTEXT_OPTIONS = --keyword=_ --keyword=N_

#######################################################################
gettextsrcdir = $(datadir)/gettext/po
POTFILES = $(patsubst %,$(top_srcdir)/%,$(POTFILES.in))
POFILES = $(patsubst %,$(srcdir)/%.po,$(LINGUAS))
GMOFILES = $(patsubst %,$(srcdir)/%.gmo,$(LINGUAS))
UPDATEPOFILES = $(patsubst %,%.po-update,$(LINGUAS))
DUMMYPOFILES = $(patsubst %,%.nop,$(LINGUAS))
MSGMERGE_UPDATE = @MSGMERGE@ --update
# This is kind of a regression the the gettextize Makefile.in.in:
# no update-po is run before dist, you have to do so manually
# (on the other hand, call it a feature, because this way one can
# run make dist more easily)
EXTRA_DIST = $(DOMAIN).pot stamp-po remove-potcdate.sin

# automake does not like patsubst in EXTRA_DIST, so copy manually:
dist-hook: $(POFILES) $(GMOFILES)
	cp -p $(POFILES) $(GMOFILES) $(distdir)/

%.mo: %.po
	@echo "$(MSGFMT) -c -o $@ $<"; \
	$(MSGFMT) -c -o t-$@ $< && mv t-$@ $@

%.gmo: %.po
	@lang=`echo $* | sed -e 's,.*/,,'`; \
	test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	echo "$${cdcmd}rm -f $${lang}.gmo && $(GMSGFMT) -c --statistics -o $${lang}.gmo $${lang}.po"; \
	cd $(srcdir) && rm -f $${lang}.gmo && $(GMSGFMT) -c --statistics -o t-$${lang}.gmo $${lang}.po && mv t-$${lang}.gmo $${lang}.gmo

%.sed: %.sin
	sed -e '/^#/d' $< > t-$@
	mv t-$@ $@

all: all-@USE_NLS@

all-yes: stamp-po
all-no:

# $(srcdir)/$(DOMAIN).pot is only created when needed. When xgettext finds no
# internationalized messages, no $(srcdir)/$(DOMAIN).pot is created (because
# we don't want to bother translators with empty POT files). We assume that
# LINGUAS is empty in this case, i.e. $(POFILES) and $(GMOFILES) are empty.
# In this case, stamp-po is a nop (i.e. a phony target).

# stamp-po is a timestamp denoting the last time at which the GMOFILES have
# been loosely updated. Its purpose is that when a developer or translator
# checks out the package via git, and the $(DOMAIN).pot file is not in git,
# "make" will update the $(DOMAIN).pot and the $(GMOFILES), but subsequent
# invocations of "make" will do nothing. This timestamp would not be necessary
# if updating the $(GMOFILES) would always touch them; however, the rule for
# $(POFILES) has been designed to not touch files that don't need to be
# changed.
stamp-po: $(srcdir)/$(DOMAIN).pot
	test -z "$(GMOFILES)" || $(MAKE) $(GMOFILES)
	@test ! -f $(srcdir)/$(DOMAIN).pot || { \
	  echo "touch stamp-po" && \
	  echo timestamp > stamp-poT && \
	  mv stamp-poT stamp-po; \
	}

# Note: Target 'all' must not depend on target '$(DOMAIN).pot-update',
# otherwise packages like GCC can not be built if only parts of the source
# have been downloaded.

# This target rebuilds $(DOMAIN).pot; it is an expensive operation.
# Note that $(DOMAIN).pot is not touched if it doesn't need to be changed.
$(DOMAIN).pot-update: $(POTFILES) remove-potcdate.sed
	$(XGETTEXT) --default-domain=$(DOMAIN) --directory="$(top_srcdir)" \
	  --add-comments=TRANSLATORS: $(XGETTEXT_OPTIONS) \
	  --copyright-holder='$(COPYRIGHT_HOLDER)' \
	  --msgid-bugs-address='$(MSGID_BUGS_ADDRESS)' \
	  $(POTFILES.in)
	test ! -f $(DOMAIN).po || { \
	  if test -f $(srcdir)/$(DOMAIN).pot; then \
	    sed -f remove-potcdate.sed < $(srcdir)/$(DOMAIN).pot > $(DOMAIN).1po && \
	    sed -f remove-potcdate.sed < $(DOMAIN).po > $(DOMAIN).2po && \
	    if cmp $(DOMAIN).1po $(DOMAIN).2po >/dev/null 2>&1; then \
	      rm -f $(DOMAIN).1po $(DOMAIN).2po $(DOMAIN).po; \
	    else \
	      rm -f $(DOMAIN).1po $(DOMAIN).2po $(srcdir)/$(DOMAIN).pot && \
	      mv $(DOMAIN).po $(srcdir)/$(DOMAIN).pot; \
	    fi; \
	  else \
	    mv $(DOMAIN).po $(srcdir)/$(DOMAIN).pot; \
	  fi; \
	}

# This rule has no dependencies: we don't need to update $(DOMAIN).pot at
# every "make" invocation, only create it when it is missing.
# Only "make $(DOMAIN).pot-update" or "make dist" will force an update.
$(srcdir)/$(DOMAIN).pot:
	$(MAKE) $(DOMAIN).pot-update

# This target rebuilds a PO file if $(DOMAIN).pot has changed.
# Note that a PO file is not touched if it doesn't need to be changed.
$(POFILES): $(srcdir)/$(DOMAIN).pot
	@lang=`echo $@ | sed -e 's,.*/,,' -e 's/\.po$$//'`; \
	if test -f "$(srcdir)/$${lang}.po"; then \
	  test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	  echo "$${cdcmd}$(MSGMERGE_UPDATE) $${lang}.po $(DOMAIN).pot"; \
	  cd $(srcdir) && $(MSGMERGE_UPDATE) $${lang}.po $(DOMAIN).pot; \
	else \
	  $(MAKE) $${lang}.po-create; \
	fi

install-data-local: install-data-local-@USE_NLS@

install-data-local-no: all
install-data-local-yes: all
	$(mkinstalldirs) $(DESTDIR)$(datadir)
	for lang in $(LINGUAS) ; do \
		cat=$$lang.gmo \
		dir=$(localedir)/$$lang/LC_MESSAGES; \
		$(mkinstalldirs) $(DESTDIR)$$dir; \
		if test -r $$cat; then realcat=$$cat; else realcat=$(srcdir)/$$cat; fi; \
	  	echo "installing $$realcat as $(DESTDIR)$$dir/$(DOMAIN).mo"; \
		$(INSTALL_DATA) $$realcat $(DESTDIR)$$dir/$(DOMAIN).mo; \
	done

uninstall-local: uninstall-local-@USE_NLS@
uninstall-local-no:
uninstall-local-yes:
	for lang in $(LINGUAS) ; do \
		rm -f $(DESTDIR)$(localedir)/$$lang/LC_MESSAGES/$(DOMAIN).mo ; \
	done

MOSTLYCLEANFILES = remove-potcdate.sed stamp-poT core core.* $(DOMAIN).po $(DOMAIN).1po $(DOMAIN).2po *.new.po
DISTCLEANFILES = *.mo
MAINTAINERCLEANFILES = Makefile.in stamp-po $(GMOFILES)

update-po: Makefile
	$(MAKE) $(DOMAIN).pot-update
	$(MAKE) $(UPDATEPOFILES)
	$(MAKE) $(GMOFILES)

# General rule for creating PO files.
%.po-create: %.nop
	@lang=`echo $@ | sed -e 's/\.po-create$$//'`; \
	echo "File $$lang.po does not exist. If you are a translator, you can create it through 'msginit'." 1>&2; \
	exit 1

# General rule for updating PO files.
%.po-update: %.nop
	@lang=`echo $@ | sed -e 's/\.po-update$$//'`; \
	tmpdir=`pwd`; \
	echo "$$lang:"; \
	test "$(srcdir)" = . && cdcmd="" || cdcmd="cd $(srcdir) && "; \
	echo "$${cdcmd}$(MSGMERGE) $$lang.po $(DOMAIN).pot -o $$lang.new.po"; \
	cd $(srcdir); \
	if $(MSGMERGE) $$lang.po $(DOMAIN).pot -o $$tmpdir/$$lang.new.po; then \
	  if cmp $$lang.po $$tmpdir/$$lang.new.po >/dev/null 2>&1; then \
	    rm -f $$tmpdir/$$lang.new.po; \
	  else \
	    if mv -f $$tmpdir/$$lang.new.po $$lang.po; then \
	      :; \
	    else \
	      echo "msgmerge for $$lang.po failed: cannot move $$tmpdir/$$lang.new.po to $$lang.po" 1>&2; \
	      exit 1; \
	    fi; \
	  fi; \
	else \
	  echo "msgmerge for $$lang.po failed!" 1>&2; \
	  rm -f $$tmpdir/$$lang.new.po; \
	fi

$(DUMMYPOFILES):

