function(mediaproxy_validate_static_elf_outputs
        header_output
        program_output
        dynamic_output
        nm_output
        result_variable)
    set(validation_error "")

    if(NOT header_output MATCHES "Type:[ ]+DYN")
        set(validation_error "bootstrap is not a position-independent executable")
    elseif(program_output MATCHES "INTERP")
        set(validation_error "bootstrap contains an ELF interpreter")
    elseif(program_output MATCHES "GNU_STACK[^\n]*RWE")
        set(validation_error "bootstrap requests an executable stack")
    else()
        foreach(forbidden_tag IN ITEMS
                NEEDED
                SONAME
                RPATH
                RUNPATH
                FILTER
                AUXILIARY
                CONFIG
                AUDIT
                DEPAUDIT)
            if(dynamic_output MATCHES "\\(${forbidden_tag}\\)")
                set(validation_error
                    "bootstrap contains forbidden dynamic tag DT_${forbidden_tag}")
                break()
            endif()
        endforeach()

        if(validation_error STREQUAL "" AND NOT nm_output STREQUAL "")
            set(validation_error
                "bootstrap contains unresolved symbols: ${nm_output}")
        endif()
    endif()

    set(${result_variable} "${validation_error}" PARENT_SCOPE)
endfunction()
