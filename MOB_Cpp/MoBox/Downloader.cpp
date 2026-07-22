#include "Downloader.h"

Downloader::Downloader(Mode mode)
    : mode_(mode)
{
}

void Downloader::setMode(Mode mode)
{
    mode_ = mode;
}

Downloader::Mode Downloader::mode() const
{
    return mode_;
}

void Downloader::setUserName(const QString &userName)
{
    svn_.setUserName(userName);
}

void Downloader::setPassword(const QString &password)
{
    svn_.setPassword(password);
}

void Downloader::setSourceUrl(const QString &url)
{
    svn_.setSvnUrl(url);
}

void Downloader::setLocalPath(const QString &localPath)
{
    svn_.setWorkCopyPath(localPath);
}

QString Downloader::userName() const
{
    return svn_.userName();
}

QString Downloader::sourceUrl() const
{
    return svn_.svnUrl();
}

QString Downloader::localPath() const
{
    return svn_.workCopyPath();
}

Downloader::Result Downloader::download()
{
    switch (mode_) {
    case Mode::Svn:
        return downloadViaSvn();
    }
    return {false, QStringLiteral("未知下载模式。")};
}

Downloader::Result Downloader::verify()
{
    switch (mode_) {
    case Mode::Svn:
        return verifyViaSvn();
    }
    return {false, QStringLiteral("未知下载模式。")};
}

Downloader::Result Downloader::downloadViaSvn()
{
    if (!svn_.isSvnAvailable()) {
        return {false, QStringLiteral("工具箱可能不完整，缺少 svn.exe，无法下载。")};
    }
    if (svn_.svnUrl().isEmpty()) {
        return {false, QStringLiteral("下载地址为空。")};
    }

    const bool updating = svn_.hasWorkingCopy();
    const SvnOperator::Result result = updating ? svn_.update() : svn_.checkout();
    if (!result.success) {
        const QString detail = result.stdErr.trimmed().isEmpty()
                                   ? QStringLiteral("svn 执行失败。")
                                   : result.stdErr.trimmed();
        return {false, detail};
    }
    return {true, updating ? QStringLiteral("更新完成。") : QStringLiteral("下载完成。")};
}

Downloader::Result Downloader::verifyViaSvn()
{
    if (!svn_.isSvnAvailable()) {
        return {false, QStringLiteral("缺少 svn.exe。")};
    }
    const SvnOperator::Result result = svn_.verifyCredentials();
    if (!result.success) {
        const QString detail = result.stdErr.trimmed().isEmpty()
                                   ? QStringLiteral("校验失败。")
                                   : result.stdErr.trimmed();
        return {false, detail};
    }
    return {true, QStringLiteral("校验通过。")};
}
