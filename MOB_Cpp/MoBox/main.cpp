#include "LoginDialog.h"
#include "MadOnionBox.h"

#include <QApplication>
#include <QSharedMemory>

#include <windows.h>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QSharedMemory shared("MadOnionBox_SingleInstance");
    if (!shared.create(1)) {
        // 通知已有实例：自己 show()（外部 ShowWindow 解不了 Qt 的 hide）
        if (HWND hwnd = FindWindowW(nullptr, L"MadOnionBox")) {
            PostMessageW(hwnd, RegisterWindowMessageW(L"MadOnionBox_Raise"), 0, 0);
        }
        return 0;
    }

    QString userName="";
    QString password="";
    QString svnUrl="";
    bool offlineMode = true;
    {
        LoginDialog login;
        if (login.exec() == QDialog::Accepted) {          
            userName = login.userName();
            password = login.password();
            svnUrl = login.svnUrl();
            offlineMode = login.offlineMode();
        }
    }

    MadOnionBox window(userName, password, svnUrl, offlineMode);
    window.show();
    window.initData();
    return app.exec();
}
