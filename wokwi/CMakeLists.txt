cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH ${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk)
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(trilog C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

set(SMALL_FLAGS
  -DMAX_NAME=48
  -DMAX_LIST_LIT=128
  -DMAX_CLAUSES=256
  -DMAX_BINDINGS=1024
  -DMAX_GOALS=64
  -DMAX_STACK=128
  -DMAX_ERROR_MSG=128
  -DMAX_CUSTOM_BUILTINS=8
  -DMAX_STRING_POOL=8192
  -DMAX_FILE_PATH=128
  -DMAX_MAKE_FILES=4
  -DMAX_OPEN_STREAMS=4
  -DMAX_CLAUSE_VARS=32
  -DMAX_OPS=48
  -DTERM_POOL_BYTES=49152
)

set(SRCS
  ../src/arith.c
  ../src/builtins.c
  ../src/cli.c
  ../src/debug.c
  ../src/env.c
  ../src/errors.c
  ../src/ffi.c
  ../src/io.c
  ../src/parse.c
  ../src/print.c
  ../src/quad.c
  ../src/solve.c
  ../src/streams.c
  ../src/term.c
  ../src/unify.c
  ssd1306.c
  main_rp2040.c
)

add_executable(trilog ${SRCS})

target_compile_options(trilog PRIVATE ${SMALL_FLAGS})
target_compile_definitions(trilog PRIVATE PICO_BUILD)
target_include_directories(trilog PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/../src)

pico_enable_stdio_usb(trilog 1)
pico_enable_stdio_uart(trilog 0)

target_link_libraries(trilog pico_stdlib hardware_i2c)

pico_add_extra_outputs(trilog)
