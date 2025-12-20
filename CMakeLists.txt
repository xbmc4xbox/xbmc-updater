cmake_minimum_required(VERSION 3.5)
set(XBE_TITLE updater)
set(XBOX_ISO_DIR ${CMAKE_CURRENT_BINARY_DIR}/xiso)

set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(NXDK_DEBUG_FLAG "y" CACHE STRING "Enable debug build for the NXDK submodule. 'y' or 'n'.")
endif()

project(updater C CXX ASM)

include(FetchContent)
include(FindPkgConfig)
include(PostbuildAction)
include(PrebuildNXDK)

set(CMAKE_ASM_FLAGS_DEBUG "${CMAKE_ASM_FLAGS_DEBUG} -g -gdwarf-4 -Wall -Wextra")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -gdwarf-4 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -gdwarf-4 -Wall -Wextra")
set(CMAKE_EXE_LINKER_FLAGS_DEBUG "${CMAKE_EXE_LINKER_FLAGS_DEBUG} -debug")

set(CMAKE_ASM_FLAGS_RELEASE "${CMAKE_ASM_FLAGS_RELEASE} -O2")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

add_compile_options(
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:C>>:-gdwarf-4>
    $<$<AND:$<CONFIG:Debug>,$<COMPILE_LANGUAGE:CXX>>:-gdwarf-4>
)
add_compile_options(-Wno-error=uninitialized)

add_definitions(-DXBOX -DNXDK -DMEMP_NUM_NETBUF=6 -DMEMP_NUM_NETCONN=6)

add_executable(updater
    src/filesystem/HDDirectory.cpp
    src/filesystem/HDFile.cpp
    src/utils/CustomLaunch.cpp
    src/utils/JSONVariantParser.cpp
    src/utils/StringUtils.cpp
    src/utils/Variant.cpp
    src/Downloader.cpp
    src/main.cpp
    src/Updater.cpp
    src/Util.cpp
    lib/mbedtls/glue.c
)

target_include_directories(updater PRIVATE src lib)

# Bring in Microtar archive support
add_library(microtar STATIC lib/microtar/microtar.c lib/microtar/microtar.h)
target_link_libraries(updater PUBLIC microtar)

# Bring in the DVD drive automount support
target_link_libraries(updater PUBLIC ${NXDK_DIR}/lib/libnxdk_automount_d.lib)
target_link_options(updater PRIVATE "-include:_automount_d_drive")

# Bring in networking support
target_link_libraries(updater PUBLIC ${NXDK_DIR}/lib/libnxdk_net.lib)
target_include_directories(updater PRIVATE ${NXDK_DIR}/lib/net/lwip/src/include ${NXDK_DIR}/lib/net/nforceif/include ${NXDK_DIR}/lib/net/nvnetdrv)

# Bring in mbed TLS support
message(STATUS "Downloading Mbed TLS")
FetchContent_Declare(
  mbedtls
  GIT_REPOSITORY https://github.com/Mbed-TLS/mbedtls.git
  GIT_TAG        v3.6.4
  GIT_PROGRESS TRUE
)
set(CMAKE_C_SIMULATE_ID FALSE CACHE INTERNAL "Clang" FORCE)
set(ENABLE_PROGRAMS OFF CACHE BOOL "Disable mbedtls programs")
set(ENABLE_TESTING OFF CACHE BOOL "Disable Mbed TLS tests")
set(UNSAFE_BUILD OFF CACHE BOOL "Disable unsafe build options")
set(DISABLE_PACKAGE_CONFIG_AND_INSTALL OFF CACHE BOOL "Disable package config and install targets")
add_definitions(
  -DMBEDTLS_CHECK_RETURN=
  -DMBEDTLS_CONFIG_FILE=\"${CMAKE_CURRENT_SOURCE_DIR}/lib/mbedtls/my_mbedtls_config.h\"
)
FetchContent_MakeAvailable(mbedtls)
target_include_directories(mbedx509 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/lib/mbedtls)
target_link_libraries(updater PUBLIC mbedtls ${CMAKE_SOURCE_DIR}/lib/mbedtls/ws2_32.lib)

# Bring in crpyto functions to support SHA1 and RC4
add_subdirectory(lib/xbox_eeprom)
target_link_libraries(updater PUBLIC xbox-eeprom)

# Bring in JSON library
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz)
FetchContent_MakeAvailable(json)

target_link_libraries(updater PRIVATE nlohmann_json::nlohmann_json)

#Pre-build commands

#Let's download the latest CA certificates
message(STATUS "Downloading CA certificates...")
file(DOWNLOAD
    https://curl.se/ca/cacert.pem
    ${CMAKE_SOURCE_DIR}/src/assets/cacert.pem
    SHOW_PROGRESS
    STATUS download_status
)

#Post-build commands
add_xbox_build_steps(updater ${XBE_TITLE} ${XBOX_ISO_DIR})
