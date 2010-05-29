
AM_CFLAGS = $(GTK_CFLAGS) -I$(top_srcdir)/src

if MINGW32
WIN32_LDFLAGS = -no-undefined
WIN32_LIBADD = -L../../src -layttm -lintl -lglib-2.0 -lws2_32
endif

pkglib_LTLIBRARIES = yahoo2.la
yahoo2_la_SOURCES = yahoo.c

yahoo2_la_LDFLAGS = -module -avoid-version -lyahoo2 $(WIN32_LDFLAGS)

# Gets rid of the datarootdir warning
datarootdir=@datarootdir@
