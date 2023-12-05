#############################################################################
# $Id$
#############################################################################
#############################################################################
##
##  NCBI C++ Toolkit Conan package adapter
##  Installs required packages and calls conan_basic_setup
##    Author: Andrei Gourianov, gouriano@ncbi
##


if(NCBI_PTBCFG_PACKAGING)
    set(CONAN_CMAKE_CXX_STANDARD 17)
    foreach(_sub IN LISTS CMAKE_BINARY_DIR CMAKE_MODULE_PATH)
        if (EXISTS "${_sub}/conanbuildinfo.cmake")
            include(${_sub}/conanbuildinfo.cmake)
            conan_basic_setup(KEEP_RPATHS)
            break()
        endif()
    endforeach()
elseif(NCBI_PTBCFG_USECONAN)
    if(EXISTS "${CMAKE_BINARY_DIR}/conanfile.txt")
        file(REMOVE "${CMAKE_BINARY_DIR}/conanfile.txt")
    endif()
    if (MSVC)
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/conanfile.MSVC.txt" DESTINATION "${CMAKE_BINARY_DIR}")
        file(RENAME "${CMAKE_BINARY_DIR}/conanfile.MSVC.txt" "${CMAKE_BINARY_DIR}/conanfile.txt")
    elseif (APPLE)
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/conanfile.XCODE.txt" DESTINATION "${CMAKE_BINARY_DIR}")
        file(RENAME "${CMAKE_BINARY_DIR}/conanfile.XCODE.txt" "${CMAKE_BINARY_DIR}/conanfile.txt")
    else()
        file(COPY "${CMAKE_CURRENT_LIST_DIR}/conanfile.UNIX.txt" DESTINATION "${CMAKE_BINARY_DIR}")
        file(RENAME "${CMAKE_BINARY_DIR}/conanfile.UNIX.txt" "${CMAKE_BINARY_DIR}/conanfile.txt")
    endif()
    message("#############################################################################")
    message("Installing Conan packages")
    find_program(NCBI_CONAN_APP conan${CMAKE_EXECUTABLE_SUFFIX})
    if(NCBI_CONAN_APP)
        execute_process(
            COMMAND ${NCBI_CONAN_APP} version
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE NCBI_CONAN_VERSION
            OUTPUT_QUIET ERROR_QUIET
        )
        if(NCBI_CONAN_VERSION EQUAL 0)
            set(NCBI_CONAN_VERSION 2)
        else()
            set(NCBI_CONAN_VERSION 1)
        endif()
        message("Conan v${NCBI_CONAN_VERSION}.x: ${NCBI_CONAN_APP}")
    else()
        message(FATAL_ERROR "Conan not found")
    endif()
    find_program(NCBI_CMAKE_APP cmake${CMAKE_EXECUTABLE_SUFFIX})
    message("CMake: ${NCBI_CMAKE_APP}")

    if(NCBI_CONAN_VERSION EQUAL 1)
        set(_cmd install ${CMAKE_BINARY_DIR} --build missing -pr:b default -if ${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN})
    else()
        set(_cmd install ${CMAKE_BINARY_DIR} --build missing -pr:b default -of ${CMAKE_BINARY_DIR}/${NCBI_DIRNAME_CONANGEN})
    endif()
    if ("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_C_COMPILER_ID}" STREQUAL "Intel")
        set(_cmd ${_cmd} -s compiler.libcxx=libstdc++11)
    endif()
    if(StaticComponents IN_LIST NCBI_PTBCFG_PROJECT_FEATURES OR (MSVC AND NOT BUILD_SHARED_LIBS))
        set(_cmd ${_cmd} -o *:shared=False)
    else()
        set(_cmd ${_cmd} -o *:shared=True)
        if(NOT NCBI_CONAN_VERSION EQUAL 1 AND EXISTS "${CMAKE_BINARY_DIR}/conanfile.txt")
            file(STRINGS "${CMAKE_BINARY_DIR}/conanfile.txt" _options)
            foreach(_opt IN LISTS _options)
                string(FIND "${_opt}" "#" _p1)
                string(FIND "${_opt}" ":shared" _p2)
                if(${_p2} GREATER 0 AND ${_p1} LESS 0)
                    string(REPLACE " " "" _p2 "${_p2}")
                    set(_cmd ${_cmd} -o ${_opt})
                endif()
            endforeach()
        endif()
    endif()

    string(REPLACE "," ";" NCBI_PTBCFG_CONAN_ARGS "${NCBI_PTBCFG_CONAN_ARGS}")
    string(REPLACE " " ";" NCBI_PTBCFG_CONAN_ARGS "${NCBI_PTBCFG_CONAN_ARGS}")
    set(_cmd ${_cmd} ${NCBI_PTBCFG_CONAN_ARGS})

    set(_types)
    if (NOT "${NCBI_PTBCFG_CONFIGURATION_TYPES}" STREQUAL "")
        set(_types ${NCBI_PTBCFG_CONFIGURATION_TYPES})
    elseif (NOT "${CMAKE_BUILD_TYPE}" STREQUAL "")
        set(_types ${CMAKE_BUILD_TYPE})
    elseif (NOT "${CMAKE_CONFIGURATION_TYPES}" STREQUAL "")
        set(_types ${CMAKE_CONFIGURATION_TYPES})
    endif()
    set(_configs)
    foreach(_t IN LISTS _types)
        NCBI_util_Cfg_ToStd(${_t} _cfg)
        if(NCBI_CONAN_VERSION EQUAL 1 OR NOT MSVC)
            set(_cmd${_cfg} ${_cmd} -s build_type=${_cfg})
#invoke Conan install so that it will generate files for the debug configuration, pointing at the release one
#set(_cmd${_cfg} ${_cmd} -s &:build_type=${_cfg} -s build_type=Release)
        else()
            set(_cmd${_cfg} ${_cmd} -s build_type=${_cfg} -s compiler.runtime_type=${_cfg})
        endif()
        list(APPEND _configs ${_cfg})
    endforeach()
    list(REMOVE_DUPLICATES _configs)

    foreach(_cfg IN LISTS _configs)
        execute_process(
            COMMAND ${NCBI_CONAN_APP} ${_cmd${_cfg}}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
            RESULT_VARIABLE CONAN_INSTALL_RESULT
        )
        if(NOT CONAN_INSTALL_RESULT EQUAL "0")
            message(FATAL_ERROR "Conan setup failed: error = ${CONAN_INSTALL_RESULT}")
        endif()
    endforeach()

    message("Done with installing Conan packages")
    message("#############################################################################")

    if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
        set(CONAN_CMAKE_CXX_STANDARD 17)
        include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
        conan_basic_setup(NO_OUTPUT_DIRS KEEP_RPATHS)
    endif()
#    conan_define_targets()
#    set(CMAKE_CONFIGURATION_TYPES "${CONAN_SETTINGS_BUILD_TYPE}" CACHE STRING "Reset the configurations" FORCE)
else()
    message(FATAL_ERROR "Incorrect include of ${CMAKE_CURRENT_LIST_FILE}")
endif()
