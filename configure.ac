AC_PREREQ([2.69])
AC_INIT([xfce4-finder], [1.0], [j.graves@collaboradev.com])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE
AC_CONFIG_SRCDIR([src/xfce4-finder.cc])

# Checks for programs.
AC_PROG_CXX

# Checks for libraries.
PKG_CHECK_MODULES([GLIB], [glib-2.0])
PKG_CHECK_MODULES([GIO], [gio-2.0])
PKG_CHECK_MODULES([GTKMM], [gtkmm-3.0])
PKG_CHECK_MODULES([GARCON], [garcon-1])
PKG_CHECK_MODULES([XFCONF], [libxfconf-0])
PKG_CHECK_MODULES([LIBXFCE4UTIL], [libxfce4util-1.0])
PKG_CHECK_MODULES([LIBXFCE4UI], [libxfce4ui-2])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
