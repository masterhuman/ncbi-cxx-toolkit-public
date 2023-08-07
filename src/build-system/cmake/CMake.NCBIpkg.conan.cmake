#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package adapter
##  it is used when the Toolkit is built and installed as Conan package
##    Author: Andrei Gourianov, gouriano@ncbi
##

if(NOT DEFINED NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED)
set( NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED ON)

###############################################################################
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0057 NEW)

set(NCBI_PTBCFG_PACKAGED               ON)
set(NCBI_PTBCFG_ENABLE_COLLECTOR       ON)
#set(NCBI_VERBOSE_ALLPROJECTS           OFF)
#set(NCBI_PTBCFG_ALLOW_COMPOSITE        OFF)
#set(NCBI_PTBCFG_ADDTEST                OFF)
get_filename_component(NCBI_PTBCFG_PACKAGELIST "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)
get_filename_component(NCBI_PTBCFG_PACKAGEROOT "${NCBI_PTBCFG_PACKAGELIST}/.."   ABSOLUTE)

###############################################################################
set(_listdir "${CMAKE_CURRENT_LIST_DIR}")
include(${_listdir}/CMake.NCBIptb.definitions.cmake)
include(${_listdir}/CMakeMacros.cmake)
include(${_listdir}/CMake.NCBIptb.cmake)
include(${_listdir}/CMakeChecks.compiler.cmake)
include(${_listdir}/CMake.NCBIpkg.codegen.cmake)
if(NCBI_PTBCFG_ADDTEST)
    include(${_listdir}/CMake.NCBIptb.ctest.cmake)
endif()

###############################################################################
macro(NCBIptb_setup)
    set(_listdir "${NCBI_TREE_CMAKECFG}")
    include(${_listdir}/CMake.NCBIComponents.cmake)
    include_directories(${NCBITK_INC_ROOT} ${NCBI_INC_ROOT})

    include(${_listdir}/CMake.NCBIptb.datatool.cmake)
    include(${_listdir}/CMake.NCBIptb.grpc.cmake)

    NCBI_internal_collect_packagelist()
    set(NCBI_PTBCFG_PACKAGEIMPORTS)

    if (DEFINED NCBI_EXTERNAL_TREE_ROOT)
        set(NCBI_EXTERNAL_BUILD_ROOT ${NCBI_EXTERNAL_TREE_ROOT})
        if (EXISTS ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
            foreach(_next IN LISTS NCBI_PTBCFG_PACKAGELIST)
                if(EXISTS "${_next}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}-release.cmake")
                    include(${_next}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
                    set(NCBI_PTBCFG_PACKAGEIMPORTS "${_next}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}-release.cmake")
                    break()
                endif()
            endforeach()
            if("${NCBI_PTBCFG_PACKAGEIMPORTS}" STREQUAL "")
                include(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
                file(GLOB NCBI_PTBCFG_PACKAGEIMPORTS "${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}-*.cmake")
            endif()
            foreach(_next IN LISTS NCBI_PTBCFG_PACKAGELIST)
                file(GLOB _config_files "${_next}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}-*.cmake")
                foreach(_file IN LISTS _config_files)
                    if(NOT "${_file}" IN_LIST NCBI_PTBCFG_PACKAGEIMPORTS)
                        set(_IMPORT_PREFIX ${_root})
                        include(${_file})
                        list(APPEND NCBI_PTBCFG_PACKAGEIMPORTS ${_file})
                        unset(_IMPORT_PREFIX)
                    endif()
                endforeach()
            endforeach()
        else()
            message(FATAL_ERROR "${NCBI_PTBCFG_INSTALL_EXPORT} was not found in ${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}")
        endif()
        NCBI_import_hostinfo(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.hostinfo)
        NCBI_process_imports(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.imports)
        if(COMMAND conan_basic_setup)
            NCBI_verify_targets(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.imports)
        endif()
    endif()

    NCBI_internal_adjust_conan_targets(${NCBI_EXTERNAL_BUILD_ROOT}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.imports)
    include(${_listdir}/CMakeChecks.final-message.cmake)
    unset(NCBI_PTBCFG_PACKAGEROOT)
    unset(NCBI_PTBCFG_PACKAGELIST)
    unset(NCBI_PTBCFG_PACKAGEIMPORTS)
endmacro()

###############################################################################
function(NCBI_internal_collect_packagelist)
    file(STRINGS "${NCBI_EXTERNAL_TREE_ROOT}/conaninfo.txt" _mainlist)

    set(_pkglist ${NCBI_PTBCFG_PACKAGELIST})
    file(GLOB _tmp LIST_DIRECTORIES TRUE "${NCBI_PTBCFG_PACKAGEROOT}/*")
    foreach(_t IN LISTS _tmp)
        if (EXISTS "${_t}/conaninfo.txt" AND EXISTS ${_t}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
            file(STRINGS "${_t}/conaninfo.txt" _trylist)
            list(REMOVE_ITEM _trylist ${_mainlist})
            set(_good TRUE)
            foreach(_i IN LISTS _trylist)
                if ("${_i}" MATCHES "^[ ]+with")
                    set(_good FALSE)
                    break()
                endif()
            endforeach()
            if (NOT _good)
                continue()
            endif()
        else()
            continue()
        endif()
        if (EXISTS ${_t}/${NCBI_DIRNAME_EXPORT}/${NCBI_PTBCFG_INSTALL_EXPORT}.cmake)
            list(APPEND _pkglist ${_t})
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _pkglist)
    set(NCBI_PTBCFG_PACKAGELIST ${_pkglist} PARENT_SCOPE)
endfunction()

###############################################################################
function(NCBI_internal_adjust_conan_targets _file)
    if(NOT EXISTS ${_file})
        return()
    endif()
    file(STRINGS ${_file} _imports)
    set(_alldeps)
    foreach( _prj IN LISTS _imports)
        if (TARGET "${_prj}")
            get_target_property(_deps ${_prj} INTERFACE_LINK_LIBRARIES)
            list(APPEND _alldeps ${_deps})
        endif()
    endforeach()

    if(TARGET PCRE::PCRE)
        if(NOT TARGET pcre::pcre)
            add_library(pcre::pcre ALIAS PCRE::PCRE)
            list(REMOVE_ITEM _alldeps pcre::pcre)
            list(APPEND _alldeps PCRE::PCRE)
        endif()
    endif()
    if(TARGET gRPC::gRPC)
        if(NOT TARGET grpc::grpc)
            add_library(grpc::grpc ALIAS gRPC::gRPC)
            list(REMOVE_ITEM _alldeps grpc::grpc)
            list(APPEND _alldeps gRPC::gRPC)
        endif()
    endif()
    if(TARGET LibXslt::LibXslt)
        if(NOT TARGET libxslt::libxslt)
            add_library(libxslt::libxslt ALIAS LibXslt::LibXslt)
            list(REMOVE_ITEM _alldeps libxslt::libxslt)
            list(APPEND _alldeps LibXslt::LibXslt)
        endif()
    endif()
    list(APPEND _alldeps libuv::libuv)
    list(APPEND _alldeps libnghttp2::nghttp2)

    list(REMOVE_DUPLICATES _alldeps)
    list(REMOVE_ITEM _alldeps ${_imports})

    if(NOT "${NCBI_CONFIGURATION_TYPES_COUNT}" EQUAL 1)
        set(_allcfg)
        set(_mapped_Debug)
        set(_mapped_Release)
        foreach(_imp IN LISTS NCBI_PTBCFG_PACKAGEIMPORTS)
            get_filename_component(_cfg ${_imp}   NAME)
            string(REPLACE "${NCBI_PTBCFG_INSTALL_EXPORT}-" "" _cfg ${_cfg})
            string(REPLACE ".cmake" "" _cfg ${_cfg})
            NCBI_util_Cfg_ToStd(${_cfg} _map_cfg)
            string(TOUPPER ${_cfg} _cfg)
            set(_mapped_${_map_cfg} ${_cfg})
            list(APPEND _allcfg ${_cfg})
        endforeach()
        if("${_mapped_Debug}" STREQUAL "")
            set(_mapped_Debug ${_mapped_Release})
        elseif("${_mapped_Release}" STREQUAL "")
            set(_mapped_Release ${_mapped_Debug})
        endif()

        foreach( _prj IN LISTS _alldeps)
            if (TARGET "${_prj}")
                foreach(_cfg IN LISTS NCBI_CONFIGURATION_TYPES)
                    string(TOUPPER ${_cfg} _upcfg)
                    if(NOT ${_upcfg} IN_LIST _allcfg)
                        NCBI_util_Cfg_ToStd(${_cfg} _map_cfg)
                        set_target_properties(${_prj} PROPERTIES
                            MAP_IMPORTED_CONFIG_${_upcfg} ${_mapped_${_map_cfg}})
                    endif()
                endforeach()
            endif()
        endforeach()
    endif()
endfunction()

elseif(NCBI_PTBCFG_PACKAGED)
    get_filename_component(_t "${CMAKE_CURRENT_LIST_DIR}/../../.."   ABSOLUTE)
    list(APPEND NCBI_PTBCFG_PACKAGELIST ${_t})
endif(NOT DEFINED NCBI_TOOLKIT_NCBIPTB_BUILD_SYSTEM_INCLUDED)
