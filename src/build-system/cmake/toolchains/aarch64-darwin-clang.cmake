#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper
##  Apple Clang toolchain template

set(NCBI_PTBCFG_FLAGS_DEFINED YES)
include_guard(GLOBAL)

set(CMAKE_C_COMPILER "/usr/bin/clang")
set(CMAKE_CXX_COMPILER "/usr/bin/clang++")

set(CMAKE_C_FLAGS_INIT         "-gdwarf-4")
set(CMAKE_C_FLAGS_DEBUG        "-ggdb3 -O0")
set(CMAKE_C_FLAGS_RELEASE      "-ggdb1 -O3")
set(CMAKE_C_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_CXX_FLAGS_INIT       "-gdwarf-4")
set(CMAKE_CXX_FLAGS_DEBUG      "-ggdb3 -O0")
set(CMAKE_CXX_FLAGS_RELEASE    "-ggdb1 -O3")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO  "-ggdb3 -O3")

set(CMAKE_EXE_LINKER_FLAGS_INIT     "")
set(CMAKE_SHARED_LINKER_FLAGS_INIT  "")

#----------------------------------------------------------------------------
set(NCBI_COMPILER_FLAGS_SSE       "")

set(NCBI_COMPILER_FLAGS_COVERAGE  "--coverage")
set(NCBI_LINKER_FLAGS_COVERAGE     "--coverage")

set(NCBI_COMPILER_FLAGS_MAXDEBUG  "-fsanitize=address -fstack-check")
set(NCBI_LINKER_FLAGS_MAXDEBUG   "-fsanitize=address")

set(NCBI_LINKER_FLAGS_STATICCOMPONENTS "")
