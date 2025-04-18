# $Id$

APP = dbapi_conn_policy
SRC = dbapi_conn_policy

LIB  = dbapi_sample_base$(STATIC) $(DBAPI_CTLIB) $(DBAPI_ODBC) \
       ncbi_xdbapi_ftds ncbi_xdbapi_ftds100 $(FTDS100_LIB) \
       ncbi_xdbapi_ftds14 $(FTDS14_LIB) \
       dbapi_driver$(DLL) $(XCONNEXT) xconnect xutil xncbi

LIBS = $(SYBASE_LIBS) $(SYBASE_DLLS) $(ODBC_LIBS) $(FTDS100_LIBS) \
       $(FTDS14_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_COPY = dbapi_conn_policy.ini

WATCHERS = ucko satskyse
