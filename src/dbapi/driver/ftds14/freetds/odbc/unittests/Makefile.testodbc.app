# $Id$

APP = odbc14_testodbc
SRC = testodbc common

CPPFLAGS = -DHAVE_CONFIG_H=1 $(FTDS14_INCLUDE) $(ODBC_INCLUDE) $(ORIG_CPPFLAGS)
LIB      = odbc_ftds14 tds_ftds14 odbc_ftds14
LIBS     = $(FTDS14_CTLIB_LIBS) $(NETWORK_LIBS) $(RT_LIBS) $(C_LIBS)
LINK     = $(C_LINK)

CHECK_CMD  = test-odbc14 --no-auto odbc14_testodbc
CHECK_COPY = odbc.ini

CHECK_REQUIRES = in-house-resources

WATCHERS = ucko satskyse
