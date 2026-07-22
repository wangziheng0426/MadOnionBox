#if !defined(LAUNCHER_H)
#define LAUNCHER_H

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <windows.h>

// MadOnionBox 启动器。
// 负责：检查本地程序是否存在、拉取云端更新列表、按版本下载并解压更新包，最后启动 MadOnionBox。
// 本启动器只更新 madbox python 库。 py脚本及其他文件由 MadOnionBox 自行通过 python 更新。
//
// 主要流程：
// 1. 检查本地是否存在 MadOnionBox；不存在则下载并解压到本地
// 2. 从云服务器下载更新列表
// 3. 比对本地与服务器版本；本地落后则下载压缩包
// 4. 解压更新包到本地
// 5. 启动 MadOnionBox

class Launcher
{
public:
    Launcher();
    ~Launcher();

    // 执行完整启动流程：确保目录、安装/更新、再启动程序
    void run();

private:
    // 云端更新列表地址（update.txt，INI 风格：version / packageUrl）
    static constexpr const char *kUpdateListUrl = "http://192.168.212.225/update.txt";
    // 主程序可执行文件名
    static constexpr const char *kMadOnionBoxExeName = "madonionbox.exe";
    // 更新临时目录名（相对安装根目录）
    static constexpr const char *kTempDirName = "temp";
    // 云端更新列表下载后的本地文件名
    static constexpr const char *kUpdateListFileName = "update.txt";
    // 本地版本记录（成功更新后保存的完整 update.txt）
    static constexpr const char *kLocalVersionFileName = "version.txt";

    std::string installDir_;       // 安装根目录（launcher 所在目录）
    std::string binDir_;           // 二进制目录：installDir_/bin
    std::string tempDir_;          // 更新临时目录：installDir_/temp
    std::string madOnionBoxExe_;   // 主程序完整路径
    std::string updateListFile_;   // temp/update.txt
    std::string localVersionFile_; // temp/version.txt

    // 根据当前进程路径解析安装目录
    std::string getInstallDir() const;
    // 判断指定路径是否为已存在的普通文件
    bool fileExists(const std::string &path) const;
    // 递归创建目录（若已存在则视为成功）
    bool ensureDirectory(const std::string &path) const;

    // 从本地 version 文件读取 version=；不存在或解析失败返回 "0.0.0"
    std::string readLocalVersion() const;
    // 更新成功后，将下载的 update.txt 存为本地 version.txt
    bool saveLocalUpdateFile() const;
    // 比较版本号：left < right 返回 -1，相等返回 0，left > right 返回 1
    int compareVersion(const std::string &left, const std::string &right) const;

    // 下载并解析 update.txt，输出远程版本号与全部 packageUrl
    bool downloadUpdateList(std::string &remoteVersion, std::vector<std::string> &packageUrls) const;
    // 将 url 下载到 destPath；网络不通或远程服务器无响应时直接返回 false
    bool downloadFile(const std::string &url, const std::string &destPath) const;
    // 将 zipPath 解压到 destDir（优先 tar，失败则尝试 PowerShell Expand-Archive）
    bool extractZip(const std::string &zipPath, const std::string &destDir) const;

    // 按需安装或升级 MadOnionBox：本地缺失或版本落后时下载并解压
    bool installOrUpdateMadOnionBox();
    // 启动本地 MadOnionBox 进程
    bool launchMadOnionBox() const;
    // 弹出错误提示框
    void showError(const std::string &message) const;

    // 显示/更新下载与更新过程的提示窗口（避免用户以为未启动或卡死）
    void showStatus(const std::string &message) const;
    void closeStatus() const;
    // 处理窗口消息，保持提示窗口可刷新
    void pumpStatusMessages() const;

    mutable HWND statusHwnd_ = nullptr;   // 状态提示窗口
    mutable HWND statusLabel_ = nullptr;  // 状态文字控件
    mutable HFONT statusFont_ = nullptr;  // 状态文字字体
};

#endif // LAUNCHER_H
