#include "SvnOperator.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>

namespace {
QString defaultSvnExePath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("thirdParty/svn/svn.exe"));
}

QString defaultWorkCopyPath()
{
    const QString dir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));
    QDir().mkpath(dir);
    return dir;
}
} // namespace

SvnOperator::SvnOperator()
    : svnExePath_(defaultSvnExePath())
    , workCopyPath_(defaultWorkCopyPath())
{
}

void SvnOperator::setUserName(const QString &userName)
{
    userName_ = userName.trimmed();
}

void SvnOperator::setPassword(const QString &password)
{
    password_ = password;
}

void SvnOperator::setSvnUrl(const QString &svnUrl)
{
    svnUrl_ = svnUrl.trimmed();
}

void SvnOperator::setSvnExePath(const QString &svnExePath)
{
    svnExePath_ = svnExePath.trimmed();
}

void SvnOperator::setWorkCopyPath(const QString &workCopyPath)
{
    workCopyPath_ = workCopyPath.trimmed();
}

QString SvnOperator::userName() const
{
    return userName_;
}

QString SvnOperator::password() const
{
    return password_;
}

QString SvnOperator::svnUrl() const
{
    return svnUrl_;
}

QString SvnOperator::svnExePath() const
{
    return svnExePath_;
}

QString SvnOperator::workCopyPath() const
{
    return workCopyPath_;
}

bool SvnOperator::isSvnAvailable() const
{
    return QFile::exists(svnExePath_);
}

bool SvnOperator::hasWorkingCopy() const
{
    if (workCopyPath_.isEmpty()) {
        return false;
    }
    return QFileInfo(QDir(workCopyPath_).filePath(QStringLiteral(".svn"))).isDir();
}

QStringList SvnOperator::authArgs() const
{
    QStringList args;
    if (!userName_.isEmpty()) {
        args << QStringLiteral("--username") << userName_;
    }
    if (!password_.isEmpty()) {
        args << QStringLiteral("--password") << password_;
    }
    args << QStringLiteral("--non-interactive") << QStringLiteral("--no-auth-cache");
    return args;
}

SvnOperator::Result SvnOperator::runCommand(const QStringList &arguments) const
{
    Result result;
    if (!isSvnAvailable()) {
        result.stdErr = QStringLiteral("找不到 svn.exe：%1").arg(svnExePath_);
        return result;
    }

    QProcess process;
    qDebug() << "运行命令: " << svnExePath_ << arguments;
    process.setProgram(svnExePath_);
    process.setArguments(arguments);
    process.setProcessChannelMode(QProcess::SeparateChannels);
    process.start();
    if (!process.waitForStarted(15000)) {
        result.stdErr = QStringLiteral("无法启动 svn：%1").arg(process.errorString());
        return result;
    }
    if (!process.waitForFinished(-1)) {
        process.kill();
        process.waitForFinished(3000);
        result.stdErr = QStringLiteral("svn 执行超时或被中断。");
        return result;
    }

    result.stdOut = QString::fromLocal8Bit(process.readAllStandardOutput());
    result.stdErr = QString::fromLocal8Bit(process.readAllStandardError());
    result.exitCode = process.exitCode();
    result.success =
        process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
    return result;
}

SvnOperator::Result SvnOperator::verifyCredentials() const
{
    Result result;
    if (svnUrl_.isEmpty()) {
        result.stdErr = QStringLiteral("SVN 地址为空。");
        return result;
    }
    if (userName_.isEmpty() || password_.isEmpty()) {
        result.stdErr = QStringLiteral("用户名或密码为空。");
        return result;
    }

    QStringList args{QStringLiteral("info"), svnUrl_};
    args.append(authArgs());
    return runCommand(args);
}

SvnOperator::Result SvnOperator::testUrlAccessible() const
{
    Result result;
    if (svnUrl_.isEmpty()) {
        result.stdErr = QStringLiteral("SVN 地址为空。");
        return result;
    }

    QStringList args{QStringLiteral("info"), svnUrl_};
    if (!userName_.isEmpty()) {
        args.append(authArgs());
    } else {
        args << QStringLiteral("--non-interactive");
    }
    return runCommand(args);
}

SvnOperator::Result SvnOperator::checkout() const
{
    Result result;
    if (svnUrl_.isEmpty()) {
        result.stdErr = QStringLiteral("SVN 地址为空。");
        return result;
    }
    if (workCopyPath_.isEmpty()) {
        result.stdErr = QStringLiteral("工作副本目录为空。");
        return result;
    }

    // 只创建父目录，让 svn 创建最后一层（与命令行 checkout 出 python_embed 等行为一致）
    const QFileInfo destInfo(workCopyPath_);
    QDir().mkpath(destInfo.absolutePath());
    QStringList args{QStringLiteral("checkout"), svnUrl_, workCopyPath_};
    args.append(authArgs());
    return runCommand(args);
}

SvnOperator::Result SvnOperator::update() const
{
    Result result;
    if (workCopyPath_.isEmpty()) {
        result.stdErr = QStringLiteral("工作副本目录为空。");
        return result;
    }
    if (!hasWorkingCopy()) {
        result.stdErr = QStringLiteral("目录不是 SVN 工作副本：%1").arg(workCopyPath_);
        return result;
    }

    QStringList args{QStringLiteral("update"), workCopyPath_};
    args.append(authArgs());
    return runCommand(args);
}

SvnOperator::Result SvnOperator::exportTo() const
{
    Result result;
    if (svnUrl_.isEmpty()) {
        result.stdErr = QStringLiteral("SVN 地址为空。");
        return result;
    }
    if (workCopyPath_.isEmpty()) {
        result.stdErr = QStringLiteral("本地导出路径为空。");
        return result;
    }

    QString dest = workCopyPath_;
    const QFileInfo destInfo(dest);
    // 本地路径是目录时，把 URL 最后一段当作文件名拼上去
    if (dest.endsWith(QLatin1Char('/')) || dest.endsWith(QLatin1Char('\\')) ||
        (destInfo.exists() && destInfo.isDir()) ||
        (!destInfo.exists() && destInfo.suffix().isEmpty())) {
        QString remoteName = svnUrl_;
        while (remoteName.endsWith(QLatin1Char('/'))) {
            remoteName.chop(1);
        }
        remoteName = remoteName.section(QLatin1Char('/'), -1);
        if (remoteName.isEmpty()) {
            result.stdErr = QStringLiteral("无法从 URL 解析文件名。");
            return result;
        }
        dest = QDir(workCopyPath_).filePath(remoteName);
    }

    QDir().mkpath(QFileInfo(dest).absolutePath());
    QStringList args{QStringLiteral("export"), svnUrl_, dest, QStringLiteral("--force")};
    args.append(authArgs());
    return runCommand(args);
}
