#! /bin/sh
# $Id$
# Author:  Denis Vakatov (vakatov@ncbi.nlm.nih.gov)

def_builddir="$NCBI/ver0.0/c++/Debug/build"

script=`basename $0`


#################################

Usage()
{
  cat <<EOF
USAGE:  $script <name> <type> [builddir]
SYNOPSIS:
   Create a model makefile "Makefile.<name>_<type>" to build
   a library or an application that uses pre-built NCBI C++ toolkit.
ARGUMENTS:
   <name>      -- name of the project (will be subst. to the makefile name)
   <type>      -- one of {lib, app} (whether to build library or application)
   [builddir]  -- path to the pre-built NCBI C++ toolkit
                  (default = $def_builddir)

ERROR:  $1
EOF
  exit 1
}

#################################

CreateMakefile_Lib()
{
  makefile_name="$1"

  cat > "$makefile_name" <<EOF
# Makefile:  $makefile_name
# This file was originally generated from by shell script "$script"


###  PATH TO A PRE-BUILT C++ TOOLKIT  ###
builddir = $builddir
# builddir = $(NCBI)/ver0.0/c++/Release/build


###  DEFAULT COMPILATION FLAGS -- DON'T EDIT OR MOVE THESE 3 LINES !!!  ###
include $(builddir)/Makefile.mk
BINCOPY = :
LOCAL_CPPFLAGS = -I.


#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (LIBRARY) TARGET HERE                  ### 

LIBOBJ = 
LIB    = 
# LOBJ =

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
#                                                                         ###
#############################################################################


###  LIBRARY BUILD RULES -- DON'T EDIT OR MOVE THIS LINE !!!  ###
include $(builddir)/Makefile.lib


###  PUT YOUR OWN ADDITIONAL TARGETS (MAKE COMMANDS/RULES) BELOW HERE  ###
EOF
}



#################################

CreateMakefile_App()
{
  makefile_name="$1"

  cat > "$makefile_name" <<EOF
# Makefile:  $makefile_name
# This file was originally generated from by shell script "$script"

###  PATH TO A PRE-BUILT C++ TOOLKIT  ###
builddir = $builddir
# builddir = $(NCBI)/ver0.0/c++/Release/build


###  DEFAULT COMPILATION FLAGS  -- DON'T EDIT OR MOVE THESE 3 LINES !!!  ###
include $(builddir)/Makefile.mk
BINCOPY = :
LOCAL_CPPFLAGS = -I.


#############################################################################
###  EDIT SETTINGS FOR THE DEFAULT (APPLICATION) TARGET HERE              ### 
APP = 
OBJ = 

# PRE_LIBS = $(NCBI_C_LIBPATH) .....
LIB        = 
# LIBS     = $(NCBI_C_LIBPATH) -lncbi $(NETWORK_LIBS) $(ORIG_LIBS)

# CPPFLAGS = $(ORIG_CPPFLAGS) $(NCBI_C_INCLUDE)
# CFLAGS   = $(ORIG_CFLAGS)
# CXXFLAGS = $(ORIG_CXXFLAGS)
# LDFLAGS  = $(ORIG_LDFLAGS)
#                                                                         ###
#############################################################################


###  APPLICATION BUILD RULES  -- DON'T EDIT OR MOVE THIS LINE !!!  ###
include $(builddir)/Makefile.app


###  PUT YOUR OWN ADDITIONAL TARGETS (MAKE COMMANDS/RULES) BELOW HERE  ###
EOF
}



#################################

case $# in
  3)  proj_name="$1" ; proj_type="$2" ; builddir="$3" ;;
  2)  proj_name="$1" ; proj_type="$2" ; builddir="$def_builddir" ;;
  *)  Usage "Invalid number of arguments" ;;
esac

if test ! -d "$builddir"  ||  test ! -f "$builddir/../inc/ncbiconf.h" ; then
  Usage "Pre-built NCBI C++ toolkit is not found in:  \"$builddir\""
fi

if test "$proj_name" != `basename $proj_name` ; then
  Usage "Invalid project name:  \"$proj_name\""
fi

makefile_name="Makefile.${proj_name}_${proj_type}"
if test ! -d $proj_name ; then
  if test ! -d ../$proj_name ; then
     mkdir $proj_name
  fi
fi
test -d $proj_name &&  makefile_name="$proj_name/$makefile_name"
makefile_name=`pwd`/$makefile_name

if test -f $makefile_name ; then
  echo "\"$makefile_name\" already exists.  Do you want to override it?  [y/n]"
  read answer
  test "$answer" = "y"  ||  test "$answer" = "Y"  ||  exit 2
fi


case "$proj_type" in
  lib )  CreateMakefile_Lib $makefile_name ;;
  app )  CreateMakefile_App $makefile_name ;;
  * )  Usage "Invalid project type:  \"$proj_type\"" ;;
esac

echo "Created a model makefile \"$makefile_name\"."

