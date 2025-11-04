/**
 * @file launcher.cpp
 * @brief 一个简单的 Windows 启动器程序。
 *
 * 这个程序的功能是启动位于其所在目录下的 `bin` 子目录中的 `MadOnionBox.exe` 可执行文件。
 * 它被设计为一个轻量级的包装器，用于简化主程序的启动过程，
 * 特别是在需要将主程序和其依赖项（如DLL）放在一个单独的子目录中时。
 * 这样做可以保持根目录的整洁。
 *
 * 编译指令（示例）：
 * 1. 编译资源文件（如果 launcher.rc 存在，用于嵌入图标等）：
 *    windres --target=pe-x86-64 launcher.rc -O coff -o launcher.res
 * 2. 编译 C++ 代码并链接资源文件:
 *    g++ launcher.cpp launcher.res -o launcher.exe -mwindows
 *    (-mwindows 标志用于创建一个窗口应用程序，而不是控制台应用程序，这样就不会显示黑色的命令行窗口)
 */

#include <windows.h> // 包含 Windows API 头文件，用于访问 GetModuleFileName, CreateProcess 等函数
#include <string>    // 包含字符串库，用于方便地处理路径字符串
#include <iostream>  // 包含输入输出流库，用于在出错时打印错误信息

// WinMain 是 Windows GUI 应用程序的入口点，使用它而不是 main 可以避免在启动时出现控制台窗口。
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // 创建一个足够大的字符数组来存储当前可执行文件的完整路径
    char exePath[MAX_PATH] = {0};

    // 获取当前可执行文件（即 launcher.exe）的完整路径
    // NULL 作为第一个参数表示获取当前进程的模块句柄
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    // 将 C 风格的路径字符串转换为 std::string 以便操作
    std::string dir = exePath;

    // 从完整路径中提取目录部分
    // find_last_of 会查找最后一个'\'或'/'出现的位置
    // substr 会截取从开头到该位置的子字符串，从而得到文件所在的目录
    dir = dir.substr(0, dir.find_last_of("\\/"));

    // 构建目标可执行文件（MadOnionBox.exe）的完整路径
    // 假设目标文件位于启动器所在目录下的 "bin" 子目录中
    std::string targetExe = dir + "\\bin\\MadOnionBox.exe";

    // 初始化 STARTUPINFOA 结构体，它用于指定新进程的主窗口的属性
    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si)); // 将结构体清零
    si.cb = sizeof(si);          // 设置结构体的大小

    // PROCESS_INFORMATION 结构体，用于接收新创建进程的标识信息
    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi)); // 将结构体清零

    // 调用 CreateProcessA 函数来创建一个新进程
    BOOL ok = CreateProcessA(
        targetExe.c_str(), // 要执行的程序路径
        NULL,              // 命令行参数 (这里没有)
        NULL,              // 进程安全属性 (默认)
        NULL,              // 线程安全属性 (默认)
        FALSE,             // 句柄继承选项 (不继承)
        0,                 // 创建标志 (无特殊标志)
        NULL,              // 环境变量 (使用父进程的环境变量)
        dir.c_str(),       // 新进程的当前工作目录 (设置为启动器所在的目录)
        &si,               // 指向 STARTUPINFOA 结构体的指针
        &pi                // 指向 PROCESS_INFORMATION 结构体的指针
    );

    // 检查 CreateProcessA 是否成功
    if (!ok) {
        // 如果失败，构造错误消息并通过 MessageBox 显示
        std::string errorMessage = "无法启动: " + targetExe;
        MessageBoxA(NULL, errorMessage.c_str(), "启动错误", MB_OK | MB_ICONERROR);
        return 1; // 返回错误码
    }

    // 成功创建进程后，我们不再需要与新进程交互，
    // 因此立即关闭新进程和其主线程的句柄，以释放系统资源。
    // 这不会终止新进程的运行。
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return 0; // 成功退出
}

//windres --target=pe-x86-64 launcher.rc -O coff -o launcher.res
//g++ launcher.cpp launcher.res -o launcher.exe -mwindows -static
//(-mwindows 标志用于创建一个窗口应用程序，而不是控制台应用程序，这样就不会显示黑色的命令行窗口)