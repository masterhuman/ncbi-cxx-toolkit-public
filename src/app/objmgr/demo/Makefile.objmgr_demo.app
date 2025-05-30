#################################
# $Id$
# Author:  Aleksey Grichenko (grichenk@ncbi.nlm.nih.gov)
#################################

# Build object manager demo application "objmgr_demo"
#################################

REQUIRES = objects algo dbapi FreeTDS SQLITE3 -Cygwin

APP = objmgr_demo
SRC = objmgr_demo
LIB = ncbi_xloader_blastdb seqdb blastdb \
      ncbi_xloader_lds2 lds2 sqlitewrapp \
      ncbi_xdbapi_ftds $(FTDS_LIB) \
      $(OBJREAD_LIBS) xobjutil $(OBJMGR_LIBS) $(LMDB_LIB)

LIBS = $(SQLITE3_LIBS) $(GENBANK_THIRD_PARTY_LIBS) $(FTDS_LIBS) $(CMPRS_LIBS) \
       $(NETWORK_LIBS) $(DL_LIBS) $(BLAST_THIRD_PARTY_LIBS) $(ORIG_LIBS)


CHECK_COPY = all_readers.sh

CHECK_CMD = all_readers.sh objmgr_demo -id 568815307 -resolve all -adaptive
CHECK_CMD = all_readers.sh objmgr_demo -id ABYI02000001 -resolve all -adaptive

WATCHERS = vasilche
