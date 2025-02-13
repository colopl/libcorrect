cmake_minimum_required(VERSION 3.25)

project(Correct
        VERSION 2.0.0
        LANGUAGES C
        DESCRIPTION "Forward Error Correction Library")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)        

include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckCSourceCompiles)
include(CMakePushCheckState)
include(CheckCCompilerFlag)

# Compiler and build settings
if(MSVC)
    set(LIBM "")
    add_compile_options(/W4 /WX)
else()
    set(LIBM "m")
    add_compile_options(-fPIC -Wall -Werror)
    
    check_c_compiler_flag(-Wpedantic COMPILER_SUPPORTS_WPEDANTIC)
    if(COMPILER_SUPPORTS_WPEDANTIC)
        add_compile_options(-Wpedantic)
    endif()

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        add_compile_options(-g3 -O0)
        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            # Sanitizer options
            option(USE_ASAN_UBSAN "Use AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
            option(USE_MSAN "Use MemorySanitizer" ON)
            
            if(USE_MSAN AND HAVE_MSAN)
                if(USE_ASAN_UBSAN)
                    message(FATAL_ERROR "MemorySanitizer cannot be used with AddressSanitizer")
                endif()
                add_compile_options(-fsanitize=memory -fno-optimize-sibling-calls -fno-omit-frame-pointer)
                add_link_options(-fsanitize=memory)
            elseif(USE_ASAN_UBSAN AND HAVE_ASAN_UBSAN)
                add_compile_options(-fsanitize=address -fno-optimize-sibling-calls -fno-omit-frame-pointer)
                add_link_options(-fsanitize=address)
            endif()        
        endif()
    else()
        add_compile_options(-O2)
        if(CMAKE_BUILD_TYPE STREQUAL "Profiling")
            add_compile_options(-g3)
        endif()
    endif()
endif()

# Library detection
find_library(FEC fec)
check_library_exists(FEC dotprod "" HAVE_LIBFEC)

# SSE detection
if(NOT CMAKE_CROSSCOMPILING)
    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_DEFINITIONS -march=native)
    check_c_source_compiles("
        #include <x86intrin.h>
        int main(void) {
            __m128i a;
            __m128i b;
            __m128i c = _mm_min_epu16(a, b);
            return 0;
        }" HAVE_SSE)
    cmake_pop_check_state()
endif()

if(HAVE_SSE)
    add_compile_options(-msse4.1)
endif()

# Build settings
set(CMAKE_MACOSX_RPATH ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib)

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add subdirectories for source files
add_subdirectory(src)

# Header file installation preparation
set(INSTALL_HEADERS "${PROJECT_BINARY_DIR}/include/correct.h")

add_custom_target(correct-h ALL 
    COMMAND ${CMAKE_COMMAND} -E copy 
    ${PROJECT_SOURCE_DIR}/include/correct.h 
    ${PROJECT_BINARY_DIR}/include/correct.h
    COMMENT "Copying correct.h"
    VERBATIM)

# Object files configuration
if(HAVE_SSE)
    set(correct_obj_files 
        $<TARGET_OBJECTS:correct-reed-solomon>
        $<TARGET_OBJECTS:correct-convolutional>
        $<TARGET_OBJECTS:correct-convolutional-sse>)
    list(APPEND INSTALL_HEADERS "${PROJECT_BINARY_DIR}/include/correct-sse.h")
    add_custom_target(correct-sse-h ALL 
        COMMAND ${CMAKE_COMMAND} -E copy 
        ${PROJECT_SOURCE_DIR}/include/correct-sse.h 
        ${PROJECT_BINARY_DIR}/include/correct-sse.h
        COMMENT "Copying correct-sse.h"
        VERBATIM)
else()
    set(correct_obj_files 
        $<TARGET_OBJECTS:correct-reed-solomon>
        $<TARGET_OBJECTS:correct-convolutional>)
endif()

# Main library targets
add_library(correct SHARED ${correct_obj_files})
add_library(correct_static STATIC ${correct_obj_files})
set_target_properties(correct_static PROPERTIES 
    OUTPUT_NAME "correct"
    POSITION_INDEPENDENT_CODE ON)

if(HAVE_SSE)
    target_compile_definitions(correct PUBLIC HAVE_SSE=1)
    target_compile_definitions(correct_static PUBLIC HAVE_SSE=1)
endif()

# Additional components
add_subdirectory(util)
add_subdirectory(tests)
add_subdirectory(tools)
# add_subdirectory(benchmarks)

# Installation rules
include(GNUInstallDirs)
install(TARGETS correct correct_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
install(FILES ${INSTALL_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# FEC shim library
add_library(fec_shim_static STATIC EXCLUDE_FROM_ALL 
    src/fec_shim.c 
    ${correct_obj_files})
add_library(fec_shim_shared SHARED EXCLUDE_FROM_ALL 
    src/fec_shim.c 
    ${correct_obj_files})

set_target_properties(fec_shim_static PROPERTIES 
    OUTPUT_NAME "fec"
    POSITION_INDEPENDENT_CODE ON)
set_target_properties(fec_shim_shared PROPERTIES 
    OUTPUT_NAME "fec")

add_custom_target(fec-shim-h 
    COMMAND ${CMAKE_COMMAND} -E copy 
    ${PROJECT_SOURCE_DIR}/include/fec_shim.h 
    ${PROJECT_BINARY_DIR}/include/fec.h
    COMMENT "Copying fec_shim.h"
    VERBATIM)
add_custom_target(shim DEPENDS fec_shim_static fec_shim_shared fec-shim-h)

# Optional FEC shim installation
install(TARGETS fec_shim_static fec_shim_shared
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    OPTIONAL
)
install(FILES ${PROJECT_BINARY_DIR}/include/fec.h 
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    OPTIONAL
)
