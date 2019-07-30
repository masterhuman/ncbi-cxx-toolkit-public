#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI CMake wrapper extension
##  In NCBI CMake wrapper, adds support of datatool code generation
##    Author: Andrei Gourianov, gouriano@ncbi
##


##############################################################################
function(NCBI_internal_process_dataspec _variable _access _value)
    if(NOT "${_access}" STREQUAL "MODIFIED_ACCESS" OR "${_value}" STREQUAL "")
        return()
    endif()
    cmake_parse_arguments(DT "GENERATE" "DATASPEC;REQUIRES;RETURN" "" ${_value})

    get_filename_component(_path     ${DT_DATASPEC} DIRECTORY)
    get_filename_component(_basename ${DT_DATASPEC} NAME_WE)
    get_filename_component(_ext      ${DT_DATASPEC} EXT)
    file(RELATIVE_PATH     _relpath  ${NCBI_SRC_ROOT} ${_path})

    set(_specfiles  ${DT_DATASPEC})
    set(_srcfiles   ${_path}/${_basename}__.cpp ${_path}/${_basename}___.cpp)
    set(_incfiles   ${NCBI_INC_ROOT}/${_relpath}/${_basename}__.hpp)
    if(NOT "${DT_RETURN}" STREQUAL "")
        set(${DT_RETURN} DATASPEC ${_specfiles} SOURCES ${_srcfiles} HEADERS ${_incfiles} PARENT_SCOPE)
    endif()

    if(NOT DT_GENERATE)
        return()
    endif()

    set(_module_imports "")
    set(_imports "")
    if(EXISTS "${_path}/${_basename}.module")
        FILE(READ "${_path}/${_basename}.module" _module_contents)
        STRING(REGEX MATCH "MODULE_IMPORT *=[^\n]*[^ \n]" _tmp "${_module_contents}")
        STRING(REGEX REPLACE "MODULE_IMPORT *= *" "" _tmp "${_tmp}")
        STRING(REGEX REPLACE "  *$" "" _imp_list "${_tmp}")
        STRING(REGEX REPLACE " " ";" _imp_list "${_imp_list}")

        foreach(_module IN LISTS _imp_list)
            set(_module_imports "${_module_imports} ${_module}${_ext}")
        endforeach()
        if (NOT "${_module_imports}" STREQUAL "")
            set(_imports -M ${_module_imports})
        endif()
    endif()

    if (DEFINED NCBI_${NCBI_PROJECT}_PCH)
        set(_fpch ${NCBI_${NCBI_PROJECT}_PCH})
    elseif (DEFINED NCBI__PCH)
        set(_fpch ${NCBI__PCH})
    else()
        set(_fpch ${NCBI_DEFAULT_PCH})
    endif()
    if (NOT "${_fpch}" STREQUAL "")
        set(_pch -pch ${_fpch})
    endif()

    set(_oc ${_basename})
    set(_od ${_path}/${_basename}.def)
    set(_oex -oex " ")
    set(_cmd ${NCBI_DATATOOL} ${_oex} ${_pch} -m ${DT_DATASPEC} -oA -oc ${_oc} -od ${_od} -odi -ocvs -or ${_relpath} -oR ${NCBI_TREE_ROOT} ${_imports})
    set(_depends ${NCBI_DATATOOL} ${DT_DATASPEC})
    if(EXISTS ${_od})
        set(_depends ${_depends} ${_od})
    endif()
    add_custom_command(
        OUTPUT ${_srcfiles} ${_incfiles} ${_path}/${_basename}.files
        COMMAND ${_cmd} VERBATIM
        WORKING_DIRECTORY ${NCBI_TREE_ROOT}
        COMMENT "Generate C++ classes from ${DT_DATASPEC}"
        DEPENDS ${_depends}
        VERBATIM
    )
endfunction()

#############################################################################
NCBI_register_hook(DATASPEC NCBI_internal_process_dataspec ".asn;.dtd;.xsd;.wsdl;.jsd;.json")
