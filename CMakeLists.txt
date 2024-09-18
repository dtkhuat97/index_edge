cmake_minimum_required(VERSION 3.10)
project(cgraph VERSION 1.0.0 DESCRIPTION "libcgraph for reeading compressed graphs")

set(CMAKE_C_STANDARD 11)

# Error on undefined references
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,-undefined,error")
else()
  set(CMAKE_SHARED_LINKER_FLAGS "-Wl,--no-undefined")
endif()

# Allow support for large files
add_definitions(-D_LARGEFILE_SOURCE -D_LARGE_FILES -D_FILE_OFFSET_BITS=64)

# Possibility to enable -march=native to increase performance of popcount
option(OPTIMIZE_FOR_NATIVE "Build with -march=native and -mtune=native" OFF)
option(TRACE_SYMBOLS "Add the trace symbols for the panic function" ON)
option(NO_MMAP "Do not read the compressed graph with mmap" OFF)
option(WITH_RRR "Add support for bit sequences of type RRR" OFF)
option(CLI "Enable the compilation of the command-line tool" ON)

configure_file("include/cgraph.h.cmake" "${CMAKE_CURRENT_BINARY_DIR}/cgraph.h" @ONLY)

# get files with "find include src -type f -name '*.c' | sort"
set(SOURCES
  src/bits/bitarray.c
  src/bits/bitsequence.c
  src/bits/reader.c
  src/bits/writer.c
  src/cgraph/cgraphr.c
  src/cgraph/cgraphw.c
  src/collections/hashmap.c
  src/collections/intset.c
  src/collections/map.c
  src/collections/ringqueue.c
  src/collections/treemap.c
  src/compress/dict/dict_writer.c
  src/compress/dict/fm_index_writer.c
  src/compress/dict/huffman.c
  src/compress/dict/wavelet_tree_writer.c
  src/compress/graph/eliasfano_list.c
  src/compress/graph/hgraph.c
  src/compress/graph/k2_writer.c
  src/compress/graph/repair.c
  src/compress/graph/rule_creator.c
  src/compress/graph/slhr_grammar.c
  src/compress/graph/slhr_grammar_writer.c
  src/reader/bitsequence_r.c
  src/reader/dict.c
  src/reader/edge.c
  src/reader/eliasfano.c
  src/reader/fmindex.c
  src/reader/grammar.c
  src/reader/k2.c
  src/reader/rules.c
  src/reader/startsymbol.c
  src/reader/wavelettree.c
  src/util/arith.c
  src/util/memdup.c
  src/util/panic.c
)

if(WITH_RRR)
  add_definitions(-DRRR)

  list(APPEND SOURCES
    src/bits/rrr_writer.c
    src/bits/rrr_writer.h
    src/reader/table.c
    src/reader/table_data.h
    src/reader/table.h
  )
endif()

if(NOT NO_MMAP)
  add_definitions(-DUSE_MMAP)
endif()

set(INCLUDES
  ${CMAKE_CURRENT_BINARY_DIR}
  src/bits
  src/collections
  src/compress/dict
  src/compress/graph
  src/reader
  src/util
  /home/linuxbrew/.linuxbrew/include/serd-0/serd
  /home/linuxbrew/.linuxbrew/include
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDES})

target_link_libraries(${PROJECT_NAME} PRIVATE m) # link with math library
#target_link_libraries(${PROJECT_NAME} PRIVATE /usr/local/lib/libdivsufsort64.dylib)
#target_link_libraries(${PROJECT_NAME} PRIVATE divsufsort64) # link with libdivsufsort to create the suffix array
target_link_libraries(${PROJECT_NAME} PRIVATE /home/linuxbrew/.linuxbrew/lib/libdivsufsort64.so)

set_target_properties(${PROJECT_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${CMAKE_CURRENT_BINARY_DIR}/cgraph.h)

# build with -march=native if OPTIMIZE_FOR_NATIVE option set
if(OPTIMIZE_FOR_NATIVE)
  target_compile_options(${PROJECT_NAME} PRIVATE -march=native -mtune=native)
endif()

# adding the trace symbols
if(TRACE_SYMBOLS)
  target_link_options(${PROJECT_NAME} PRIVATE -rdynamic)
endif()

# CLI
if(CLI)
  add_executable(cgraph-cli cmd/cgraph.c)
  add_dependencies(cgraph-cli ${PROJECT_NAME}) # add library add dependency

  target_include_directories(cgraph-cli PRIVATE ${INCLUDES})

  target_link_libraries(cgraph-cli PRIVATE ${PROJECT_NAME})
  #target_link_libraries(cgraph-cli PRIVATE /usr/local/lib/libserd-0.dylib)
  #target_link_libraries(cgraph-cli PRIVATE serd-0) # RDF-parser
  target_link_libraries(cgraph-cli PRIVATE /home/linuxbrew/.linuxbrew/lib/libserd-0.so)
  
  if(TRACE_SYMBOLS)
    target_link_options(cgraph-cli PRIVATE -rdynamic)
  endif()
endif()
