#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>

class QCheckBox;
class QLineEdit;

// 启动时登录对话框：用户名、密码、SVN 地址；记住密码时用 DPAPI 加密写入 config/usersetup.ini
// 登录时通过 Downloader::verify 校验凭据/地址；失败可选择离网模式继续
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);

    QString userName() const;
    QString password() const;
    QString svnUrl() const;
    // true=离网；仅校验通过后为 false（在线）
    bool offlineMode() const { return offlineMode_; }

private slots:
    void onLoginClicked();

private:
    QString settingsFilePath() const;
    void loadSettings();
    void saveSettings() const;
    // 调用 Downloader::verify；成功返回 true
    bool verifyLogin();

    QLineEdit *userEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QLineEdit *svnEdit_ = nullptr;
    QCheckBox *rememberCheck_ = nullptr;
    bool offlineMode_ = true; // 默认离网
};

#endif // LOGINDIALOG_H
