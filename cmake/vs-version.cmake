function(generate_version_file)
    # set() all needed variables here, they are only set in function scope
    set(options)
    set(oneValueArgs
            VERSION_MAJOR
            VERSION_MINOR
            VERSION_PATCH
            VERSION_REF
            VERSION_MAJOR_STR
            VERSION_MINOR_STR
            VERSION_PATCH_STR
            VERSION_REF_STR
            INTERNAL_NAME
            COPYRIGHT
            ORIGINAL_FILENAME
            PRODUCT_NAME
            DESCRIPTION
    )
    set(multiValueArgs)

    # cmake_parse_arguments
    cmake_parse_arguments(PROJECT "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # check variables that are not optional
    if (NOT DEFINED PROJECT_VERSION_MAJOR)
        message(FATAL_ERROR "VERSION_MAJOR has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_VERSION_MINOR)
        message(FATAL_ERROR "VERSION_MINOR has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_VERSION_PATCH)
        message(FATAL_ERROR "VERSION_PATCH has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_VERSION_REF)
        message(FATAL_ERROR "VERSION_REF has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_VERSION_MAJOR_STR)
        set(PROJECT_VERSION_MAJOR_STR "${PROJECT_VERSION_MAJOR}")
    endif ()
    if (NOT DEFINED PROJECT_VERSION_MINOR_STR)
        set(PROJECT_VERSION_MINOR_STR "${PROJECT_VERSION_MINOR}")
    endif ()
    if (NOT DEFINED PROJECT_VERSION_PATCH_STR)
        set(PROJECT_VERSION_PATCH_STR "${PROJECT_VERSION_PATCH}")
    endif ()
    if (NOT DEFINED PROJECT_VERSION_REF_STR)
        set(PROJECT_VERSION_REF_STR "${PROJECT_VERSION_REF}")
    endif ()

    if (NOT DEFINED PROJECT_INTERNAL_NAME)
        message(FATAL_ERROR "INTERNAL_NAME has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_COPYRIGHT)
        set(PROJECT_COPYRIGHT "")
    endif ()

    if (NOT DEFINED PROJECT_ORIGINAL_FILENAME)
        message(FATAL_ERROR "ORIGINAL_FILENAME has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_PRODUCT_NAME)
        message(FATAL_ERROR "PRODUCT_NAME has to be defined!")
    endif ()

    if (NOT DEFINED PROJECT_DESCRIPTION)
        set(PROJECT_DESCRIPTION "")
    endif ()

    configure_file("${CMAKE_CURRENT_FUNCTION_LIST_DIR}/VersionInfo.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.rc" @ONLY)

    set(VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.rc" PARENT_SCOPE)
endfunction()
