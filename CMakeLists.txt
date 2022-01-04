cmake_minimum_required(VERSION 2.8.12)

project(jitterentropy C)

option(STACK_PROTECTOR "Compile Jitter with stack protector enabled" ON)
option(INTERNAL_TIMER "Compile Jitter with the internal thread based timer" ON)
option(USE_OPENSSL "Compile Jitter and use OpenSSL or a fork such as AWS-LC" ON)

# CMake sets the variable MSVC to true automatically, replicate that for other compilers
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    set(CLANG 1)
elseif(CMAKE_C_COMPILER_ID MATCHES "GNU")
    set(GCC 1)
endif()

file(GLOB JITTER_SRC "src/*.c")
file(GLOB JITTER_BASE_HEADERS "src/*.h")
file(GLOB JITTER_ARCH_HEADERS "arch/*.h")

if(INTERNAL_TIMER)
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -DJENT_CONF_ENABLE_INTERNAL_TIMER")
endif()

if(USE_OPENSSL)
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -DOPENSSL")
endif()

if(MSVC)
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS}")
    set(JITTER_LINK_FLAGS "${JITTER_LINK_FLAGS}")
else()
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -fwrapv --param ssp-buffer-size=4 -fvisibility=hidden -fPIE -Wcast-align")
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -Wmissing-field-initializers -Wshadow -Wswitch-enum")
    set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -Wextra -Wall -pedantic -fPIC -O0 -fwrapv -Wconversion")
    set(JITTER_LINKER_FLAGS "-Wl,-z,relro,-z,now")
endif()


if(STACK_PROTECTOR)
    if(GCC)
        if(CMAKE_C_COMPILER_VERSION VERSION_GREATER "4.9.0")
            set(JITTER_C_FLAGS "${JITTER_C_FLAGS}  -fstack-protector-strong")
        else()
            set(JITTER_C_FLAGS "${JITTER_C_FLAGS}   -fstack-protector-all")
        endif()
    elseif(CLANG)
        set(JITTER_C_FLAGS "${JITTER_C_FLAGS} -fstack-protector-strong")
    endif()
endif()

set(CMAKE_SHARED_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS}  ${JITTER_LINKER_FLAGS}")
set(CMAKE_C_FLAGS ${JITTER_C_FLAGS})

add_library(${PROJECT_NAME} ${JITTER_SRC})
if(COMPILE_WITH_OPENSSL)
    find_package(OpenSSL REQUIRED)
    target_link_libraries(${PROJECT_NAME} PUBLIC OpenSSL::Crypto)
    target_include_directories(${PROJECT_NAME} PUBLIC ${OPENSSL_INCLUDE_DIR})
endif()

if(NOT MSVC)
    target_link_libraries(${PROJECT_NAME} PUBLIC pthread)
endif()

target_include_directories(${PROJECT_NAME} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)

add_subdirectory(tests/raw-entropy/recording_userspace)