AM_CPPFLAGS = -I@top_srcdir@/include
AM_CXXFLAGS = -std=c++17 -g -O0 -W -Wall -pedantic


LDADD = libfolia.la

lib_LTLIBRARIES = libfolia.la
libfolia_la_LDFLAGS = -version-info 22:0:0

libfolia_la_SOURCES = folia_impl.cxx folia_document.cxx folia_utils.cxx \
	folia_types.cxx folia_properties.cxx folia_provenance.cxx \
	folia_subclasses.cxx folia_textpolicy.cxx folia_engine.cxx

bin_PROGRAMS = folialint
folialint_SOURCES = folialint.cxx

bin_SCRIPTS = foliadiff.sh

check_PROGRAMS = simpletest
TESTS = $(check_PROGRAMS)
TESTS_ENVIRONMENT = topsrcdir=$(top_srcdir)
simpletest_SOURCES = simpletest.cxx
CLEANFILES = simpletest.out

EXTRA_DIST = foliadiff.sh
