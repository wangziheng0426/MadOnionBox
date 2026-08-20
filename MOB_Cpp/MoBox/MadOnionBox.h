#ifndef MADONIONBOX_H
#define MADONIONBOX_H

#include "CustomButton.h"

#include <QMainWindow>
#include <QString>
#include <QPushButton>
#include <QLabel>
#include <QSystemTrayIcon>
#include <QProgressBar>
#include <QPoint>
#include <QMouseEvent>
#include <QCloseEvent>
#include <QEvent>
#include <QMenu>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 主窗口：使用 ui/main.ui 生成的界面
class MadOnionBox : public QMainWindow
{
    Q_OBJECT

public:
    explicit MadOnionBox(const QString &userName, const QString &password, const QString &svnUrl,
                         bool offlineMode = false, QWidget *parent = nullptr);
    ~MadOnionBox() override;
    void initData();
protected:
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void initUI();    
    void createTitleBar();
    void onDownloadClicked();
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void createTrayIcon();
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShow();
    void changeEvent(QEvent *event) override;
    void onTrayQuit();
    bool downLoadConfigFile();
    void onDownloadTaskFinished(bool success, const QString &message);
    void onAllDownloadTasksFinished();
    void setDownloadBusy(bool busy);
    
    bool loadConfigFile();
    // 入队配置中的工具下载；返回 true 表示已异步入队（需等 allFinished 再建按钮）
    bool downLoadToolsInConfig();

    

    //根据配置文件加载dcc按钮
    void createDccButtons();
    void loadSoftwareInfoFromRegistry(QString dccName, QJsonObject dccInfoDic);
    //根据配置文件加载python脚本按钮
    void createPythonScriptButtons();
    // 将按钮列表中的按钮添加到主窗口的控件中，并进行分裂排列
    void arrangeButtons();
    // 切换工具箱
    void switchToolbox();

    Ui::MainWindow *ui = nullptr;
    QSystemTrayIcon *trayIcon = nullptr;
    QPushButton *userSettingButton = nullptr;
    QPushButton *updateButton = nullptr;
    QPushButton *minimizeButton = nullptr;
    QPushButton *closeButton = nullptr;
    QLabel *versionLabel = nullptr;
    QLabel *infoLabel = nullptr;
    QProgressBar *busyBar_ = nullptr;
    bool isDragging = false;
    QPoint startPos;
    QPoint endPos;
    QString userName;
    QString password;
    QString svnUrl;
    QString pythonEmbedPath;
    QJsonObject boxConfigJson;
    bool offlineMode_ = false;
    // dcc按钮列表
    QList<CustomButton*> dccButtons;
    // python脚本按钮列表
    QList<CustomButton*> appButtons;
};

#endif // MADONIONBOX_H
