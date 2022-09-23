cmake_minimum_required(VERSION 3.15.7)

project(jitterentropy C)
set(CMAKE_C_STANDARD 99)

option(STACK_PROTECTOR "Compile Jitter with stack protector enabled" ON)
option(INTERNAL_TIMER "Compile Jitter with the internal thread based timer" ON)
option(EXTERNAL_CRYPTO "Compile Jitter and use an external libcrypto, valid options are [AWSLC, OPENSSL, LIBGCRYPT]")

set(EXTERNAL_CRYPTO_SUPPORTED AWSLC OPENSSL LIBGCRYPT)
if (NOT EXTERNAL_CRYPTO)
    message(STATUS "Using internal functions for everything - EXTERNAL_CRYPTO not set")
elseif( NOT (EXTERNAL_CRYPTO IN_LIST EXTERNAL_CRYPTO_SUPPORTED))
    message(FATAL_ERROR "Unknown EXTERNAL_CRYPTO option ${EXTERNAL_CRYPTO}")
endif()


# CMake defines the variable MSVC to true automatically when building with MSVC, replicate that for other compilers
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CLANG 1)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(GCC 1)
endif()

add_compile_options(
    ### C Based Language and ID.
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-fdiagnostics-color=always>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcolor-diagnostics>

    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wall>
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Wextra>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wpedantic> # Note not used in GNU to allow C99 designated initializers.
    $<$<COMPILE_LANG_AND_ID:C,Clang,GNU>:-Werror>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Weverything>

    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-c++98-compat>
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-c++98-compat-pedantic>

    # Allow clang and gnu pramgmas.
    $<$<COMPILE_LANG_AND_ID:C,GNU>:-Wno-unknown-pragmas>

    # Documentation types
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcomment-block-commands=retval> # Doesn't recongize doxygen retval
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-fcomment-block-commands=copydetails> # Doesn't recongize doxygen copydetails
)


add_library(${PROJECT_NAME} )
target_sources(${PROJECT_NAME}
  PRIVATE
    src/jitterentropy-base.c
    src/jitterentropy-base.h
    src/jitterentropy-gcd.c
    src/jitterentropy-gcd.h
    src/jitterentropy-health.c
    src/jitterentropy-health.h
    src/jitterentropy-noise.c
    src/jitterentropy-noise.h
    src/jitterentropy-sha3.c
    src/jitterentropy-sha3.h
    src/jitterentropy-timer.c
    src/jitterentropy-timer.h
)

target_compile_definitions(${PROJECT_NAME}
  PRIVATE
    $<$<BOOL:INTERNAL_TIMER>:JENT_CONF_ENABLE_INTERNAL_TIMER>
    $<$<BOOL:EXTERNAL_CRYPTO>:${EXTERNAL_CRYPTO}>
)

target_compile_options(${PROJECT_NAME}
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,MSVC>:/Od /W4 /DYNAMICBASE>
    # Note -Wcast-align -Wmissing-field-initializers -Wshadow -Wswitch-enum -Wextra -Wall -pedantic are all enabled above.
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-fwrapv --param ssp-buffer-size=4 -fvisibility=hidden -fPIE -fPIC -O0 -fwrapv -Wconversion>

    $<$<AND:$<BOOL:STACK_PROTECTOR>,$<COMPILE_LANG_AND_ID:C,GNU>,$<VERSION_LESS_EQUAL:$<C_COMPILER_VERSION>,"4.9.0">>:-fstack-protector-all>
    $<$<AND:$<BOOL:STACK_PROTECTOR>,$<COMPILE_LANG_AND_ID:C,GNU>,$<VERSION_GREATER:$<C_COMPILER_VERSION>,"4.9.0">>:-fstack-protector-strong>
    $<$<AND:$<BOOL:STACK_PROTECTOR>,$<COMPILE_LANG_AND_ID:C,Clang>>:-fstack-protector-strong>

    # These are expected based on coding style.
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-language-extension-token> # for asm
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-padded> # padded struct `rand_data` in jitterentropy.h - reorder values uint64_t on top then smaller sized variable after.
    $<$<COMPILE_LANG_AND_ID:C,Clang>:-Wno-reserved-macro-identifier> # use of `_` before name of defined header guards
)

target_link_options(${PROJECT_NAME}
  PRIVATE
    $<$<COMPILE_LANG_AND_ID:C,GNU,Clang>:-Wl,-z,relro,-z,now>
)

target_include_directories(${PROJECT_NAME}
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if(EXTERNAL_CRYPTO STREQUAL "AWSLC")
    # This header is unique from OpenSSL
    set(EXTERNAL_CRYPTO_HEADER "openssl/base.h")
    set(EXTERNAL_CRYPTO_LIBRARY_NAME libcrypto)
elseif(EXTERNAL_CRYPTO STREQUAL "OPENSSL")
    set(EXTERNAL_CRYPTO_HEADER "openssl/crypto.h")
    set(EXTERNAL_CRYPTO_LIBRARY_NAME libcrypto)
elseif(EXTERNAL_CRYPTO STREQUAL "LIBGCRYPT")
    set(EXTERNAL_CRYPTO_HEADER "g10lib.h")
    set(EXTERNAL_CRYPTO_LIBRARY_NAME libgcrypt)
endif()

if(EXTERNAL_CRYPTO)
    find_path(LIBCRYPTO_INCLUDE_DIR
            NAMES "${EXTERNAL_CRYPTO_HEADER}"
            PATH_SUFFIXES include
            )
    message(STATUS "Found external crypto headers at ${LIBCRYPTO_INCLUDE_DIR}")
    target_include_directories(${PROJECT_NAME} PUBLIC ${LIBCRYPTO_INCLUDE_DIR})
    if(BUILD_SHARED_LIBS)
        find_library(LIBCRYPTO_LIBRARY
                NAMES "${EXTERNAL_CRYPTO_LIBRARY_NAME}.so" "${EXTERNAL_CRYPTO_LIBRARY_NAME}.dylib" "${EXTERNAL_CRYPTO_LIBRARY_NAME}.dll"
                HINTS "${CMAKE_INSTALL_PREFIX}"
                PATH_SUFFIXES build/crypto build lib64 lib
                )
    else()
        find_library(LIBCRYPTO_LIBRARY
                NAMES "${EXTERNAL_CRYPTO_LIBRARY_NAME}.so" "${EXTERNAL_CRYPTO_LIBRARY_NAME}.dylib" "${EXTERNAL_CRYPTO_LIBRARY_NAME}.dll"
                HINTS "${CMAKE_INSTALL_PREFIX}"
                PATH_SUFFIXES build/crypto build lib64 lib
                )
    endif()
    message(STATUS "Found external crypto library ${LIBCRYPTO_LIBRARY}")
    target_link_libraries(${PROJECT_NAME} PUBLIC ${LIBCRYPTO_LIBRARY})

endif()

if(INTERNAL_TIMER)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
  find_package(Threads)
endif()

target_link_libraries(${PROJECT_NAME}
  PUBLIC
    $<$<BOOL:${INTERNAL_TIMER}>:Threads::Threads>
)

add_subdirectory(tests/gcd)
add_subdirectory(tests/raw-entropy/recording_userspace)
