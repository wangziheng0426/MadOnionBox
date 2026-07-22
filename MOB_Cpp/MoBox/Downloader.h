#ifndef DOWNLOADER_H
#define DOWNLOADER_H

#include "SvnOperator.h"

#include <QString>

// 文件下载/同步入口。当前实现走 SVN，以后可换成 HTTP 等而不改主窗口。
class Downloader
{
public:
    enum class Mode {
        Svn // 后续可扩展 Http 等
    };

    struct Result {
        bool success = false;
        QString message;
    };

    explicit Downloader(Mode mode = Mode::Svn);

    void setMode(Mode mode);
    Mode mode() const;

    // 同步所需凭据与目标（SVN 模式下传给 SvnOperator）
    void setUserName(const QString &userName);
    void setPassword(const QString &password);
    void setSourceUrl(const QString &url);
    void setLocalPath(const QString &localPath);

    QString userName() const;
    QString sourceUrl() const;
    QString localPath() const;

    // 执行下载/同步：已有本地副本则更新，否则检出
    Result download();

    // 仅校验凭据与地址是否可用（不拉文件）
    Result verify();

private:
    Result downloadViaSvn();
    Result verifyViaSvn();

    Mode mode_ = Mode::Svn;
    SvnOperator svn_;
};

#endif // DOWNLOADER_H
