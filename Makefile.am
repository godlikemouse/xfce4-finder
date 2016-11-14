AUTOMAKE_OPTIONS = subdir-objects foreign
ACLOCAL_AMFLAGS = $(ACLOCAL_FLAGS)

BUILT_SOURCES = glade.h default-css.h
CLEANFILES = src/glade.h src/default-css.h

glade.h:
	$(srcdir)/scripts/build-glade-h

default-css.h:
	$(srcdir)/scripts/build-default-css-h

bin_PROGRAMS = xfce4-finder
xfce4_finder_CPPFLAGS = $(GTKMM_CFLAGS) $(XFCONF_CFLAGS) $(GARCON_CFLAGS) $(LIBXFCE4UTIL_CFLAGS) $(LIBXFCE4UI_CFLAGS) $(GLIB_CFLAGS) $(GIO_CFLAGS)
xfce4_finder_LDADD = $(GTKMM_LIBS) $(XFCONF_LIBS) $(GARCON_LIBS) $(LIBXFCE4UTIL_LIBS) $(LIBXFCE4_LIBS) $(GLIB_LIBS) $(GIO_LIBS)
xfce4_finder_SOURCES = config.h \
src/glade.h \
src/default-css.h \
src/custom-css.h \
src/xfce4-finder.cc


