cmake_minimum_required(VERSION 3.22)

#
# Root CMakeLists.txt（用户可编辑）
# cmake/stm32cubemx/CMakeLists.txt 包含从 Keil 迁移的所有源文件，无需手动修改
#

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Debug")
endif()

set(CMAKE_PROJECT_NAME Wifi_Gyroscope)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(${CMAKE_PROJECT_NAME})
message("Build type: ${CMAKE_BUILD_TYPE}")

enable_language(C ASM)

add_executable(${CMAKE_PROJECT_NAME})

# 迁移的所有源文件、include 路径、宏定义在此子目录中
add_subdirectory(cmake/stm32cubemx)

# ---- 用户扩展区域 ----
target_sources(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的 .c 文件
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的头文件路径
)

target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的宏定义
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    stm32cubemx
    # 在此添加用户自定义静态库
)

# 编译后生成 HEX / BIN 并输出内存占用（必须在 add_executable 同一目录）
add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary
            $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
            ${PROJECT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${CMAKE_PROJECT_NAME}>
    COMMENT "Generating ${CMAKE_PROJECT_NAME}.hex / .bin"
)