#! /bin/sh
#############################################################################
# Setup the local working environment for the "configure" script
#   Compiler:    WorkShop-6 C++ 5.1
#   OS:          Solaris 2.6 (or higher)
#   Processors:  Sparc,  Intel
#
# $Revision$  // by Denis Vakatov, NCBI (vakatov@ncbi.nlm.nih.gov)
#############################################################################


## Compiler location and attributes
WS_BIN="/netopt/forte6/bin"
export WS_BIN

## Configure using generic script "WorkShop.sh"
${CONFIG_SHELL-/bin/sh} `dirname $0`/WorkShop.sh "$@"
