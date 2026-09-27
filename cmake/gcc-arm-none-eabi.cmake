set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

set(TOOLCHAIN_PREFIX                arm-none-eabi-)

# 自动探测 GCC：优先 Bundle Manager，其次 winget / 系统安装
# cube-cmake 会将 Bundle Manager GCC 加入 PATH，_gcc_hints 作为补充保障
file(GLOB _gcc_hints
    "$ENV{CUBE_BUNDLE_PATH}/gnu-tools-for-stm32/*/bin"
    "$ENV{APPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin"
    "$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin"
    "$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/Arm.GnuArmEmbeddedToolchain*/*/bin"
    "C:/Program Files (x86)/GNU Arm Embedded Toolchain/*/bin"
    "C:/Program Files/GNU Arm Embedded Toolchain/*/bin"
    "C:/Program Files/GNU Tools Arm Embedded/*/bin"
)

if(_gcc_hints)
    find_program(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc     HINTS ${_gcc_hints} REQUIRED)
    find_program(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++     HINTS ${_gcc_hints})
    find_program(CMAKE_ASM_COMPILER ${TOOLCHAIN_PREFIX}gcc     HINTS ${_gcc_hints})
    find_program(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy HINTS ${_gcc_hints})
    find_program(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size    HINTS ${_gcc_hints})
else()
    set(CMAKE_C_COMPILER   ${TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_ASM_COMPILER ${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER ${TOOLCHAIN_PREFIX}g++)
    set(CMAKE_OBJCOPY      ${TOOLCHAIN_PREFIX}objcopy)
    set(CMAKE_SIZE         ${TOOLCHAIN_PREFIX}size)
endif()

set(CMAKE_EXECUTABLE_SUFFIX_ASM ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C   ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# MCU flags（由 keil2cubevscode.ps1 自动识别）
set(TARGET_FLAGS "-mcpu=cortex-m33 -mthumb -mfpu=fpv5-sp-d16 -mfloat-abi=hard")

set(CMAKE_C_FLAGS   "${CMAKE_C_FLAGS} ${TARGET_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS "${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

# 链接选项（Keil 工程无 syscalls.c，使用 nosys.specs）
set(CMAKE_EXE_LINKER_FLAGS "${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T \"${CMAKE_SOURCE_DIR}/STM32U575RITx_FLASH.ld\"")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs --specs=nosys.specs")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=${CMAKE_PROJECT_NAME}.map,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -u _printf_float")   # 支持 printf %f（SHT20温湿度打印）