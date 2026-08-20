# MadOnionBox（Qt6 Widgets）
# 依赖：Qt6（本机示例路径 C:/QT/6.9.2/mingw_64）
# 配置示例：
#   cmake -B build -DMOB_BUILD_MADONIONBOX=ON -DCMAKE_PREFIX_PATH=C:/QT/6.9.2/mingw_64

cmake_minimum_required(VERSION 3.21)

# ---------- Qt 自动化（moc / uic / rcc）----------
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/ui")

# ---------- 查找 Qt6 ----------
if(NOT CMAKE_PREFIX_PATH AND DEFINED ENV{CMAKE_PREFIX_PATH})
    set(CMAKE_PREFIX_PATH "$ENV{CMAKE_PREFIX_PATH}")
endif()

find_package(Qt6 6.2 COMPONENTS Widgets REQUIRED)

if(NOT Qt6_FOUND)
    message(FATAL_ERROR
        "未找到 Qt6。请配置 -DCMAKE_PREFIX_PATH=<Qt安装>/mingw_64 后重试。\n"
        "例如: -DCMAKE_PREFIX_PATH=C:/QT/6.9.2/mingw_64")
endif()

# ---------- 源文件（自动收集，新增/删除后会触发重新配置）----------
file(GLOB MADONIONBOX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui/*.ui"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.qrc"
)

# ---------- 可执行文件 ----------
# qt_add_executable：正确处理 WIN32、AUTOMOC，并便于后续扩展资源/翻译
qt_add_executable(MadOnionBox WIN32 ${MADONIONBOX_SOURCES})
#qt_add_executable(MadOnionBox  ${MADONIONBOX_SOURCES})
target_include_directories(MadOnionBox PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_BINARY_DIR}"
)

target_link_libraries(MadOnionBox PRIVATE
    Qt6::Widgets
    crypt32
)

# 输出目录：build/MoBox/MadOnionBox.exe（默认），安装到 deploy/bin
set_target_properties(MadOnionBox PROPERTIES
    OUTPUT_NAME "MadOnionBox"
    #WIN32_EXECUTABLE TRUE
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/MoBox"
)

# ---------- 编码（中文源码 / 字符串）----------
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(MadOnionBox PRIVATE
        -finput-charset=UTF-8
        -fexec-charset=UTF-8
    )
elseif(MSVC)
    target_compile_options(MadOnionBox PRIVATE /utf-8)
endif()

# ---------- 安装到 deploy/bin（与 launcher 约定一致）----------
install(TARGETS MadOnionBox
    RUNTIME DESTINATION bin
)

message(STATUS "MadOnionBox: Qt ${Qt6_VERSION}, UI=ui/main.ui")

# 安装 Qt 运行时依赖项
install(FILES
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Core.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Gui.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Widgets.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Network.dll"
    DESTINATION bin
)
install(FILES
    "C:/Qt/6.9.2/mingw_64/plugins/platforms/qwindows.dll"
    DESTINATION bin/platforms
)
# 安装 MinGW 运行时依赖项
install(FILES
    "C:/mingw64/bin/libgcc_s_seh-1.dll"
    "C:/mingw64/bin/libstdc++-6.dll"
    "C:/mingw64/bin/libwinpthread-1.dll"
    DESTINATION bin
)
