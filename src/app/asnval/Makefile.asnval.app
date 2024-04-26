#################################
# $Id$
# Author:  Colleen Bollin, based on file by Mati Shomrat
#################################

# Build application "asnval"
#################################

APP = asnvalidate
SRC = asnval thread_state app_config formatters message_handler
LIB = xvalidate taxon1 xmlwrapp xhugeasn $(OBJEDIT_LIBS) $(XFORMAT_LIBS) \
      xalnmgr xobjutil \
      valerr tables xregexp $(PCRE_LIB) $(DATA_LOADERS_UTIL_LIB) $(OBJMGR_LIBS)
LIBS = $(LIBXSLT_LIBS) $(DATA_LOADERS_UTIL_LIBS) $(LIBXML_LIBS) $(PCRE_LIBS) \
       $(CMPRS_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

POST_LINK = $(VDB_POST_LINK)

REQUIRES = objects LIBXML LIBXSLT BerkeleyDB SQLITE3

CXXFLAGS += $(ORIG_CXXFLAGS)
LDFLAGS  += $(ORIG_LDFLAGS)

WATCHERS = stakhovv gotvyans foleyjp
