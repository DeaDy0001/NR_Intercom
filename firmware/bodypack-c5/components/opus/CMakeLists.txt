# libopus (xiph.org, Submodul opus/ auf v1.5.2) als ESP-IDF-Komponente.
# Festkomma-Build (FIXED_POINT), keine Float-API, keine DNN-Erweiterungen (DRED/OSCE/Deep-PLC).

set(OPUS_DIR "${CMAKE_CURRENT_LIST_DIR}/opus")

# Liest eine Quellliste "VAR = a.c \ b.c ..." aus den Makefile-Fragmenten von libopus
function(nrb_opus_sources mkfile var out)
    file(READ "${OPUS_DIR}/${mkfile}" content)
    string(REPLACE "\\\n" " " content "${content}")
    string(REGEX MATCH "(^|\n)${var} =[^\n]*" line "${content}")
    string(REGEX REPLACE "(^|\n)${var} =" "" line "${line}")
    separate_arguments(items UNIX_COMMAND "${line}")
    list(TRANSFORM items PREPEND "${OPUS_DIR}/")
    set(${out} ${items} PARENT_SCOPE)
endfunction()

if(NOT EXISTS "${OPUS_DIR}/include/opus.h")
    message(FATAL_ERROR "libopus fehlt: 'git submodule update --init' im Repository ausfuehren")
endif()

nrb_opus_sources(celt_sources.mk CELT_SOURCES celt_srcs)
nrb_opus_sources(silk_sources.mk SILK_SOURCES silk_srcs)
nrb_opus_sources(silk_sources.mk SILK_SOURCES_FIXED silk_fixed_srcs)
nrb_opus_sources(opus_sources.mk OPUS_SOURCES opus_srcs)

idf_component_register(
    SRCS ${celt_srcs} ${silk_srcs} ${silk_fixed_srcs} ${opus_srcs}
    INCLUDE_DIRS "opus/include"
    PRIV_INCLUDE_DIRS "opus" "opus/celt" "opus/silk" "opus/silk/fixed" "opus/src"
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    OPUS_BUILD
    FIXED_POINT=1
    DISABLE_FLOAT_API
    VAR_ARRAYS
    HAVE_LRINT=1
    HAVE_LRINTF=1
)
target_compile_options(${COMPONENT_LIB} PRIVATE
    -O2
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-maybe-uninitialized
    -Wno-stringop-overflow
    -Wno-array-parameter
)
