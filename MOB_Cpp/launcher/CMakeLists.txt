# MadOnionBox 启动器：Win32 + WinHTTP，无 Qt 依赖

add_executable(launcher WIN32
    launcher.cpp
    launcher.h
    launcher.rc
)

target_link_libraries(launcher PRIVATE winhttp sensapi)

# 源码与字符串字面量按 UTF-8 处理，避免中文界面乱码
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(launcher PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    # MinGW 下尽量静态链接运行时，便于单独分发
    target_link_options(launcher PRIVATE -static -static-libgcc -static-libstdc++)
elseif(MSVC)
    target_compile_options(launcher PRIVATE /utf-8)
endif()

# 可选：cmake --install 安装到 CMAKE_INSTALL_PREFIX（默认项目根 deploy/）
# 日常开发由 .vscode/tasks.json 负责复制到 deploy/
install(TARGETS launcher RUNTIME DESTINATION .)
