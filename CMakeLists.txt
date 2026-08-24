cmake_minimum_required(VERSION 3.15)

# =========================================================================
# 1. Toolchain, must be before project()
# =========================================================================
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR riscv)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(TOOLCHAIN_PATH "/home/dxxdx/pub/RISC-V Embedded GCC15")

set(CMAKE_C_COMPILER   "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-gcc" CACHE FILEPATH "C compiler" FORCE)
set(CMAKE_CXX_COMPILER "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-g++" CACHE FILEPATH "CXX compiler" FORCE)
set(CMAKE_ASM_COMPILER "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-gcc" CACHE FILEPATH "ASM compiler" FORCE)

set(CMAKE_OBJCOPY "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-objcopy" CACHE FILEPATH "objcopy" FORCE)
set(CMAKE_OBJDUMP "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-objdump" CACHE FILEPATH "objdump" FORCE)
set(CMAKE_SIZE    "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-size" CACHE FILEPATH "size" FORCE)

set(CMAKE_AR     "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-gcc-ar" CACHE FILEPATH "ar" FORCE)
set(CMAKE_RANLIB "${TOOLCHAIN_PATH}/bin/riscv32-wch-elf-gcc-ranlib" CACHE FILEPATH "ranlib" FORCE)

if(NOT EXISTS "${CMAKE_C_COMPILER}")
    message(FATAL_ERROR "C compiler not found: ${CMAKE_C_COMPILER}")
endif()

if(NOT EXISTS "${CMAKE_OBJCOPY}")
    message(FATAL_ERROR "objcopy not found: ${CMAKE_OBJCOPY}")
endif()

# =========================================================================
# 2. Project
# =========================================================================
project(CH32V203C8U C CXX ASM)

message(STATUS "WCH CC      = ${CMAKE_C_COMPILER}")
message(STATUS "WCH CXX     = ${CMAKE_CXX_COMPILER}")
message(STATUS "WCH ASM     = ${CMAKE_ASM_COMPILER}")
message(STATUS "WCH OBJCOPY = ${CMAKE_OBJCOPY}")
message(STATUS "WCH OBJDUMP = ${CMAKE_OBJDUMP}")
message(STATUS "WCH SIZE    = ${CMAKE_SIZE}")

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

# =========================================================================
# 3. Target / linker script
# =========================================================================
set(TARGET_NAME CH32V203C8U)
set(LINKER_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/Ld/Link.ld")

if(NOT EXISTS "${LINKER_SCRIPT}")
    message(FATAL_ERROR "Linker script not found: ${LINKER_SCRIPT}")
endif()

# =========================================================================
# 4. Flags from native MounRiver Makefile
# =========================================================================
set(ARCH_FLAGS
        -march=rv32imacxw
        -mabi=ilp32
        -msmall-data-limit=8
)

set(COMMON_COMPILE_FLAGS
        ${ARCH_FLAGS}
        -msave-restore
        -fmax-errors=20
        -Og
        -fmessage-length=0
        -fsigned-char
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wunused
        -Wuninitialized
        -g3

)

set(COMMON_LINK_FLAGS
        ${ARCH_FLAGS}
        -msave-restore
        -fmax-errors=20
        -Og
        -fmessage-length=0
        -fsigned-char
        -ffunction-sections
        -fdata-sections
        -fno-common
        -Wunused
        -Wuninitialized
        -g3
        "-T${LINKER_SCRIPT}"
        -nostartfiles
        -Wl,--gc-sections
        "-Wl,-Map,${TARGET_NAME}.map"
        --specs=nano.specs
        --specs=nosys.specs
        -Wl,--print-memory-usage

)

# =========================================================================
# 5. Source files
# =========================================================================
file(GLOB_RECURSE PROJECT_C_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
)

file(GLOB_RECURSE PROJECT_ASM_SOURCES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.s"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.S"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.asm"
)

# Exclude CMake / build generated files
list(FILTER PROJECT_C_SOURCES EXCLUDE REGEX ".*/cmake-build-.*/.*")
list(FILTER PROJECT_C_SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER PROJECT_C_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")

list(FILTER PROJECT_ASM_SOURCES EXCLUDE REGEX ".*/cmake-build-.*/.*")
list(FILTER PROJECT_ASM_SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER PROJECT_ASM_SOURCES EXCLUDE REGEX ".*/CMakeFiles/.*")

set_source_files_properties(${PROJECT_ASM_SOURCES} PROPERTIES LANGUAGE ASM)

add_executable(${TARGET_NAME}
        ${PROJECT_C_SOURCES}
        ${PROJECT_ASM_SOURCES}
        User/system_IT.c
        User/system_IT.h
        dev/APP/App.c
        dev/APP/App.h
        dev/ETH/ETH_Dispatcher.c
        dev/ETH/ETH_Dispatcher.h
        dev/ETH/ARP.c
        dev/ETH/ARP.h
        dev/ETH/IPv4.c
        dev/ETH/IPv4.h
        dev/ETH/ETH_Transmitter.c
        dev/ETH/ETH_Transmitter.h
        dev/ETH/UDP.c
        dev/ETH/UDP.h
        dev/ETH/ICMP.c
        dev/ETH/ICMP.h
        dev/ETH/DHCP.h
        dev/USB/USBH.c
        dev/USB/USBH.h
        dev/USB/USBH_HW.c
        dev/APP/LocalShell.c
        dev/APP/LocalShell.h
        dev/APP/KeyBoardResolve.c
        dev/USB/readme.h
        dev/APP/LocalShellDaemon.c
        dev/APP/LocalShellDaemon.h
        dev/APP/LocalShellCommandDispatch.c
        dev/APP/LocalShellCommandDispatch.h
        dev/APP/app/echo.c
        dev/APP/app/echo.h
        dev/Displayer/displayerChip.c
        dev/Displayer/displayerChip.h
        dev/Displayer/displayerChips/ST7735S/chipST7735.c
        dev/Displayer/displayerChips/ST7735S/chipST7735.h
        "dev/Displayer/displayerChips/ST7789/chipST7789.c"
        "dev/Displayer/displayerChips/ST7789/chipST7789.h"
        dev/Displayer/displayerChips/ST7789/st7789.c
        dev/Displayer/displayerChips/ST7789/st7789.h
        dev/Displayer/displayerChips/ST7789/st7789_HW.c
        dev/APP/app/nmap.c
        dev/APP/app/nmap.h
        dev/ETH/TCP/ETH_TCPManager.c
        dev/ETH/TCP/ETH_TCPManager.h
        dev/ETH/NetworkManager.c
        dev/ETH/NetworkManager.h
        dev/ETH/TCP/TCPdebug.c
        dev/ETH/TCP/TCPdebug.h

)

set_target_properties(${TARGET_NAME} PROPERTIES
        OUTPUT_NAME "${TARGET_NAME}"
        SUFFIX ".elf"
)

# =========================================================================
# 6. Include directories
# =========================================================================
target_include_directories(${TARGET_NAME} PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/User"
        "${CMAKE_CURRENT_SOURCE_DIR}/Startup"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/inc"
        "${CMAKE_CURRENT_SOURCE_DIR}/Peripheral/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/Debug"
        "${CMAKE_CURRENT_SOURCE_DIR}/Core"
)

# Auto include all dirs containing .h files
file(GLOB_RECURSE PROJECT_HEADERS CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)

set(PROJECT_HEADER_DIRS "")
foreach(_header ${PROJECT_HEADERS})
    get_filename_component(_dir "${_header}" DIRECTORY)
    list(APPEND PROJECT_HEADER_DIRS "${_dir}")
endforeach()

list(REMOVE_DUPLICATES PROJECT_HEADER_DIRS)

target_include_directories(${TARGET_NAME} PRIVATE
        ${PROJECT_HEADER_DIRS}
)

# =========================================================================
# 7. Compile and link options
# =========================================================================
target_compile_options(${TARGET_NAME} PRIVATE
        ${COMMON_COMPILE_FLAGS}
        $<$<COMPILE_LANGUAGE:ASM>:-x>
        $<$<COMPILE_LANGUAGE:ASM>:assembler-with-cpp>
)

target_link_options(${TARGET_NAME} PRIVATE
        ${COMMON_LINK_FLAGS}
)

# =========================================================================
# 8. Static libraries
# =========================================================================
file(GLOB_RECURSE ALL_LIB_FILES CONFIGURE_DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/*.a"
)

if(ALL_LIB_FILES)
    list(FILTER ALL_LIB_FILES EXCLUDE REGEX ".*/cmake-build-.*/.*")
    list(FILTER ALL_LIB_FILES EXCLUDE REGEX ".*/build/.*")
    list(FILTER ALL_LIB_FILES EXCLUDE REGEX ".*/CMakeFiles/.*")

    target_link_libraries(${TARGET_NAME} PRIVATE
            ${ALL_LIB_FILES}
    )
endif()



# =========================================================================
# 9. Post-build outputs: HEX / LST / ASM / ELF copy
# =========================================================================
add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND "${CMAKE_OBJCOPY}"
        -O ihex
        "$<TARGET_FILE:${TARGET_NAME}>"
        "${TARGET_NAME}.hex"

        COMMAND /bin/sh -c
        "\"${CMAKE_OBJDUMP}\" --source --all-headers --demangle -M xw --line-numbers --wide \"$<TARGET_FILE:${TARGET_NAME}>\" > \"${TARGET_NAME}.lst\""

        COMMAND /bin/sh -c
        "\"${CMAKE_OBJDUMP}\" -S -d -M xw \"$<TARGET_FILE:${TARGET_NAME}>\" > \"${TARGET_NAME}.asm\""

        COMMAND "${CMAKE_COMMAND}"
        -E copy
        "$<TARGET_FILE:${TARGET_NAME}>"
        "${CMAKE_CURRENT_SOURCE_DIR}/${TARGET_NAME}.elf"

        WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
        COMMENT "Generating HEX/LST/ASM files and displaying memory usage..."
)
set(EANALY_EXE "/bin/eanaly")
if(EANALY_EXE)
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
            COMMAND "${EANALY_EXE}"
            "$<TARGET_FILE:${TARGET_NAME}>"

            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            COMMENT "Running WCH post-analysis..."
    )
endif()