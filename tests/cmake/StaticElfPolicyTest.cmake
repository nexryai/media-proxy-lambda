if(NOT DEFINED SOURCE_DIR)
    message(FATAL_ERROR "SOURCE_DIR is required")
endif()

include("${SOURCE_DIR}/cmake/StaticElfPolicy.cmake")

set(valid_header "Type:                              DYN (Shared object file)")
set(valid_program [=[
  LOAD           0x000000 0x0000000000000000 R E 0x1000
  DYNAMIC        0x000f20 0x0000000000002f20 RW  0x8
  GNU_STACK      0x000000 0x0000000000000000 RW  0x0
]=])
set(valid_dynamic [=[
Dynamic section at offset 0xf20 contains 5 entries:
  0x000000006ffffffb (FLAGS_1)      NOW PIE
  0x0000000000000007 (RELA)         0x2d0
  0x0000000000000019 (INIT_ARRAY)   0x2f10
  0x000000000000001a (FINI_ARRAY)   0x2f18
  0x0000000000000000 (NULL)         0x0
]=])

function(assert_policy_accepts test_name header program dynamic symbols)
    mediaproxy_validate_static_elf_outputs(
        "${header}"
        "${program}"
        "${dynamic}"
        "${symbols}"
        policy_error
    )
    if(NOT policy_error STREQUAL "")
        message(FATAL_ERROR "${test_name}: unexpectedly rejected: ${policy_error}")
    endif()
endfunction()

function(assert_policy_rejects test_name expected_error header program dynamic symbols)
    mediaproxy_validate_static_elf_outputs(
        "${header}"
        "${program}"
        "${dynamic}"
        "${symbols}"
        policy_error
    )
    if(NOT policy_error STREQUAL expected_error)
        message(FATAL_ERROR
            "${test_name}: expected '${expected_error}', got '${policy_error}'")
    endif()
endfunction()

assert_policy_accepts(
    "static PIE self-relocation dynamic table"
    "${valid_header}"
    "${valid_program}"
    "${valid_dynamic}"
    ""
)
assert_policy_accepts(
    "static PIE without a dynamic table"
    "${valid_header}"
    "${valid_program}"
    "There is no dynamic section in this file."
    ""
)

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
    set(forbidden_dynamic
        "${valid_dynamic}\n  0x0000000000000001 (${forbidden_tag}) forbidden")
    assert_policy_rejects(
        "forbidden DT_${forbidden_tag}"
        "bootstrap contains forbidden dynamic tag DT_${forbidden_tag}"
        "${valid_header}"
        "${valid_program}"
        "${forbidden_dynamic}"
        ""
    )
endforeach()

assert_policy_rejects(
    "ELF type"
    "bootstrap is not a position-independent executable"
    "Type:                              EXEC (Executable file)"
    "${valid_program}"
    "${valid_dynamic}"
    ""
)
assert_policy_rejects(
    "ELF interpreter"
    "bootstrap contains an ELF interpreter"
    "${valid_header}"
    "${valid_program}\n  INTERP         0x000318 0x0000000000000318 R   0x1"
    "${valid_dynamic}"
    ""
)
assert_policy_rejects(
    "executable stack"
    "bootstrap requests an executable stack"
    "${valid_header}"
    "  GNU_STACK      0x000000 0x0000000000000000 RWE 0x0"
    "${valid_dynamic}"
    ""
)
assert_policy_rejects(
    "unresolved symbol"
    "bootstrap contains unresolved symbols:                  U external_symbol\n"
    "${valid_header}"
    "${valid_program}"
    "${valid_dynamic}"
    "                 U external_symbol\n"
)
