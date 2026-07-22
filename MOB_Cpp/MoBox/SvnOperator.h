#ifndef SVNOPERATOR_H
#define SVNOPERATOR_H

#include <QString>
#include <QStringList>

// 封装对本机 svn.exe 的调用：凭据校验、地址探测、checkout / update / export。
// 默认 svn 路径：exe 旁 thirdParty/svn/svn.exe
// 默认工作目录：exe 旁 config/
class SvnOperator
{
public:
    // 一次 svn 命令的执行结果
    struct Result {
        bool success = false; // 进程正常退出且 exitCode == 0
        QString stdOut;       // 标准输出
        QString stdErr;       // 标准错误（失败时优先看这里）
        int exitCode = -1;    // svn 退出码；未执行成功启动时为 -1
    };

    SvnOperator();

    // ---------- 配置 ----------
    void setUserName(const QString &userName);
    void setPassword(const QString &password);
    void setSvnUrl(const QString &svnUrl);           // 仓库 URL（目录或单文件，视命令而定）
    void setSvnExePath(const QString &svnExePath);   // svn.exe 完整路径
    void setWorkCopyPath(const QString &workCopyPath); // checkout/update 目录，或 export 本地目标

    QString userName() const;
    QString password() const;
    QString svnUrl() const;
    QString svnExePath() const;
    QString workCopyPath() const;

    // ---------- 状态查询 ----------
    // svn.exe 是否存在
    bool isSvnAvailable() const;
    // workCopyPath_ 下是否已有 .svn（是否工作副本）
    bool hasWorkingCopy() const;

    // ---------- SVN 操作 ----------
    // svn info + 用户名密码：校验凭据
    Result verifyCredentials() const;
    // svn info：测试 URL 是否可访问（有用户名时带认证）
    Result testUrlAccessible() const;
    // svn checkout：URL 必须是目录，检出到 workCopyPath_
    Result checkout() const;
    // svn update：更新已有工作副本
    Result update() const;
    // svn export：可导出文件或目录，本地不产生 .svn
    // workCopyPath_ 为目录时，自动在其后追加 URL 末尾文件名
    Result exportTo() const;

private:
    // 组装 --username / --password / --non-interactive / --no-auth-cache
    QStringList authArgs() const;
    // 启动 svn.exe 并等待结束，填充 Result
    Result runCommand(const QStringList &arguments) const;

    QString userName_;
    QString password_;
    QString svnUrl_;
    QString svnExePath_;   // svn.exe 路径
    QString workCopyPath_; // 本地工作副本 / 导出目标
};

#endif // SVNOPERATOR_H
