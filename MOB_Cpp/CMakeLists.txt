# 最低要求的 CMake 版本
cmake_minimum_required(VERSION 3.16)

# 项目名称和使用的语言
project(MadOnionBox LANGUAGES CXX)

# 如果未指定，默认将构建类型设置为 Debug
# if(NOT CMAKE_BUILD_TYPE)
#     set(CMAKE_BUILD_TYPE Debug)
# endif()

# 指定使用的 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Qt 特定构建步骤的自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
# 自动转换uic文件
set(CMAKE_AUTOUIC ON) 

# 指定 Qt 安装路径
set(CMAKE_PREFIX_PATH "C:/Qt/6.9.2/mingw_64")

# 查找所需的 Qt6 包及其 Widgets 组件
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network)

# 定义可执行目标及其源文件
file(GLOB SOURCES "*.cpp" "*.h" "ui/*.ui" "resources.qrc")



# 关键修复：将 ui 目录添加到包含目录中
#add_executable(MadOnionBox ${SOURCES})
add_executable(MadOnionBox WIN32 ${SOURCES})
# 将 Qt Widgets 库链接到可执行目标
target_link_libraries(MadOnionBox PUBLIC
    Qt::Core
    Qt::Gui
    Qt::Widgets
    Qt::Network
)
# 设置安装路径,设置到build下的my_install_dir,设置为相对路径
set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/install_dir")

#安装文件部分############################################################################
# 安装可执行文件到根目录
install(TARGETS MadOnionBox
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION bin
    ARCHIVE DESTINATION bin)

# 安装 Qt 运行时依赖项
install(FILES
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Core.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Gui.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Widgets.dll"
    "C:/Qt/6.9.2/mingw_64/bin/Qt6Network.dll"
    DESTINATION bin
)
# 安装加载器
install(FILES
    ${PROJECT_SOURCE_DIR}/launcher/launcher.exe
    DESTINATION .
)
#安装图标
# file(GLOB ICON_FILES "${PROJECT_SOURCE_DIR}/launcher/icon/*.ico")
# install(FILES
#     ${ICON_FILES}
#     DESTINATION bin/icon
# )

install(FILES
    "C:/Qt/6.9.2/mingw_64/plugins/platforms/qwindows.dll"
    DESTINATION bin/platforms
)
# 在ui目录下找到所有ui文件,并安装到install_dir/ui目录下
# install(FILES ${UI_FILES} DESTINATION ui)
# 安装其他资源文件（如果有）
install(FILES
    "temp.txt"
    DESTINATION resources
)


# 安装 MinGW 运行时依赖项
install(FILES
    "C:/mingw64/bin/libgcc_s_seh-1.dll"
    "C:/mingw64/bin/libstdc++-6.dll"
    "C:/mingw64/bin/libwinpthread-1.dll"
    DESTINATION bin
)

# cmake -S . -B build -G "MinGW Makefiles"