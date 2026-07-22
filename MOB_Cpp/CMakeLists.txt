cmake_minimum_required(VERSION 3.16)

# launcher（Win32 静态链接）+ MadOnionBox（Qt），可安装到项目根 deploy/
project(MOB_Cpp LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 安装前缀默认：项目根下的 deploy/
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/deploy" CACHE PATH "Install prefix" FORCE)
endif()

# MadOnionBox 依赖 Qt，源码未齐时可先只编 launcher
option(MOB_BUILD_MADONIONBOX "Build MadOnionBox (Qt)" ON)

add_subdirectory(launcher)

if(MOB_BUILD_MADONIONBOX)
    add_subdirectory(MoBox)
endif()
