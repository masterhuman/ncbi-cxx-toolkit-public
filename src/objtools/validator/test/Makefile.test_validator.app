###############################
# $Id$
###############################

APP = test_validator
SRC = test_validator
LIB = xvalidate xhugeasn taxon1 $(OBJEDIT_LIBS) $(XFORMAT_LIBS) \
      xalnmgr xobjutil valerr tables xregexp $(PCRE_LIB) $(OBJMGR_LIBS)

LIBS =  $(GENBANK_THIRD_PARTY_LIBS) $(PCRE_LIBS) $(CMPRS_LIBS) $(DL_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS)

CHECK_CMD  = test_validator.sh
CHECK_COPY = current.prt test_validator.sh

REQUIRES = -Cygwin objects

WATCHERS = stakhovv kans foleyjp asztalos gotvyans
