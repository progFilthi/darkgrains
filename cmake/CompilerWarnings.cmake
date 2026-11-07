# Adds a consistent warning baseline to DarkGrain-owned targets.
#
# JUCE's recommended warning flags can be used alongside this function, but the
# project keeps its own baseline so CI can opt into -Werror without changing the
# DSP or UI source code.

option(DARKGRAIN_WARNINGS_AS_ERRORS "Treat compiler warnings in DarkGrain targets as errors" OFF)

function(darkgrain_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /Zc:__cplusplus)

        if(DARKGRAIN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)

        if(DARKGRAIN_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()
