#include "LoginDialog.h"
#include "Downloader.h"

#include <QCheckBox>
#include <QCoreApplication>
#include <QDir>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>
#include <QApplication>
#include <QDebug>

#include <windows.h>
#include <wincrypt.h>

namespace {
constexpr const char *kSettingsFileName = "usersetup.ini";
constexpr const char *kGroupLogin = "login";
constexpr const char *kKeyUser = "userName";
constexpr const char *kKeyPasswordEnc = "passwordEnc"; // DPAPI 加密后的 Base64
constexpr const char *kKeyPasswordLegacy = "password";  // 旧明文键，保存时删除
constexpr const char *kKeySvn = "svnUrl";
constexpr const char *kKeyRemember = "rememberPassword";

// 使用当前 Windows 用户 DPAPI 保护密码（密文仅本机当前用户可解开）
QString protectPassword(const QString &plain)
{
    if (plain.isEmpty()) {
        return {};
    }

    QByteArray utf8 = plain.toUtf8();
    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(utf8.data());
    input.cbData = static_cast<DWORD>(utf8.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"MadOnionBoxLogin", nullptr, nullptr, nullptr, 0, &output)) {
        return {};
    }

    const QByteArray encrypted(reinterpret_cast<const char *>(output.pbData),
                               static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return QString::fromLatin1(encrypted.toBase64());
}

QString unprotectPassword(const QString &storedBase64)
{
    if (storedBase64.isEmpty()) {
        return {};
    }

    QByteArray encrypted = QByteArray::fromBase64(storedBase64.toLatin1());
    if (encrypted.isEmpty()) {
        return {};
    }

    DATA_BLOB input{};
    input.pbData = reinterpret_cast<BYTE *>(encrypted.data());
    input.cbData = static_cast<DWORD>(encrypted.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, 0, &output)) {
        return {};
    }

    const QString plain = QString::fromUtf8(reinterpret_cast<const char *>(output.pbData),
                                            static_cast<int>(output.cbData));
    LocalFree(output.pbData);
    return plain;
}
} // namespace

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("登录"));
    setModal(true);
    setFixedWidth(360);

    auto *title = new QLabel(tr("请输入登录信息"), this);
    title->setAlignment(Qt::AlignCenter);

    userEdit_ = new QLineEdit(this);
    userEdit_->setPlaceholderText(tr("用户名"));

    passwordEdit_ = new QLineEdit(this);
    passwordEdit_->setPlaceholderText(tr("密码"));
    passwordEdit_->setEchoMode(QLineEdit::Password);

    svnEdit_ = new QLineEdit(this);
    svnEdit_->setPlaceholderText(tr("例如 https://svn.example.com/repo"));

    rememberCheck_ = new QCheckBox(tr("记住密码"), this);

    auto *form = new QFormLayout();
    form->addRow(tr("用户名"), userEdit_);
    form->addRow(tr("密码"), passwordEdit_);
    form->addRow(tr("SVN地址"), svnEdit_);
    form->addRow(QString(), rememberCheck_);

    auto *loginButton = new QPushButton(tr("登录"), this);
    auto *cancelButton = new QPushButton(tr("取消"), this);
    loginButton->setDefault(true);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->addWidget(loginButton);
    buttons->addWidget(cancelButton);

    auto *root = new QVBoxLayout(this);
    root->addWidget(title);
    root->addSpacing(8);
    root->addLayout(form);
    root->addSpacing(12);
    root->addLayout(buttons);

    connect(loginButton, &QPushButton::clicked, this, &LoginDialog::onLoginClicked);
    connect(cancelButton, &QPushButton::clicked, this, &LoginDialog::reject);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);
    connect(svnEdit_, &QLineEdit::returnPressed, this, &LoginDialog::onLoginClicked);

    loadSettings();
    if (userName().isEmpty()) {
        userEdit_->setFocus();
    } else if (password().isEmpty()) {
        passwordEdit_->setFocus();
    } else {
        svnEdit_->setFocus();
    }
}

QString LoginDialog::userName() const
{
    return userEdit_ ? userEdit_->text().trimmed() : QString();
}

QString LoginDialog::password() const
{
    return passwordEdit_ ? passwordEdit_->text() : QString();
}

QString LoginDialog::svnUrl() const
{
    return svnEdit_ ? svnEdit_->text().trimmed() : QString();
}

QString LoginDialog::settingsFilePath() const
{
    // exe 旁 bin/config/usersetup.ini（开发构建则在可执行文件目录下的 config）
    const QString configDir =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));
    QDir().mkpath(configDir);
    return QDir(configDir).filePath(QString::fromUtf8(kSettingsFileName));
}

void LoginDialog::loadSettings()
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(QString::fromUtf8(kGroupLogin));

    const QString user = settings.value(QString::fromUtf8(kKeyUser)).toString();
    const QString svn = settings.value(QString::fromUtf8(kKeySvn)).toString();
    const bool remember = settings.value(QString::fromUtf8(kKeyRemember), false).toBool();
    const QString passwordEnc = settings.value(QString::fromUtf8(kKeyPasswordEnc)).toString();

    settings.endGroup();

    if (userEdit_) {
        userEdit_->setText(user);
    }
    if (svnEdit_) {
        svnEdit_->setText(svn);
    }
    if (rememberCheck_) {
        rememberCheck_->setChecked(remember);
    }
    if (passwordEdit_ && remember && !passwordEnc.isEmpty()) {
        passwordEdit_->setText(unprotectPassword(passwordEnc));
    }
}

void LoginDialog::saveSettings() const
{
    QSettings settings(settingsFilePath(), QSettings::IniFormat);
    settings.beginGroup(QString::fromUtf8(kGroupLogin));

    settings.setValue(QString::fromUtf8(kKeyUser), userName());
    settings.setValue(QString::fromUtf8(kKeySvn), svnUrl());
    settings.remove(QString::fromUtf8(kKeyPasswordLegacy)); // 清除旧明文

    const bool remember = rememberCheck_ && rememberCheck_->isChecked();
    settings.setValue(QString::fromUtf8(kKeyRemember), remember);
    if (remember) {
        const QString enc = protectPassword(password());
        if (!enc.isEmpty()) {
            settings.setValue(QString::fromUtf8(kKeyPasswordEnc), enc);
        } else {
            settings.remove(QString::fromUtf8(kKeyPasswordEnc));
        }
    } else {
        settings.remove(QString::fromUtf8(kKeyPasswordEnc));
    }

    settings.endGroup();
    settings.sync();
}

void LoginDialog::onLoginClicked()
{
    if (userName().isEmpty()) {
        QMessageBox::warning(this, tr("登录"), tr("请输入用户名。"));
        userEdit_->setFocus();
        return;
    }
    if (password().isEmpty()) {
        QMessageBox::warning(this, tr("登录"), tr("请输入密码。"));
        passwordEdit_->setFocus();
        return;
    }
    if (svnUrl().isEmpty()) {
        QMessageBox::warning(this, tr("登录"), tr("请输入 SVN 地址。"));
        svnEdit_->setFocus();
        return;
    }

    offlineMode_ = true; // 默认离网，只有校验通过才改成在线
    if (verifyLogin()) {
        offlineMode_ = false;
    } else {
        const auto choice = QMessageBox::question(
            this,
            tr("登录失败"),
            tr("用户名/密码不正确，或 SVN 地址不可用。\n\n是否留在登录页重新输入？\n选「否」将以离网模式进入主界面。"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);
        if (choice == QMessageBox::Yes) {
            return; // 是：留在登录页
        }
        offlineMode_ = true; // 否：离网开启主窗体
    }

    saveSettings();
    accept();
}

bool LoginDialog::verifyLogin()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    setEnabled(false);
    QApplication::processEvents();

    Downloader downloader(Downloader::Mode::Svn);
    downloader.setUserName(userName());
    downloader.setPassword(password());
    downloader.setSourceUrl(svnUrl());
    const Downloader::Result result = downloader.verify();

    setEnabled(true);
    QApplication::restoreOverrideCursor();

    if (!result.success) {
        qWarning("登录校验失败: %s", qUtf8Printable(result.message));
    }
    return result.success;
}
