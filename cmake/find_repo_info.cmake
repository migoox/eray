function(find_repo_info OUT_VAR)

    execute_process(
        COMMAND git rev-parse --abbrev-ref HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_BRANCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        OUTPUT_VARIABLE GIT_COMMIT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )

    if(GIT_BRANCH AND GIT_COMMIT)
        set(COMMIT_STRING "${GIT_BRANCH}-${GIT_COMMIT}")
    else()
        set(COMMIT_STRING "unknown")
    endif()

    set(${OUT_VAR} "${COMMIT_STRING}" PARENT_SCOPE)

endfunction()