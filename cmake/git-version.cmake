function(get_git_version)
    # --- Check for uncommitted changes ---
    execute_process(
            COMMAND git status -s
            OUTPUT_VARIABLE GIT_STATUS
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (GIT_STATUS)
        set(UNCOMMITTED_CHANGES TRUE)
    else ()
        set(UNCOMMITTED_CHANGES FALSE)
    endif ()

    # --- Get latest tag ---
    execute_process(
            COMMAND git describe --tags --abbrev=0
            OUTPUT_VARIABLE LATEST_TAG
            RESULT_VARIABLE GIT_DESCRIBE_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    set(DIRTY FALSE)

    if (NOT GIT_DESCRIBE_RESULT EQUAL 0)
        # No tag found
        set(LATEST_TAG "0.0.0")
        set(DIRTY TRUE)
    else ()
        execute_process(
                COMMAND git rev-list -n 1 ${LATEST_TAG}
                OUTPUT_VARIABLE TAG_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        execute_process(
                COMMAND git rev-parse HEAD
                OUTPUT_VARIABLE CURRENT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if (NOT TAG_HASH STREQUAL CURRENT_HASH)
            set(DIRTY TRUE)
        endif ()
        if (UNCOMMITTED_CHANGES)
            set(DIRTY TRUE)
        endif ()
    endif ()

    # --- GitHub run number (default: 1) ---
    if (DEFINED ENV{GITHUB_RUN_NUMBER})
        set(RUN_NUMBER $ENV{GITHUB_RUN_NUMBER})
    else ()
        set(RUN_NUMBER "1")
    endif ()

    # --- Parse major/minor/patch ---
    string(REPLACE "." ";" VERSION_LIST ${LATEST_TAG})
    list(GET VERSION_LIST 0 VERSION_MAJOR)
    list(GET VERSION_LIST 1 VERSION_MINOR)
    list(GET VERSION_LIST 2 VERSION_PATCH)

    # --- VERSION_REF (numeric) ---
    if (DIRTY)
        set(VERSION_REF ${RUN_NUMBER})
    else ()
        set(VERSION_REF 0)
    endif ()

    # --- VERSION_REF_STR (string, may contain suffix) ---
    if (DIRTY)
        execute_process(
                COMMAND git rev-parse --short HEAD
                OUTPUT_VARIABLE SHORT_HASH
                OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        set(VERSION_REF_STR "(pre ${SHORT_HASH}")
        if (UNCOMMITTED_CHANGES)
            set(VERSION_REF_STR "${VERSION_REF_STR}+")
        endif ()
        set(VERSION_REF_STR "${VERSION_REF_STR})")
    else ()
        set(VERSION_REF_STR "${VERSION_REF}")
    endif ()

    set(VERSION_MAJOR "${VERSION_MAJOR}" PARENT_SCOPE)
    set(VERSION_MINOR "${VERSION_MINOR}" PARENT_SCOPE)
    set(VERSION_PATCH "${VERSION_PATCH}" PARENT_SCOPE)
    set(VERSION_REF "${VERSION_REF}" PARENT_SCOPE)
    set(VERSION_REF_STR "${VERSION_REF_STR}" PARENT_SCOPE)
endfunction()
