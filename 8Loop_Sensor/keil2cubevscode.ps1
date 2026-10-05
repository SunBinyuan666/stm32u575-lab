# ===========================================================================
# keil2cubevscode.ps1 - STM32 Keil -> VS Code (STM32CubeIDE Extension)
# 生成与 cube-cmake / Bundle Manager GCC 兼容的 CubeMX 式工程结构
# 无需手动配置环境变量，安装 STM32CubeIDE for VS Code 扩展后即可使用
#
# 用法：powershell -ExecutionPolicy Bypass -File keil2cubevscode.ps1
#       powershell -ExecutionPolicy Bypass -File keil2cubevscode.ps1 -UvprojxPath "D:\proj\proj.uvprojx"
# ===========================================================================
param(
    [string]$UvprojxPath = ""
)

Set-StrictMode -Off
$ErrorActionPreference = "Stop"
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

# ---------------------------------------------------------------------------
# 彩色输出
# ---------------------------------------------------------------------------
function Write-Info  { param([string]$msg) Write-Host $msg -ForegroundColor Cyan }
function Write-OK    { param([string]$msg) Write-Host "[OK] $msg" -ForegroundColor Green }
function Write-Warn  { param([string]$msg) Write-Host "[!]  $msg" -ForegroundColor Yellow }
function Write-Err   { param([string]$msg) Write-Host "[ERR] $msg" -ForegroundColor Red }
function Write-Step  { param([string]$msg) Write-Host "`n==> $msg" -ForegroundColor Magenta }

# ---------------------------------------------------------------------------
# 路径工具
# ---------------------------------------------------------------------------
function To-Slash { param([string]$p) $p.Replace('\', '/') }

function Resolve-ProjPath {
    param([string]$base, [string]$rel)
    try { return [System.IO.Path]::GetFullPath([System.IO.Path]::Combine($base, $rel)) }
    catch { return $null }
}

function Get-CommonAncestor {
    param([string[]]$paths)
    if ($paths.Count -eq 0) { return "" }
    if ($paths.Count -eq 1) { return [System.IO.Path]::GetDirectoryName($paths[0]) }
    $parts = $paths[0] -split '[/\\]'
    for ($pi = 1; $pi -lt $paths.Count; $pi++) {
        $op  = $paths[$pi] -split '[/\\]'
        $len = [Math]::Min($parts.Count, $op.Count)
        $m   = 0
        for ($i = 0; $i -lt $len; $i++) {
            if ($parts[$i] -ieq $op[$i]) { $m = $i + 1 } else { break }
        }
        if ($m -eq 0) { return "" }
        $parts = $parts[0..($m - 1)]
    }
    return ($parts -join '\')
}

# ---------------------------------------------------------------------------
# 芯片参数推导
# ---------------------------------------------------------------------------
function Get-MCUFlags {
    param([string]$chip)
    if ($chip -match '^STM32([A-Z])(\d{3})') {
        $s = $Matches[1]; $n = [int]$Matches[2]
        if     ($s -eq 'H')                 { return "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'F' -and $n -ge 700) { return "-mcpu=cortex-m7 -mthumb -mfpu=fpv5-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'F' -and $n -ge 400) { return "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'F' -and $n -ge 300) { return "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'L' -and $n -ge 400) { return "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'G' -and $n -ge 400) { return "-mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard" }
        elseif ($s -eq 'F' -and $n -ge 100) { return "-mcpu=cortex-m3 -mthumb" }
        elseif ($s -eq 'L' -and $n -ge 100) { return "-mcpu=cortex-m3 -mthumb" }
        elseif ($s -eq 'G' -and $n -lt 100) { return "-mcpu=cortex-m0plus -mthumb" }
        elseif ($s -eq 'L' -and $n -lt 100) { return "-mcpu=cortex-m0plus -mthumb" }
        else                                 { return "-mcpu=cortex-m0 -mthumb" }
    }
    Write-Warn "无法自动识别芯片核心，已设为 cortex-m3，请手动确认 cmake/gcc-arm-none-eabi.cmake"
    return "-mcpu=cortex-m3 -mthumb"
}

function Get-HalDeviceMacro {
    param([string]$chip)
    if ($chip -match '^(STM32[A-Z]\d{3})([A-Z])([A-Z0-9])') {
        $base = $Matches[1]; $size = $Matches[3]
        if ($base -match '^STM32[FH][4-9]' -or $base -match '^STM32H') { return "${base}xx" }
        if ($base -match '^STM32(F0|L0|G0)')  { return "${base}x${size}" }
        if ($base -match '^STM32(L4|G4|WB|WL)') { return "${base}xx" }
        if ($base -match '^STM32F10[57]') { return "${base}x${size}" }
        if ($base -match '^STM32F1') {
            $sUp = $size.ToUpper()
            $mapped = switch ($sUp) {
                '4' { '6' }
                '6' { '6' }
                '8' { 'B' }
                'B' { 'B' }
                'C' { 'E' }
                'D' { 'E' }
                'E' { 'E' }
                'F' { 'G' }
                'G' { 'G' }
                default { $sUp }
            }
            return "${base}x${mapped}"
        }
        return "${base}x${size}"
    }
    return $null
}

function Get-StdPeriphDensityMacro {
    param([int64]$flashBytes, [string]$chip)
    # F1 Connectivity Line
    if ($chip -match 'F10[57]') { return "STM32F10X_CL" }
    # F1 density (LD/MD/HD/XL)
    if ($chip -match 'STM32F1') {
        if ($flashBytes -le 32768)   { return "STM32F10X_LD" }
        if ($flashBytes -le 131072)  { return "STM32F10X_MD" }
        if ($flashBytes -le 524288)  { return "STM32F10X_HD" }
        return "STM32F10X_XL"
    }
    # F2
    if ($chip -match 'STM32F2')                              { return "STM32F2XX" }
    # F3
    if ($chip -match 'STM32F37')                             { return "STM32F37X" }
    if ($chip -match 'STM32F3')                              { return "STM32F30X" }
    # F4 sub-family (order matters: specific before general)
    if ($chip -match 'STM32F40[15]|STM32F407|STM32F41[57]') { return "STM32F40_41xxx" }
    if ($chip -match 'STM32F427|STM32F437')                  { return "STM32F427_437xx" }
    if ($chip -match 'STM32F429|STM32F439')                  { return "STM32F429_439xx" }
    if ($chip -match 'STM32F401')                            { return "STM32F401xx" }
    if ($chip -match 'STM32F411')                            { return "STM32F411xE" }
    if ($chip -match 'STM32F446')                            { return "STM32F446xx" }
    if ($chip -match 'STM32F4')                              { return "STM32F4XX" }
    # F0
    if ($chip -match 'STM32F0')                              { return "STM32F0XX" }
    # L1 density
    if ($chip -match 'STM32L1') {
        if ($flashBytes -le 131072)  { return "STM32L1XX_MD" }
        if ($flashBytes -le 262144)  { return "STM32L1XX_MDP" }
        return "STM32L1XX_HD"
    }
    # L4/G0/G4/H7/WB 等系列没有 StdPeriph，返回 null 让 Keil 原有 define 直接透传
    return $null
}

function Get-FlashSizeBytes {
    param([string]$hexOrDec)
    $s = $hexOrDec.Trim()
    if ($s -match '^0[xX]') { return [Convert]::ToInt64($s, 16) }
    return [int64]$s
}

function Detect-LibraryType {
    param([string]$defines, [string]$includePaths, [string[]]$filePaths, [string]$projDir)
    if ($defines      -match 'USE_HAL_DRIVER')      { return 'HAL' }
    if ($defines      -match 'USE_STDPERIPH_DRIVER') { return 'StdPeriph' }
    if ($includePaths -match 'HAL_Driver')           { return 'HAL' }
    if ($includePaths -match 'StdPeriph_Driver')     { return 'StdPeriph' }
    foreach ($fp in $filePaths) {
        if ($fp -match 'HAL_Driver') { return 'HAL' }
        if ($fp -match 'StdPeriph')  { return 'StdPeriph' }
    }
    if (Get-ChildItem -Path $projDir -Filter "*.ioc" -Recurse -ErrorAction SilentlyContinue) { return 'HAL' }
    return 'Unknown'
}

function Find-GccStartupFile {
    param([string]$root, [string[]]$asmFiles, [string]$chip = "")
    # 策略1：已登记的 .s 文件本身是 GCC 语法
    foreach ($f in $asmFiles) {
        if (-not (Test-Path $f)) { continue }
        $c = Get-Content $f -Raw -ErrorAction SilentlyContinue
        if ($c -match '\.thumb_set|\.weak|\.section\s+\.isr_vector') { return $f }
    }
    # 策略2：从 ARM 版路径推导 GCC 版路径（../gcc/ 或 ../gcc_ride7/）
    foreach ($f in $asmFiles) {
        $parent = [System.IO.Path]::GetDirectoryName([System.IO.Path]::GetDirectoryName($f))
        foreach ($d in @('gcc_ride7','gcc')) {
            $c = [System.IO.Path]::Combine($parent, $d, [System.IO.Path]::GetFileName($f))
            if (Test-Path $c) { return $c }
            $gd = [System.IO.Path]::Combine($parent, $d)
            if (Test-Path $gd) {
                $h = Get-ChildItem $gd -Filter "startup_*.s" -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($h) { return $h.FullName }
            }
        }
    }
    # 策略3：在项目内递归找同名 GCC 版（CubeMX + Keil 工程）
    foreach ($f in $asmFiles) {
        $fname = [System.IO.Path]::GetFileName($f)
        if ($fname -notmatch '^startup_') { continue }
        $h = Get-ChildItem -Path $root -Filter $fname -Recurse -ErrorAction SilentlyContinue |
             Where-Object { $_.FullName -match '[/\\]gcc[/\\]' } | Select-Object -First 1
        if ($h) { return $h.FullName }
    }
    # 策略4：HAL 标准路径
    $halPaths = @("STM32F1xx","STM32F4xx","STM32F7xx","STM32H7xx","STM32L4xx","STM32G4xx") |
        ForEach-Object { "$root\Drivers\CMSIS\Device\ST\$_\Source\Templates\gcc" }
    foreach ($p in $halPaths) {
        if (-not (Test-Path $p)) { continue }
        $cands = Get-ChildItem $p -Filter "startup_*.s" -ErrorAction SilentlyContinue
        if (-not $cands) { continue }
        if ($chip) {
            $dm = Get-HalDeviceMacro $chip
            if ($dm) {
                $ex = $cands | Where-Object { $_.Name -ieq ("startup_" + $dm.ToLower() + ".s") } | Select-Object -First 1
                if ($ex) { return $ex.FullName }
            }
            if ($chip -match 'STM32([A-Z]\d{3})') {
                $h = $cands | Where-Object { $_.Name -match $Matches[1].ToLower() } | Sort-Object Name | Select-Object -First 1
                if ($h) { return $h.FullName }
            }
        }
        $best = $cands | Where-Object { $_.Name -notmatch 'stm32f10[012]' } | Sort-Object Name | Select-Object -First 1
        if (-not $best) { $best = $cands | Sort-Object Name | Select-Object -First 1 }
        if ($best) { return $best.FullName }
    }
    # 策略5：兜底递归
    foreach ($d in (Get-ChildItem $root -Directory -Recurse -Filter "gcc" -ErrorAction SilentlyContinue)) {
        $h = Get-ChildItem $d.FullName -Filter "startup_*.s" -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($h) { return $h.FullName }
    }
    return $null
}

function Fix-CoreCm3 {
    param([string[]]$sourcePaths)
    $f = $sourcePaths | Where-Object { $_ -match '[/\\]core_cm3\.c$' } | Select-Object -First 1
    if (-not $f -or -not (Test-Path $f)) { return }
    $c = [System.IO.File]::ReadAllText($f, $Utf8NoBom)
    # 匹配 strex/strexb/strexh inline asm 中的 "=r"(result) — 空格可选，顺序可变
    $pat1 = '("=r"\s*\(result\))(\s*:\s*"r"\s*\(addr\)\s*,\s*"r"\s*\(value\))'
    $pat2 = '("=r"\s*\(result\))(\s*:\s*"r"\s*\(value\)\s*,\s*"r"\s*\(addr\))'
    $hasIssue = ($c -match $pat1) -or ($c -match $pat2)
    if (-not $hasIssue) { Write-OK "core_cm3.c：STREX 约束检查通过"; return }
    Write-Warn "core_cm3.c：检测到 STREX 寄存器约束 Bug，尝试修复..."
    try {
        $fixed = $c -replace $pat1, '"=&r" (result)$2'
        $fixed = $fixed -replace $pat2, '"=&r" (result)$2'
        [System.IO.File]::WriteAllText($f, $fixed, $Utf8NoBom)
        Write-OK "core_cm3.c：已修复（=r → =&r）"
    } catch {
        Write-Warn ('core_cm3.c: auto-fix failed. Manually change "=r"(result) to "=&r"(result) in __STREXB and __STREXH')
    }
}

function Write-Utf8 {
    param([string]$path, [string]$content)
    $dir = Split-Path $path -Parent
    if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [System.IO.File]::WriteAllText($path, $content, $Utf8NoBom)
}

# ===========================================================================
# 生成器函数（CubeMX 兼容结构）
# ===========================================================================

function New-GccArmCmake {
    param([string]$root, [string]$mcuFlags, [string]$ldFile)
    $tf = ($mcuFlags -split ' ' | Where-Object { $_ }) -join ' '
    $ld = To-Slash $ldFile
    Write-Utf8 "$root\cmake\gcc-arm-none-eabi.cmake" @"
set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

set(TOOLCHAIN_PREFIX                arm-none-eabi-)

# 自动探测 GCC：优先 Bundle Manager，其次 winget / 系统安装
# cube-cmake 会将 Bundle Manager GCC 加入 PATH，_gcc_hints 作为补充保障
file(GLOB _gcc_hints
    "`$ENV{CUBE_BUNDLE_PATH}/gnu-tools-for-stm32/*/bin"
    "`$ENV{APPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin"
    "`$ENV{LOCALAPPDATA}/stm32cube/bundles/gnu-tools-for-stm32/*/bin"
    "`$ENV{LOCALAPPDATA}/Microsoft/WinGet/Packages/Arm.GnuArmEmbeddedToolchain*/*/bin"
    "C:/Program Files (x86)/GNU Arm Embedded Toolchain/*/bin"
    "C:/Program Files/GNU Arm Embedded Toolchain/*/bin"
    "C:/Program Files/GNU Tools Arm Embedded/*/bin"
)

if(_gcc_hints)
    find_program(CMAKE_C_COMPILER   `${TOOLCHAIN_PREFIX}gcc     HINTS `${_gcc_hints} REQUIRED)
    find_program(CMAKE_CXX_COMPILER `${TOOLCHAIN_PREFIX}g++     HINTS `${_gcc_hints})
    find_program(CMAKE_ASM_COMPILER `${TOOLCHAIN_PREFIX}gcc     HINTS `${_gcc_hints})
    find_program(CMAKE_OBJCOPY      `${TOOLCHAIN_PREFIX}objcopy HINTS `${_gcc_hints})
    find_program(CMAKE_SIZE         `${TOOLCHAIN_PREFIX}size    HINTS `${_gcc_hints})
else()
    set(CMAKE_C_COMPILER   `${TOOLCHAIN_PREFIX}gcc)
    set(CMAKE_ASM_COMPILER `${CMAKE_C_COMPILER})
    set(CMAKE_CXX_COMPILER `${TOOLCHAIN_PREFIX}g++)
    set(CMAKE_OBJCOPY      `${TOOLCHAIN_PREFIX}objcopy)
    set(CMAKE_SIZE         `${TOOLCHAIN_PREFIX}size)
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
set(TARGET_FLAGS "$tf")

set(CMAKE_C_FLAGS   "`${CMAKE_C_FLAGS} `${TARGET_FLAGS} -Wall -fdata-sections -ffunction-sections")
set(CMAKE_ASM_FLAGS "`${CMAKE_C_FLAGS} -x assembler-with-cpp -MMD -MP")
set(CMAKE_C_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_C_FLAGS_RELEASE "-Os -g0")

set(CMAKE_CXX_FLAGS "`${CMAKE_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")
set(CMAKE_CXX_FLAGS_DEBUG   "-O0 -g3")
set(CMAKE_CXX_FLAGS_RELEASE "-Os -g0")

# 链接选项（Keil 工程无 syscalls.c，使用 nosys.specs）
set(CMAKE_EXE_LINKER_FLAGS "`${TARGET_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "`${CMAKE_EXE_LINKER_FLAGS} -T \"`${CMAKE_SOURCE_DIR}/$ld\"")
set(CMAKE_EXE_LINKER_FLAGS "`${CMAKE_EXE_LINKER_FLAGS} --specs=nano.specs --specs=nosys.specs")
set(CMAKE_EXE_LINKER_FLAGS "`${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=`${CMAKE_PROJECT_NAME}.map,--gc-sections")
set(CMAKE_EXE_LINKER_FLAGS "`${CMAKE_EXE_LINKER_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_EXE_LINKER_FLAGS "`${CMAKE_EXE_LINKER_FLAGS} -Wl,--print-memory-usage")
# -u _printf_float   # 取消注释以支持 printf %f/%e/%g
"@
    Write-OK "cmake/gcc-arm-none-eabi.cmake"
}

function New-CubePresetsJson {
    param([string]$root)
    Write-Utf8 "$root\CMakePresets.json" @'
{
    "version": 3,
    "configurePresets": [
        {
            "name": "default",
            "hidden": true,
            "generator": "Ninja",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "toolchainFile": "${sourceDir}/cmake/gcc-arm-none-eabi.cmake",
            "cacheVariables": {}
        },
        {
            "name": "Debug",
            "inherits": "default",
            "cacheVariables": { "CMAKE_BUILD_TYPE": "Debug" }
        },
        {
            "name": "Release",
            "inherits": "default",
            "cacheVariables": { "CMAKE_BUILD_TYPE": "Release" }
        }
    ],
    "buildPresets": [
        { "name": "Debug",   "configurePreset": "Debug"   },
        { "name": "Release", "configurePreset": "Release" }
    ]
}
'@
    Write-OK "CMakePresets.json"
}

function New-RootCMakeLists {
    param([string]$root, [string]$projName)
    if (Test-Path "$root\CMakeLists.txt") { Write-Warn "已存在，跳过：CMakeLists.txt"; return }
    Write-Utf8 "$root\CMakeLists.txt" @"
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

set(CMAKE_PROJECT_NAME $projName)
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

project(`${CMAKE_PROJECT_NAME})
message("Build type: `${CMAKE_BUILD_TYPE}")

enable_language(C ASM)

add_executable(`${CMAKE_PROJECT_NAME})

# 迁移的所有源文件、include 路径、宏定义在此子目录中
add_subdirectory(cmake/stm32cubemx)

# ---- 用户扩展区域 ----
target_sources(`${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的 .c 文件
)

target_include_directories(`${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的头文件路径
)

target_compile_definitions(`${CMAKE_PROJECT_NAME} PRIVATE
    # 在此添加新的宏定义
)

target_link_libraries(`${CMAKE_PROJECT_NAME}
    stm32cubemx
    # 在此添加用户自定义静态库
)

# 编译后生成 HEX / BIN 并输出内存占用（必须在 add_executable 同一目录）
add_custom_command(TARGET `${CMAKE_PROJECT_NAME} POST_BUILD
    COMMAND `${CMAKE_OBJCOPY} -O ihex
            `$<TARGET_FILE:`${CMAKE_PROJECT_NAME}>
            `${PROJECT_BINARY_DIR}/`${CMAKE_PROJECT_NAME}.hex
    COMMAND `${CMAKE_OBJCOPY} -O binary
            `$<TARGET_FILE:`${CMAKE_PROJECT_NAME}>
            `${PROJECT_BINARY_DIR}/`${CMAKE_PROJECT_NAME}.bin
    COMMAND `${CMAKE_SIZE} `$<TARGET_FILE:`${CMAKE_PROJECT_NAME}>
    COMMENT "Generating `${CMAKE_PROJECT_NAME}.hex / .bin"
)
"@
    Write-OK "CMakeLists.txt"
}

function New-StmCubeCMakeLists {
    param(
        [string]$root, [string]$projName,
        [string[]]$defines, [string[]]$inclRelPaths,
        [string[]]$cRelPaths, [string]$startupRel,
        [string]$libType, [string]$deviceMacro
    )

    # 宏定义块（含 DEBUG 生成器表达式）
    $defsLines = ""
    if ($libType -eq 'HAL') {
        $defsLines += "    USE_HAL_DRIVER`n"
        if ($deviceMacro) { $defsLines += "    $deviceMacro`n" }
    } else {
        $defsLines += "    USE_STDPERIPH_DRIVER`n"
        if ($deviceMacro) { $defsLines += "    $deviceMacro`n" }
    }
    foreach ($d in $defines) {
        $d = $d.Trim()
        if (-not $d) { continue }
        if ($d -eq 'USE_HAL_DRIVER' -or $d -eq 'USE_STDPERIPH_DRIVER') { continue }
        if ($d -match '^STM32F10X_' -or $d -match '^STM32[A-Z]\d{3}x') { continue }
        if ($deviceMacro -and $d -eq $deviceMacro) { continue }
        $defsLines += "    $d`n"
    }
    $defsLines += '    $<$<CONFIG:Debug>:DEBUG>'

    # Include 路径块（相对于 project root，用 CMAKE_SOURCE_DIR 引用）
    $inclLines = ($inclRelPaths | Where-Object { $_ } |
        ForEach-Object { "    `${CMAKE_SOURCE_DIR}/$(To-Slash $_)" }) -join "`n"

    # 源文件块
    $srcLines = ($cRelPaths | Where-Object { $_ } |
        ForEach-Object { "    `${CMAKE_SOURCE_DIR}/$(To-Slash ($_ -replace '\.C$','.c'))" }) -join "`n"

    $startupLine = if ($startupRel) {
        "    `${CMAKE_SOURCE_DIR}/$(To-Slash $startupRel)"
    } else {
        "    # [未找到 GCC 启动文件，请手动填写]"
    }

    Write-Utf8 "$root\cmake\stm32cubemx\CMakeLists.txt" @"
cmake_minimum_required(VERSION 3.22)
enable_language(C ASM)

# ===========================================================================
# 由 keil2cubevscode.ps1 自动生成
# 包含从 Keil 工程迁移的源文件、include 路径、宏定义
# ===========================================================================

set(MX_Defines_Syms
$defsLines
)

set(MX_Include_Dirs
$inclLines
    `${CMAKE_SOURCE_DIR}
)

set(MX_Sources
$srcLines
)

set(MX_Startup
$startupLine
)

# ---------------------------------------------------------------------------
# stm32cubemx INTERFACE 库：向上传递 include 路径、宏定义、编译选项
# 根 CMakeLists.txt 通过 target_link_libraries(... stm32cubemx) 自动继承
# ---------------------------------------------------------------------------
add_library(stm32cubemx INTERFACE)
target_include_directories(stm32cubemx INTERFACE `${MX_Include_Dirs})
target_compile_definitions(stm32cubemx INTERFACE `${MX_Defines_Syms})
target_compile_options(stm32cubemx INTERFACE
    -ffunction-sections
    -fdata-sections
    -fno-common
    `$<`$<COMPILE_LANGUAGE:C>:-include `${CMAKE_SOURCE_DIR}/keil_compat.h>
    `$<`$<COMPILE_LANGUAGE:CXX>:-include `${CMAKE_SOURCE_DIR}/keil_compat.h>
)

# ---------------------------------------------------------------------------
# 将迁移的源文件添加到可执行目标
# ---------------------------------------------------------------------------
target_sources(`${CMAKE_PROJECT_NAME} PRIVATE `${MX_Sources} `${MX_Startup})

if((CMAKE_C_STANDARD EQUAL 90) OR (CMAKE_C_STANDARD EQUAL 99))
    message(ERROR "keil2cubevscode: C11 or higher required")
endif()
"@
    Write-OK "cmake/stm32cubemx/CMakeLists.txt"
}

function New-CubeSettings {
    param([string]$root)
    $path = "$root\.vscode\settings.json"
    if (Test-Path $path) { Write-Warn "已存在，跳过：.vscode/settings.json"; return }
    Write-Utf8 $path @'
{
    "cortex-debug.JLinkGDBServerPath": "JLinkGDBServerCL",
    "cmake.cmakePath": "cube-cmake",
    "cmake.configureArgs": ["-DCMAKE_COMMAND=cube-cmake"],
    "cmake.preferredGenerators": ["Ninja"]
}
'@
    Write-OK ".vscode/settings.json"
}

function New-CubeTasksJson {
    param([string]$root)
    $path = "$root\.vscode\tasks.json"
    if (Test-Path $path) { Write-Warn "已存在，跳过：.vscode/tasks.json"; return }
    Write-Utf8 $path @'
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Cube: Build Debug",
            "type": "shell",
            "command": "cube-cmake",
            "args": ["--build", "--preset", "Debug", "--parallel"],
            "group": { "kind": "build", "isDefault": true },
            "presentation": { "reveal": "always", "panel": "shared" },
            "problemMatcher": {
                "owner": "gcc",
                "fileLocation": ["relative", "${workspaceFolder}"],
                "pattern": {
                    "regexp": "^(.*):(\\d+):(\\d+):\\s+(warning|error):\\s+(.*)$",
                    "file": 1, "line": 2, "column": 3, "severity": 4, "message": 5
                }
            }
        },
        {
            "label": "Cube: Build Release",
            "type": "shell",
            "command": "cube-cmake",
            "args": ["--build", "--preset", "Release", "--parallel"],
            "group": "build",
            "presentation": { "reveal": "always", "panel": "shared" },
            "problemMatcher": ["$gcc"]
        },
        {
            "label": "Cube: Clean",
            "type": "shell",
            "command": "cube-cmake",
            "args": ["--build", "--preset", "Debug", "--target", "clean"],
            "group": "build",
            "presentation": { "reveal": "always", "panel": "shared" },
            "problemMatcher": []
        },
        {
            "label": "Flash: J-Link",
            "type": "shell",
            "command": "JLink",
            "args": ["-CommandFile", "flash.jlink"],
            "options": { "cwd": "${workspaceFolder}" },
            "group": "build",
            "presentation": { "reveal": "always", "panel": "shared" },
            "problemMatcher": []
        },
        {
            "label": "Flash: J-Link (全擦)",
            "type": "shell",
            "command": "JLink",
            "args": ["-CommandFile", "flash_full.jlink"],
            "options": { "cwd": "${workspaceFolder}" },
            "group": "build",
            "presentation": { "reveal": "always", "panel": "shared" },
            "problemMatcher": []
        }
    ]
}
'@
    Write-OK ".vscode/tasks.json"
}

function New-CubeLaunchJson {
    param([string]$root, [string]$projName, [string]$chip)
    $path = "$root\.vscode\launch.json"
    if (Test-Path $path) { Write-Warn "已存在，跳过：.vscode/launch.json"; return }
    Write-Utf8 $path @"
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug (J-Link)",
            "type": "cortex-debug",
            "request": "launch",
            "servertype": "jlink",
            "executable": "`${workspaceFolder}/build/Debug/${projName}.elf",
            "device": "${chip}",
            "interface": "SWD",
            "speed": 4000,
            "runToEntryPoint": "main",
            "serverpath": "JLinkGDBServerCL"
        },
        {
            "name": "STM32Cube: ST-Link Debug",
            "type": "stlinkgdbtarget",
            "request": "launch",
            "cwd": "`${workspaceFolder}",
            "preBuild": "`${command:st-stm32-ide-debug-launch.build}",
            "runEntry": "main",
            "imagesAndSymbols": [
                {
                    "imageFileName": "`${workspaceFolder}/build/Debug/${projName}.elf"
                }
            ]
        }
    ]
}
"@
    Write-OK ".vscode/launch.json"
}

function New-CubeCppProperties {
    param([string]$root)
    $path = "$root\.vscode\c_cpp_properties.json"
    if (Test-Path $path) { Write-Warn "已存在，跳过：.vscode/c_cpp_properties.json"; return }
    Write-Utf8 $path @'
{
    "configurations": [
        {
            "name": "GCC ARM (cube-cmake)",
            "compileCommands": "${workspaceFolder}/build/Debug/compile_commands.json",
            "compilerPath": "arm-none-eabi-gcc",
            "intelliSenseMode": "gcc-arm"
        }
    ],
    "version": 4
}
'@
    Write-OK ".vscode/c_cpp_properties.json"
}

function New-FlashJlink {
    param([string]$root, [string]$projName, [string]$chip)
    $p1 = "$root\flash.jlink"
    if (-not (Test-Path $p1)) {
        Write-Utf8 $p1 @"
si SWD
speed 4000
device ${chip}
r
h
loadfile build/Debug/${projName}.hex
r
g
exit
"@
        Write-OK "flash.jlink"
    } else { Write-Warn "已存在，跳过：flash.jlink" }

    $p2 = "$root\flash_full.jlink"
    if (-not (Test-Path $p2)) {
        Write-Utf8 $p2 @"
si SWD
speed 4000
device ${chip}
r
h
erase
loadfile build/Debug/${projName}.hex
r
g
exit
"@
        Write-OK "flash_full.jlink"
    } else { Write-Warn "已存在，跳过：flash_full.jlink" }
}

function New-KeilCompatH {
    param([string]$root)
    $path = "$root\keil_compat.h"
    if (Test-Path $path) { Write-Warn "已存在，跳过：keil_compat.h"; return }
    Write-Utf8 $path @'
#ifndef KEIL_COMPAT_H
#define KEIL_COMPAT_H

#ifndef __ASSEMBLER__
#ifdef __GNUC__
  /* Keil 编译器内建关键字 -> GCC 等价 */
  #define __packed          __attribute__((packed))
  #define __weak            __attribute__((weak))
  #define __inline          inline
  #define __forceinline     __attribute__((always_inline)) inline
  #define __align(n)        __attribute__((aligned(n)))
  #define __irq
  #define __pure            __attribute__((pure))
  #ifndef __nop
    #define __nop()         __asm volatile("nop")
  #endif

  /* 强制包含常用标准头，避免 GCC 14+ 对隐式声明报错 */
  #include <stdint.h>
  #include <stddef.h>
  #include <stdbool.h>
  /* itoa 冲突保护：用户若自定义了 itoa，stdlib.h 的声明会冲突 */
  #define itoa __newlib_itoa
  #include <stdlib.h>
  #undef itoa
  #include <string.h>
#endif
#endif /* __ASSEMBLER__ */

#endif /* KEIL_COMPAT_H */
'@
    Write-OK "keil_compat.h"
}

function New-GitIgnore {
    param([string]$root)
    $path = "$root\.gitignore"
    if (Test-Path $path) { Write-Warn "已存在，跳过：.gitignore"; return }
    Write-Utf8 $path @'
build/
*.map
*.elf
*.hex
*.bin
.vscode/settings.local.json
'@
    Write-OK ".gitignore"
}

function New-LinkerScript {
    param([string]$root, [string]$chip, [long]$flashOrigin, [long]$flashSize, [long]$ramOrigin, [long]$ramSize)
    $ldName = "${chip}_FLASH.ld"
    $path = "$root\$ldName"
    if (Test-Path $path) {
        $e = Get-Content $path -Raw -ErrorAction SilentlyContinue
        if ($e -notmatch 'LENGTH\s*=\s*0K\b' -and $e -notmatch 'LENGTH\s*=\s*0\b') {
            Write-Warn "已存在，跳过：$ldName"; return $ldName
        }
        Write-Warn "$ldName 内存值异常，重新生成..."
    }
    $foH = "0x{0:X8}" -f $flashOrigin; $fk = [int]($flashSize / 1024)
    $roH = "0x{0:X8}" -f $ramOrigin;   $rk = [int]($ramSize  / 1024)
    Write-Utf8 $path @"
_estack = ORIGIN(RAM) + LENGTH(RAM);

ENTRY(Reset_Handler)

MEMORY
{
    FLASH (rx)  : ORIGIN = ${foH}, LENGTH = ${fk}K
    RAM   (rwx) : ORIGIN = ${roH},  LENGTH = ${rk}K
}

SECTIONS
{
    .isr_vector : { . = ALIGN(4); KEEP(*(.isr_vector)) . = ALIGN(4); } > FLASH
    .text :
    {
        . = ALIGN(4); *(.text) *(.text*) *(.glue_7) *(.glue_7t)
        KEEP(*(.init)) KEEP(*(.fini)) . = ALIGN(4); _etext = .;
    } > FLASH
    .rodata   : { . = ALIGN(4); *(.rodata) *(.rodata*) . = ALIGN(4); } > FLASH
    .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } > FLASH
    .ARM      : { __exidx_start = .; *(.ARM.exidx*) __exidx_end = .; } > FLASH
    .preinit_array : { PROVIDE_HIDDEN(__preinit_array_start = .); KEEP(*(.preinit_array*)) PROVIDE_HIDDEN(__preinit_array_end = .); } > FLASH
    .init_array    : { PROVIDE_HIDDEN(__init_array_start = .); KEEP(*(SORT(.init_array.*))) KEEP(*(.init_array*)) PROVIDE_HIDDEN(__init_array_end = .); } > FLASH
    .fini_array    : { PROVIDE_HIDDEN(__fini_array_start = .); KEEP(*(SORT(.fini_array.*))) KEEP(*(.fini_array*)) PROVIDE_HIDDEN(__fini_array_end = .); } > FLASH
    _sidata = LOADADDR(.data);
    .data : { . = ALIGN(4); _sdata = .; *(.data) *(.data*) . = ALIGN(4); _edata = .; } > RAM AT > FLASH
    .bss  : { . = ALIGN(4); _sbss = .; __bss_start__ = _sbss; *(.bss) *(.bss*) *(COMMON) . = ALIGN(4); _ebss = .; __bss_end__ = _ebss; } > RAM
    ._user_heap_stack : { . = ALIGN(8); PROVIDE(end = .); PROVIDE(_end = .); . = . + 0x400; . = ALIGN(8); } > RAM
    .ARM.attributes 0 : { *(.ARM.attributes) }
}
"@
    Write-OK "$ldName"
    return $ldName
}

# ===========================================================================
# 主程序
# ===========================================================================
Write-Host ""
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host "  STM32 Keil -> VS Code (cube-cmake / ST Extension)" -ForegroundColor Cyan
Write-Host "  keil2cubevscode.ps1" -ForegroundColor Cyan
Write-Host "========================================================" -ForegroundColor Cyan
Write-Host ""

# 检查 cube-cmake 是否可用（非阻断，仅提示）
Write-Step "检查 cube-cmake"
$cubeCmake = Get-Command "cube-cmake" -ErrorAction SilentlyContinue
if ($cubeCmake) {
    Write-OK "cube-cmake：$($cubeCmake.Source)"
} else {
    Write-Warn "cube-cmake 未找到 — 请先安装 STM32CubeIDE for VS Code 扩展并运行 Bundle Manager"
    Write-Host "  扩展 ID：stmicroelectronics.stm32-vscode-extension" -ForegroundColor Yellow
    Write-Host "  安装后扩展会自动注册 cube-cmake 到 PATH" -ForegroundColor Yellow
    Write-Host "  迁移仍可继续，生成的工程在安装扩展后即可使用" -ForegroundColor Yellow
}

# --- Step 1: 查找 .uvprojx ---
Write-Step "查找 Keil 工程文件"
if (-not $UvprojxPath) {
    $found = Get-ChildItem -Path (Get-Location) -Filter "*.uvprojx" -Recurse -ErrorAction SilentlyContinue
    if ($found.Count -eq 1) {
        $UvprojxPath = $found[0].FullName
        Write-Info "自动找到：$UvprojxPath"
    } elseif ($found.Count -gt 1) {
        Write-Info "找到多个 .uvprojx 文件："
        for ($i = 0; $i -lt $found.Count; $i++) { Write-Info "  [$i] $($found[$i].FullName)" }
        $UvprojxPath = $found[[int](Read-Host "请输入编号")].FullName
    } else {
        $UvprojxPath = Read-Host "未找到 .uvprojx，请手动输入完整路径"
    }
}
if (-not (Test-Path $UvprojxPath)) { Write-Err "文件不存在：$UvprojxPath"; exit 1 }
$projFileDir = Split-Path $UvprojxPath -Parent
Write-OK "工程文件：$UvprojxPath"

# --- Step 2: 解析 XML ---
Write-Step "解析 .uvprojx"
[xml]$proj = [System.IO.File]::ReadAllText($UvprojxPath, [System.Text.Encoding]::UTF8)
$targets = @($proj.Project.Targets.Target)
$target = $null
if ($targets.Count -eq 1) {
    $target = $targets[0]
    Write-Info "Target：$($target.TargetName)"
} else {
    Write-Info "检测到多个 Target："
    for ($i = 0; $i -lt $targets.Count; $i++) { Write-Info "  [$i] $($targets[$i].TargetName)" }
    $target = $targets[[int](Read-Host "请选择 Target 编号")]
}

$opt  = $target.TargetOption
$tco  = $opt.TargetCommonOption
$ads  = $opt.TargetArmAds

$projName = $tco.OutputName
$chipName = ($tco.Device -replace '\s.*','').Trim()

# Flash / RAM 读取
$ocm = $ads.ArmAdsMisc.OnChipMemories
$flashOriginStr = $ocm.IROM.StartAddress; $flashSizeStr = $ocm.IROM.Size
$ramOriginStr   = $ocm.IRAM.StartAddress; $ramSizeStr   = $ocm.IRAM.Size

if (-not $flashSizeStr -or (Get-FlashSizeBytes $flashSizeStr) -eq 0) {
    for ($i = 1; $i -le 10; $i++) {
        $e = $ocm."OCR_RVCT$i"
        if ($e -and $e.Type -eq '1' -and $e.Size -and (Get-FlashSizeBytes $e.Size) -gt 0) {
            $flashOriginStr = $e.StartAddress; $flashSizeStr = $e.Size; break
        }
    }
}
if (-not $ramSizeStr -or (Get-FlashSizeBytes $ramSizeStr) -eq 0) {
    for ($i = 1; $i -le 10; $i++) {
        $e = $ocm."OCR_RVCT$i"
        if ($e -and $e.Type -eq '0' -and $e.Size -and (Get-FlashSizeBytes $e.Size) -gt 0) {
            $ramOriginStr = $e.StartAddress; $ramSizeStr = $e.Size; break
        }
    }
}

$flashOrigin = Get-FlashSizeBytes $flashOriginStr
$flashSize   = Get-FlashSizeBytes $flashSizeStr
$ramOrigin   = Get-FlashSizeBytes $ramOriginStr
$ramSize     = Get-FlashSizeBytes $ramSizeStr

if ($flashSize -eq 0 -or $ramSize -eq 0) {
    Write-Warn "无法从 .uvprojx 读取 Flash/RAM 大小，链接脚本将生成占位值，请手动填写"
}

$definesRaw  = $ads.Cads.VariousControls.Define
$includesRaw = $ads.Cads.VariousControls.IncludePath
$scatterFile = $ads.LDads.ScatterFile

$defines  = ($definesRaw  -split '[,;\s]+') | Where-Object { $_ }
$includes = ($includesRaw -split ';')       | Where-Object { $_ }

$allFiles = $target.Groups.Group | ForEach-Object { $_.Files.File } | Where-Object { $_ }
$cFiles   = @($allFiles | Where-Object { $_.FileType -eq '1' })
$asmFiles = @($allFiles | Where-Object { $_.FileType -eq '2' })

$cAbsPaths   = @($cFiles   | ForEach-Object { Resolve-ProjPath $projFileDir $_.FilePath } | Where-Object { $_ })
$asmAbsPaths = @($asmFiles | ForEach-Object { Resolve-ProjPath $projFileDir $_.FilePath } | Where-Object { $_ })

Write-OK "项目名：$projName   芯片：$chipName"
Write-OK "Flash：0x$("{0:X}" -f $flashOrigin) / $([int]($flashSize/1024))KB   RAM：0x$("{0:X}" -f $ramOrigin) / $([int]($ramSize/1024))KB"
Write-OK "C 源文件：$($cAbsPaths.Count) 个   汇编文件：$($asmAbsPaths.Count) 个"

# --- Step 3: 确定 ROOT ---
Write-Step "确定项目根目录 [ROOT]"
$allAbsPaths = $cAbsPaths + $asmAbsPaths
if ($allAbsPaths.Count -gt 0) {
    $root = Get-CommonAncestor $allAbsPaths
    if ($root.Length -le 3) { $root = Split-Path $projFileDir -Parent }
} else {
    $root = Split-Path $projFileDir -Parent
    if (-not (Test-Path $root)) { $root = $projFileDir }
}
Write-OK "[ROOT] = $root"

function To-RelPath { param([string]$abs)
    if ($abs.StartsWith($root, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $abs.Substring($root.Length).TrimStart('\','/')
    }
    return $abs
}

$cRelPaths    = $cAbsPaths | ForEach-Object { To-RelPath $_ }
$inclRelPaths = $includes  | ForEach-Object {
    $abs = Resolve-ProjPath $projFileDir $_
    if ($abs) { To-RelPath $abs } else { $_ }
}

# --- Step 4: 库类型 ---
Write-Step "识别库类型（HAL / StdPeriph）"
$libType = Detect-LibraryType $definesRaw $includesRaw ($cAbsPaths + $asmAbsPaths) $root
if ($libType -eq 'Unknown') {
    Write-Info "  [0] 标准库（StdPeriph）  [1] HAL 库"
    $libType = if ((Read-Host "请选择") -eq '1') { 'HAL' } else { 'StdPeriph' }
}
Write-OK "库类型：$libType"

# --- Step 4b: FreeRTOS RVDS → GCC ---
$freeRtosSrc = $cAbsPaths | Where-Object { $_ -match 'FreeRTOS' } | Select-Object -First 1
if ($freeRtosSrc) {
    Write-Step "FreeRTOS 移植检测"
    $rvdsSrc = $cAbsPaths | Where-Object { $_ -match '[/\\]RVDS[/\\]' }
    if ($rvdsSrc) {
        $gccPortDir  = (Split-Path ($rvdsSrc | Select-Object -First 1) -Parent) -replace '\\RVDS\\','\GCC\' -replace '/RVDS/','/GCC/'
        $gccPortFile = Join-Path $gccPortDir 'port.c'
        if (Test-Path $gccPortFile) {
            $cAbsPaths = $cAbsPaths | ForEach-Object { $_ -replace '\\RVDS\\','\GCC\' -replace '/RVDS/','/GCC/' }
            $includes  = $includes  | ForEach-Object { $_ -replace '[/\\]RVDS[/\\]','/GCC/' }
            $cRelPaths    = $cAbsPaths | ForEach-Object { To-RelPath $_ }
            $inclRelPaths = $includes  | ForEach-Object { $abs = Resolve-ProjPath $projFileDir $_; if ($abs) { To-RelPath $abs } else { $_ } }
            Write-OK "FreeRTOS RVDS → GCC 替换完成"
        } else {
            Write-Warn "FreeRTOS RVDS port 存在但 GCC port 不存在：$gccPortDir"
            Write-Host "  请从 FreeRTOS-Kernel/portable/GCC/ 下载对应型号的 port.c + portmacro.h" -ForegroundColor Yellow
        }
    } else {
        Write-OK "FreeRTOS：无需替换"
    }
}

# --- Step 5: 编译参数 ---
Write-Step "推导编译参数"
$mcuFlags    = Get-MCUFlags $chipName
$deviceMacro = $null
if ($libType -eq 'HAL') {
    $deviceMacro = Get-HalDeviceMacro $chipName
    if ($deviceMacro) { Write-OK "HAL 设备宏：$deviceMacro" }
    else              { Write-Warn "无法自动推导 HAL 设备宏，请在 cmake/stm32cubemx/CMakeLists.txt 中手动填写" }
} else {
    $deviceMacro = Get-StdPeriphDensityMacro $flashSize $chipName
    Write-OK "StdPeriph 密度宏：$deviceMacro"
}
Write-OK "MCU Flags：$mcuFlags"

# --- Step 6: GCC 启动文件 ---
Write-Step "查找 GCC 语法启动文件"
$startupAbs = Find-GccStartupFile $root $asmAbsPaths $chipName
if ($startupAbs) {
    $startupRel = To-RelPath $startupAbs
    Write-OK "启动文件：$startupRel"
} else {
    Write-Warn "未找到 GCC 语法启动文件，请手动填写 cmake/stm32cubemx/CMakeLists.txt 中的 MX_Startup"
    $startupRel = $null
}

# --- Step 7: core_cm3.c 兼容性修复 ---
Write-Step "检查 GCC 兼容性"
Fix-CoreCm3 $cAbsPaths

# --- Step 8: 链接脚本 ---
Write-Step "处理链接脚本"
$ldFile = $null
if ($scatterFile -and $scatterFile -match '\.ld$') {
    $ldAbs = Resolve-ProjPath $projFileDir $scatterFile
    if ($ldAbs -and (Test-Path $ldAbs)) {
        $ldContent = Get-Content $ldAbs -Raw -ErrorAction SilentlyContinue
        if ($ldContent -notmatch '_estack') {
            $ldContent = "_estack = ORIGIN(RAM) + LENGTH(RAM);`n`n" + $ldContent
            [System.IO.File]::WriteAllText($ldAbs, $ldContent, $Utf8NoBom)
            Write-OK "已在链接脚本头部插入 _estack 定义"
        }
        $ldFile = To-RelPath $ldAbs
        Write-OK "使用现有链接脚本：$ldFile"
    }
}
if (-not $ldFile) {
    Write-Info "未找到 .ld 文件，生成标准链接脚本..."
    $ldFile = New-LinkerScript $root $chipName $flashOrigin $flashSize $ramOrigin $ramSize
}

# --- Step 9: 生成所有配置文件 ---
Write-Step "生成配置文件"
New-GccArmCmake       $root $mcuFlags $ldFile
New-CubePresetsJson   $root
New-RootCMakeLists    $root $projName
New-StmCubeCMakeLists $root $projName $defines $inclRelPaths $cRelPaths $startupRel $libType $deviceMacro
New-KeilCompatH       $root
New-CubeSettings      $root
New-CubeTasksJson     $root
New-CubeCppProperties $root
New-CubeLaunchJson    $root $projName $chipName
New-FlashJlink        $root $projName $chipName
New-GitIgnore         $root

# --- 完成摘要 ---
Write-Host ""
Write-Host "========================================================" -ForegroundColor Green
Write-Host "  迁移完成！" -ForegroundColor Green
Write-Host "========================================================" -ForegroundColor Green
Write-Host ""
Write-Info "项目根目录（用 VS Code 打开此目录）："
Write-Host "  $root" -ForegroundColor White
Write-Host ""
Write-Info "工作流程（STM32CubeIDE for VS Code 扩展）："
Write-Host "  1. VS Code 打开 $root" -ForegroundColor White
Write-Host "  2. 扩展侧边栏 -> Project Setup -> 选择芯片 -> Toolchain: GCC -> Finish" -ForegroundColor White
Write-Host "  3. 扩展底部状态栏点击 Build 按钮（或 Ctrl+Shift+B）" -ForegroundColor White
Write-Host "  4. 按报错逐一修复（参考综合移植手册）" -ForegroundColor White
Write-Host ""
if (-not $startupAbs) {
    Write-Host "  [必须] 手动填写 cmake/stm32cubemx/CMakeLists.txt 中的 MX_Startup 启动文件路径" -ForegroundColor Red
}
Write-Host "  生成文件一览：" -ForegroundColor Cyan
Write-Host "    CMakeLists.txt                    <- 用户可编辑" -ForegroundColor White
Write-Host "    cmake/gcc-arm-none-eabi.cmake     <- 工具链（MCU flags、链接脚本路径）" -ForegroundColor White
Write-Host "    cmake/stm32cubemx/CMakeLists.txt  <- 所有源文件（来自 Keil）" -ForegroundColor White
Write-Host "    CMakePresets.json                 <- cube-cmake 入口" -ForegroundColor White
Write-Host "    keil_compat.h                     <- Keil 关键字兼容层" -ForegroundColor White
Write-Host "    .vscode/settings.json             <- cube-cmake 配置" -ForegroundColor White
Write-Host ""
