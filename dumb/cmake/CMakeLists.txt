cmake_minimum_required(VERSION 2.6)
project(libdumb C)

OPTION(BUILD_SHARED "Build Shared Library (OFF for Static)" ON)
OPTION(USE_SSE "Use SSE" OFF)

set(COMMON_FLAGS "-Wall -DDUMB_DECLARE_DEPRECATED -Wno-unused-but-set-variable")
IF(USE_SSE)
    set(COMMON_FLAGS "${COMMON_FLAGS} -msse -D_USE_SSE")
ENDIF(USE_SSE)

set(CMAKE_C_FLAGS "${COMMON_FLAGS} -ffast-math -O2")
set(CMAKE_C_FLAGS_DEBUG "${COMMON_FLAGS} -ggdb -DDEBUGMODE")
set(CMAKE_C_FLAGS_RELEASE "${COMMON_FLAGS} -ffast-math -O2")

link_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(../include/)

SET(SOURCES
    ../src/core/unload.c
    ../src/core/rendsig.c
    ../src/core/rendduh.c
    ../src/core/register.c
    ../src/core/readduh.c
    ../src/core/rawsig.c
    ../src/core/makeduh.c
    ../src/core/loadduh.c
    ../src/core/dumbfile.c
    ../src/core/duhtag.c
    ../src/core/duhlen.c
    ../src/core/atexit.c
    ../src/helpers/stdfile.c
    ../src/helpers/silence.c
    ../src/helpers/sampbuf.c
    ../src/helpers/riff.c
    ../src/helpers/resample.c
    ../src/helpers/memfile.c
    ../src/helpers/clickrem.c
    ../src/helpers/blip_buf.c
    ../src/helpers/barray.c
    ../src/helpers/tarray.c
    ../src/it/xmeffect.c
    ../src/it/readxm2.c
    ../src/it/readxm.c
    ../src/it/readstm2.c
    ../src/it/readstm.c
    ../src/it/reads3m2.c
    ../src/it/reads3m.c
    ../src/it/readriff.c
    ../src/it/readptm.c
    ../src/it/readpsm.c
    ../src/it/readoldpsm.c
    ../src/it/readokt2.c
    ../src/it/readokt.c
    ../src/it/readmtm.c
    ../src/it/readmod2.c
    ../src/it/readmod.c
    ../src/it/readdsmf.c
    ../src/it/readasy.c
    ../src/it/readamf2.c
    ../src/it/readamf.c
    ../src/it/readam.c
    ../src/it/read6692.c
    ../src/it/read669.c
    ../src/it/ptmeffect.c
    ../src/it/loadxm2.c
    ../src/it/loadxm.c
    ../src/it/loadstm2.c
    ../src/it/loadstm.c
    ../src/it/loads3m2.c
    ../src/it/loads3m.c
    ../src/it/loadriff2.c
    ../src/it/loadriff.c
    ../src/it/loadptm2.c
    ../src/it/loadptm.c
    ../src/it/loadpsm2.c
    ../src/it/loadpsm.c
    ../src/it/loadoldpsm2.c
    ../src/it/loadoldpsm.c
    ../src/it/loadokt2.c
    ../src/it/loadokt.c
    ../src/it/loadmtm2.c
    ../src/it/loadmtm.c
    ../src/it/loadmod2.c
    ../src/it/loadmod.c
    ../src/it/loadasy2.c
    ../src/it/loadasy.c
    ../src/it/loadamf2.c
    ../src/it/loadamf.c
    ../src/it/load6692.c
    ../src/it/load669.c
    ../src/it/itunload.c
    ../src/it/itrender.c
    ../src/it/itread2.c
    ../src/it/itread.c
    ../src/it/itorder.c
    ../src/it/itmisc.c
    ../src/it/itload2.c
    ../src/it/itload.c
    ../src/it/readany.c
    ../src/it/loadany2.c
    ../src/it/loadany.c
    ../src/it/readany2.c
    ../src/helpers/lanczos_resampler.c
    ../src/helpers/lpc.c
)

set(INSTALL_HEADERS
    ../include/dumb.h
)

set(INSTALL_HEADERS_INTERNAL
    ../include/internal/blip_buf.h
    ../include/internal/lanczos_resampler.h
)

IF(BUILD_SHARED)
    add_library(dumb SHARED ${SOURCES})
ELSE(BUILD_SHARED)
    add_library(dumb STATIC ${SOURCES})
ENDIF(BUILD_SHARED)

set_target_properties(dumb PROPERTIES DEBUG_POSTFIX d)

INSTALL(FILES ${INSTALL_HEADERS} DESTINATION include/dumb/)
INSTALL(FILES ${INSTALL_HEADERS_INTERNAL} DESTINATION include/dumb/internal/)
INSTALL(TARGETS dumb
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)
