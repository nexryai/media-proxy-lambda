foreach(required_variable IN ITEMS
        AR
        BOOTSTRAP
        COMPILE_COMMANDS
        CONFIG_HEADER
        FORTIFY_INCLUDE_DIR
        LIBAOM_ARCHIVE
        LINK_MAP
        NM
        PKGCONFIG
        TARGET_ARCH
        TARGET_TRIPLE)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

foreach(required_file IN ITEMS
        "${BOOTSTRAP}"
        "${COMPILE_COMMANDS}"
        "${CONFIG_HEADER}"
        "${LIBAOM_ARCHIVE}"
        "${LINK_MAP}"
        "${PKGCONFIG}")
    if(NOT EXISTS "${required_file}")
        message(FATAL_ERROR "Required libaom build artifact is absent: ${required_file}")
    endif()
endforeach()

execute_process(
    COMMAND "${AR}" t "${LIBAOM_ARCHIVE}"
    RESULT_VARIABLE ar_result
    OUTPUT_VARIABLE archive_members
    ERROR_VARIABLE ar_error)
if(NOT ar_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libaom: ${ar_error}")
endif()
string(REGEX MATCHALL "[^\n]+" archive_member_list "${archive_members}")
list(LENGTH archive_member_list archive_member_count)
if(NOT archive_member_count EQUAL 166)
    message(FATAL_ERROR
        "libaom contains ${archive_member_count} objects instead of 166")
endif()

execute_process(
    COMMAND "${NM}" --defined-only --format=posix "${LIBAOM_ARCHIVE}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE archive_symbols
    ERROR_VARIABLE nm_error)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "Cannot inspect libaom symbols: ${nm_error}")
endif()
foreach(required_symbol IN ITEMS
        aom_codec_version
        aom_codec_av1_cx
        aom_codec_av1_dx
        aom_codec_enc_config_default
        aom_codec_enc_init_ver
        aom_codec_dec_init_ver)
    if(NOT archive_symbols MATCHES "(^|\n)${required_symbol} ")
        message(FATAL_ERROR "libaom does not define ${required_symbol}")
    endif()
endforeach()

file(READ "${CONFIG_HEADER}" aom_config)
foreach(required_config IN ITEMS
        "#define CONFIG_AV1_DECODER 1"
        "#define CONFIG_AV1_ENCODER 1"
        "#define CONFIG_AV1_HIGHBITDEPTH 1"
        "#define CONFIG_MULTITHREAD 1"
        "#define CONFIG_PIC 1"
        "#define CONFIG_RUNTIME_CPU_DETECT 0"
        "#define CONFIG_WEBM_IO 0"
        "#define CONFIG_LIBYUV 0")
    string(FIND "${aom_config}" "${required_config}" config_offset)
    if(config_offset EQUAL -1)
        message(FATAL_ERROR "aom_config.h is missing ${required_config}")
    endif()
endforeach()
string(CONCAT enabled_backend_regex
    "#define (AOM_ARCH_(AARCH64|ARM|PPC|RISCV|X86|X86_64)|"
    "HAVE_(AVX|AVX2|AVX512|MMX|NEON|NEON_DOTPROD|NEON_I8MM|SSE|SSE2|SSE3|"
    "SSE4_1|SSE4_2|SSSE3|SVE|SVE2)) 1")
if(aom_config MATCHES "${enabled_backend_regex}")
    message(FATAL_ERROR "libaom generic build unexpectedly enables SIMD or an architecture backend")
endif()

file(READ "${PKGCONFIG}" pkgconfig)
foreach(required_line IN ITEMS
        "Name: aom"
        "Description: Alliance for Open Media AV1 codec library v3.14.1."
        "Version: 3.14.1"
        "Libs: -L\${libdir} -laom"
        "Libs.private: -lm")
    string(FIND "${pkgconfig}" "${required_line}" pc_offset)
    if(pc_offset EQUAL -1)
        message(FATAL_ERROR "aom.pc is missing ${required_line}: ${pkgconfig}")
    endif()
endforeach()

file(READ "${COMPILE_COMMANDS}" compile_commands_json)
string(JSON command_count LENGTH "${compile_commands_json}")
math(EXPR last_command "${command_count} - 1")
set(libaom_library_command_count 0)
string(CONCAT libaom_library_output_regex
    "/CMakeFiles/(aom_rtcd|aom_obj|aom_dsp_common|aom_dsp|aom_dsp_decoder|"
    "aom_dsp_encoder|aom_mem|aom_util|aom_scale|aom_av1_common|"
    "aom_av1_decoder|aom_av1_encoder)\\.dir/")
foreach(index RANGE 0 ${last_command})
    string(JSON output GET "${compile_commands_json}" ${index} output)
    if(NOT output MATCHES "${libaom_library_output_regex}")
        continue()
    endif()
    math(EXPR libaom_library_command_count
        "${libaom_library_command_count} + 1")
    string(JSON source_file GET "${compile_commands_json}" ${index} file)
    string(JSON compile_command GET "${compile_commands_json}" ${index} command)
    if(NOT source_file MATCHES "\\.c$")
        message(FATAL_ERROR "libaom archive includes non-C source: ${source_file}")
    endif()
    foreach(required_flag IN ITEMS
            "--target=${TARGET_TRIPLE}"
            --sysroot=
            -D_FORTIFY_SOURCE=3
            -fPIC
            -fstack-protector-strong
            -ftrivial-auto-var-init=zero
            -fvisibility=hidden
            -ffunction-sections
            -fdata-sections
            -flto=thin
            -fsanitize=cfi
            -fsanitize-trap=cfi
            -fno-sanitize-recover=cfi
            -std=c11
            -Wall
            -Wextra
            -Werror
            "-isystem ${FORTIFY_INCLUDE_DIR}")
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR
                "${output} is missing ${required_flag}: ${compile_command}")
        endif()
    endforeach()
    if(TARGET_ARCH STREQUAL "x86_64")
        set(required_arch_flags -fstack-clash-protection -fcf-protection=full)
    elseif(TARGET_ARCH STREQUAL "arm64")
        set(required_arch_flags -mbranch-protection=standard)
    else()
        message(FATAL_ERROR "Unsupported TARGET_ARCH: ${TARGET_ARCH}")
    endif()
    foreach(required_flag IN LISTS required_arch_flags)
        string(FIND "${compile_command}" "${required_flag}" flag_offset)
        if(flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} is missing ${required_flag}")
        endif()
    endforeach()
    foreach(forbidden_flag IN ITEMS
            -U_FORTIFY_SOURCE
            -D_FORTIFY_SOURCE=0
            -fno-lto
            -fno-stack-protector
            -ffast-math)
        string(FIND "${compile_command}" "${forbidden_flag}" flag_offset)
        if(NOT flag_offset EQUAL -1)
            message(FATAL_ERROR "${output} contains forbidden ${forbidden_flag}")
        endif()
    endforeach()
endforeach()
if(NOT libaom_library_command_count EQUAL 166)
    message(FATAL_ERROR
        "libaom configured ${libaom_library_command_count} hardened archive "
        "compile commands instead of 166")
endif()

get_filename_component(library_dir "${LIBAOM_ARCHIVE}" DIRECTORY)
get_filename_component(build_dir "${COMPILE_COMMANDS}" DIRECTORY)
file(GLOB forbidden_outputs
    "${library_dir}/libaom.so*"
    "${build_dir}/aomdec"
    "${build_dir}/aomenc"
    "${build_dir}/examples/*"
    "${build_dir}/tools/*")
if(forbidden_outputs)
    message(FATAL_ERROR "libaom built forbidden outputs: ${forbidden_outputs}")
endif()

file(READ "${LINK_MAP}" link_map)
get_filename_component(archive_name "${LIBAOM_ARCHIVE}" NAME)
string(REPLACE "." "\\." archive_regex "${archive_name}")
if(NOT link_map MATCHES "${library_dir}/[^ ]*${archive_regex}")
    message(FATAL_ERROR "bootstrap does not contain ${LIBAOM_ARCHIVE}")
endif()
foreach(required_symbol IN ITEMS
        aom_codec_version
        aom_codec_av1_cx
        aom_codec_av1_dx
        aom_codec_enc_config_default)
    if(NOT link_map MATCHES "${required_symbol}")
        message(FATAL_ERROR "bootstrap link map omits ${required_symbol}")
    endif()
endforeach()
