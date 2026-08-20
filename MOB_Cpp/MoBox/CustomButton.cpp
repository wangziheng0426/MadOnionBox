#include "CustomButton.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QProcessEnvironment>

CustomButton::CustomButton(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    connect(qbutton, &QPushButton::clicked, this, &CustomButton::startSoftware);
}

CustomButton::CustomButton(const QString &softName, const QString &iconPath, const QString &executePath,
                           QWidget *parent)
    : CustomButton(parent)
{
    setSoftName(softName);
    setIconPath(iconPath);
    setExecutePath(executePath);
}

void CustomButton::setupUi()
{
    // 与历史样式一致：圆角边框 + hover 渐变 + 按下凹陷
    const QString buttonStyle =
        QStringLiteral("QPushButton {text-align: center;padding-top: 0px;padding: 5px;"
                       "border-radius:6px;border:1px groove gray;border-style:outset;}\n"
                       "QPushButton:hover {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
                       "stop: 0 #ffffff, stop: 1 #aaaaaa);}\n"
                       "QPushButton:pressed {border-style: inset;}");

    qbutton = new QPushButton(this);
    qbutton->setIconSize(QSize(44, 44));
    qbutton->setFixedSize(48, 48);
    qbutton->setFlat(true);
    qbutton->setStyleSheet(buttonStyle);
    qbutton->move(1, 0);

    // 按钮投影
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8);
    shadow->setOffset(2);
    shadow->setColor(QColor(0, 0, 0, 160));
    qbutton->setGraphicsEffect(shadow);

    // 下方软件名
    qlabel = new QLabel(this);
    qlabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    //qlabel->setStyleSheet(QStringLiteral("color: black;"));
    qlabel->setFixedSize(56, 20);
    qlabel->move(0, 56);

    // 文字投影
    auto *shadow2 = new QGraphicsDropShadowEffect(this);
    shadow2->setBlurRadius(6);
    shadow2->setOffset(1);
    shadow2->setColor(QColor(0, 0, 0, 120));
    qlabel->setGraphicsEffect(shadow2);

    setFixedSize(66, 76);
}

void CustomButton::applyIcon(const QString &iconPath)
{
    iconPath_ = iconPath;
    if (!qbutton) {
        return;
    }

    QPixmap butPixmap(iconPath);
    if (butPixmap.isNull()) {
        qbutton->setIcon(QIcon());
        return;
    }

    // 圆角裁剪 + 自上而下半透明渐变（DestinationIn 压透明度）
    QPixmap rounded(butPixmap.size());
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    path.addRoundedRect(butPixmap.rect(), 18, 18);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, butPixmap);

    QLinearGradient gradient(0, 0, 0, butPixmap.height());
    gradient.setColorAt(0, QColor(255, 255, 255, 255));
    gradient.setColorAt(1, QColor(255, 255, 255, 180));
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(butPixmap.rect(), gradient);
    painter.end();

    qbutton->setIcon(QIcon(rounded));
}

void CustomButton::setSoftName(const QString &name)
{
    softName_ = name;
    if (qlabel) {
        qlabel->setText(name);
        qlabel->setToolTip(name);
    }
    setToolTip(name);
}

void CustomButton::setVersion(const QString &version)
{
    version_ = version;
    if (qlabel) {
        qlabel->setText(version);
    }
}

void CustomButton::setIconPath(const QString &iconPath)
{
    applyIcon(iconPath);
}

void CustomButton::setExecutePath(const QString &path)
{
    executePath_ = path.trimmed();
}

void CustomButton::setArguments(const QStringList &args)
{
    arguments_ = args;
}

void CustomButton::setEnvironment(const QMap<QString, QString> &env)
{
    environment_ = env;
}

void CustomButton::setPrePythonScript(const QString &scriptPath)
{
    prePythonScript_ = scriptPath.trimmed();
}

void CustomButton::setPrePythonArgs(const QStringList &args)
{
    prePythonArgs_ = args;
}

void CustomButton::setPythonExePath(const QString &pythonExe)
{
    pythonExePath_ = pythonExe.trimmed();
}

QString CustomButton::resolvePythonExe() const
{
    if (!pythonExePath_.isEmpty() && QFileInfo::exists(pythonExePath_)) {
        return pythonExePath_;
    }

    const QDir appDir(QCoreApplication::applicationDirPath());
    const QStringList candidates{
        appDir.filePath(QStringLiteral("python_embed/python-3.11.2/python.exe")),
        appDir.filePath(QStringLiteral("python_embed/python.exe")),
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }
    return {};
}

QProcessEnvironment CustomButton::buildEnvironment(const QMap<QString, QString> &extra)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
        const QString key = it.key();
        const QString value = it.value();
        // 约定：值以 "+;" 开头表示追加到原变量（常用于 PATH）
        if (value.startsWith(QStringLiteral("+;"))) {
            const QString appended = value.mid(2);
            const QString old = env.value(key);
            env.insert(key, old.isEmpty() ? appended : (old + QDir::listSeparator() + appended));
        } else {
            env.insert(key, value);
        }
    }
    return env;
}

bool CustomButton::runPrePythonScript(QString *errorMessage)
{
    if (prePythonScript_.isEmpty()) {
        return true;
    }

    const QFileInfo scriptInfo(prePythonScript_);
    if (!scriptInfo.exists() || !scriptInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = tr("预制脚本不存在：\n%1").arg(prePythonScript_);
        }
        return false;
    }

    const QString pythonExe = resolvePythonExe();
    if (pythonExe.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("找不到 Python 解释器（python_embed）。");
        }
        return false;
    }

    QStringList args{scriptInfo.absoluteFilePath()};
    args.append(prePythonArgs_);

    QProcess proc;
    proc.setProgram(pythonExe);
    proc.setArguments(args);
    proc.setWorkingDirectory(scriptInfo.absolutePath());
    proc.setProcessEnvironment(buildEnvironment(environment_));
    proc.start();
    if (!proc.waitForStarted(10000)) {
        if (errorMessage) {
            *errorMessage = tr("无法启动预制脚本：\n%1").arg(proc.errorString());
        }
        return false;
    }
    if (!proc.waitForFinished(-1)) {
        proc.kill();
        if (errorMessage) {
            *errorMessage = tr("预制脚本执行超时或被中断。");
        }
        return false;
    }
    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        const QString err = QString::fromLocal8Bit(proc.readAllStandardError()).trimmed();
        if (errorMessage) {
            *errorMessage = err.isEmpty()
                                ? tr("预制脚本失败，退出码 %1。").arg(proc.exitCode())
                                : err;
        }
        return false;
    }
    return true;
}

bool CustomButton::launchTarget(QString *errorMessage)
{
    if (executePath_.isEmpty()) {
        if (errorMessage) {
            *errorMessage = tr("未设置可执行文件路径。");
        }
        return false;
    }

    const QFileInfo exeInfo(executePath_);
    if (!exeInfo.exists() || !exeInfo.isFile()) {
        if (errorMessage) {
            *errorMessage = tr("找不到程序：\n%1").arg(executePath_);
        }
        return false;
    }

    // 用 QProcess 设置环境后 startDetached，子进程独立运行、不挂在按钮上
    QProcess proc;
    qDebug() << "arguments_:" << arguments_;
    proc.setProgram(exeInfo.absoluteFilePath());
    proc.setArguments(arguments_);
    proc.setWorkingDirectory(exeInfo.absolutePath());
    proc.setProcessEnvironment(buildEnvironment(environment_));

    qint64 pid = 0;
    if (!proc.startDetached(&pid)) {
        if (errorMessage) {
            *errorMessage = tr("无法启动软件：\n%1").arg(proc.errorString());
        }
        return false;
    }
    return true;
}

bool CustomButton::startSoftware()
{
    QString error;
    if (!runPrePythonScript(&error)) {
        QMessageBox::warning(this, tr("启动失败"), error);
        emit launchFailed(softName_, error);
        return false;
    }
    if (!launchTarget(&error)) {
        QMessageBox::warning(this, tr("启动失败"), error);
        emit launchFailed(softName_, error);
        return false;
    }
    emit launchSucceeded(softName_);
    return true;
}
