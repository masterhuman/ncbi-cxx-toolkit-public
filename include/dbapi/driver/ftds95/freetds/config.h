#ifndef DBAPI_DRIVER_FTDS95_FREETDS___CONFIG__H
#define DBAPI_DRIVER_FTDS95_FREETDS___CONFIG__H

/*  $Id$
 *
 *  Config file for FreeTDS 0.95 (supporting TDS versions up to 7.3).
 *
 *  NOTE:  Instead of being generated by FreeTDS's own configure script,
 *         it just uses the NCBI C++ Toolkit config file <ncbiconf.h>.
 */

#define TDS73   1
#define VERSION "0.95"

/* ODBC settings */
#define UNIXODBC         1
#define TDS_NO_DM        1
#define ENABLE_ODBC_WIDE 1

/* Some upstream unit tests need this definition to find their data. */
#ifdef NEED_FREETDS_SRCDIR
#  define FREETDS_SRCDIR "."
#endif

#include <ncbiconf.h>

#ifndef NCBI_FTDS_RENAME_SYBDB
#  define NCBI_FTDS_RENAME_SYBDB 1
#endif

#ifdef _DEBUG
#  define DEBUG 1
/* Should we leave the extra checks off? */
#  define ENABLE_EXTRA_CHECKS 1
#endif

#if defined(HAVE_GETHOSTBYADDR_R)
#  if   HAVE_GETHOSTBYADDR_R == 5
#    define HAVE_FUNC_GETHOSTBYADDR_R_5 1
#  elif HAVE_GETHOSTBYADDR_R == 7
#    define HAVE_FUNC_GETHOSTBYADDR_R_7 1
#  elif HAVE_GETHOSTBYADDR_R == 8
#    define HAVE_FUNC_GETHOSTBYADDR_R_8 1
#  else
#    error "Unexpected number of arguments detected for gethostbyaddr_r()"
#  endif
#endif

#if defined(HAVE_GETHOSTBYNAME_R)
#  if   HAVE_GETHOSTBYNAME_R == 3
#     define HAVE_FUNC_GETHOSTBYNAME_R_3 1
#  elif HAVE_GETHOSTBYNAME_R == 5
#     define HAVE_FUNC_GETHOSTBYNAME_R_5 1
#  elif HAVE_GETHOSTBYNAME_R == 6
#     define HAVE_FUNC_GETHOSTBYNAME_R_6 1
#  else
#    error "Unexpected number of arguments detected for gethostbyname_r()"
#  endif
#endif


#ifdef NCBI_HAVE_GETPWUID_R
#  define HAVE_GETPWUID_R 1
#  if NCBI_HAVE_GETPWUID_R == 4
#    define HAVE_FUNC_GETPWUID_R_4 1
#    define HAVE_FUNC_GETPWUID_R_4_PW 1
#  elif NCBI_HAVE_GETPWUID_R == 5
#    define HAVE_FUNC_GETPWUID_R_5 1
#  else
#    error "Unexpected number of arguments detected for getpwuid_r()"
#  endif
#endif

#if defined(HAVE_GETSERVBYNAME_R)
#  if HAVE_GETSERVBYNAME_R == 4
#    define HAVE_FUNC_GETSERVBYNAME_R_4 1
#  elif HAVE_GETSERVBYNAME_R == 5
#    define HAVE_FUNC_GETSERVBYNAME_R_5 1
#  elif HAVE_GETSERVBYNAME_R == 6
#    define HAVE_FUNC_GETSERVBYNAME_R_6 1
#  else
#    error "Unexpected number of arguments detected for getservbyname_r()"
#  endif
#endif

#if defined(HAVE_LIBICONV)  &&  !defined(NCBI_OS_SOLARIS)
#  define HAVE_ICONV 1
#endif

#if SIZEOF_LONG == 8  ||  SIZEOF_LONG_LONG == 8  ||  SIZEOF___INT64 == 8
#  define HAVE_INT64 1
#endif

#ifndef HAVE_SOCKLEN_T
typedef int socklen_t;
#endif

#if 0 /* Avoid extra dependencies, not needed in-house */
#  if defined(HAVE_LIBGNUTLS)
#    define HAVE_GNUTLS 1
#  endif

#  if defined(HAVE_LIBOPENSSL)
#    define HAVE_OPENSSL 1
#  endif
#endif

#ifndef SIZEOF_SQLWCHAR
#  ifdef SQL_WCHART_CONVERT
#    define SIZEOF_SQLWCHAR SIZEOF_WCHAR_T
#  else
#    define SIZEOF_SQLWCHAR SIZEOF_SHORT
#  endif
#endif

#ifdef NCBI_HAVE_READDIR_R
#  define HAVE_READDIR_R 1
#endif

#define SIZEOF_VOID_P SIZEOF_VOIDP

#if defined(HAVE_ATTRIBUTE_DESTRUCTOR)
#  define TDS_ATTRIBUTE_DESTRUCTOR 1
#endif

#if defined(HAVE_DECL_CLOCK_MONOTONIC)  &&  HAVE_DECL_CLOCK_MONOTONIC
#  define TDS_GETTIMEMILLI_CONST CLOCK_MONOTONIC
#elif defined(HAVE_DECL_CLOCK_SGI_CYCLE)  &&  HAVE_DECL_CLOCK_SGI_CYCLE
#  define TDS_GETTIMEMILLI_CONST CLOCK_SGI_CYCLE
#elif defined(HAVE_DECL_CLOCK_REALTIME)  &&  HAVE_DECL_CLOCK_REALTIME
#  define TDS_GETTIMEMILLI_CONST CLOCK_REALTIME
#endif

#if defined(HAVE_PTHREAD_MUTEX)
#  define TDS_HAVE_PTHREAD_MUTEX 1
#endif

#if defined(NCBI_HAVE_STDIO_LOCKED)
#  define TDS_HAVE_STDIO_LOCKED 1
#endif

#ifdef NCBI_SQLCOLATTRIBUTE_SQLLEN
#  define TDS_SQLCOLATTRIBUTE_SQLLEN 1
#endif

#if defined(NCBI_COMPILER_MSVC)
#  if _MSC_VER < 1900 /* Visual Studio 2015 */
#    define snprintf _snprintf
#  endif
#  define TDS_I64_PREFIX "I64"
#  define inline __inline
/* Defined here rather than via ncbiconf(_msvc).h because Microsoft's
 * implementation is too barebones for the connect tree, lacking in
 * particular EAI_SYSTEM.
 */
#  define HAVE_GETADDRINFO 1
#endif

#ifdef NCBI_COMPILER_MSVC
#  define HAVE_SSPI 1
#elif HAVE_LIBKRB5
#  define ENABLE_KRB5 1
#endif

#ifndef _THREAD_SAFE
#  define TDS_NO_THREADSAFE 1
#endif

#include "../impl/rename_ftds_replacements.h"
#include "../impl/rename_ftds_tds.h"

#endif  /* DBAPI_DRIVER_FTDS95_FREETDS___CONFIG__H */
