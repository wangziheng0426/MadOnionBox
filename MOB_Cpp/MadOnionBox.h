#ifndef MADONIONBOX_H
#define MADONIONBOX_H
#include <string.h>
#include <QRegularExpression>

#include <QMainWindow> // 继承自你在 Designer 中选择的基类
#include <QMouseEvent>

#include <QSystemTrayIcon>
#include <QMenu>
#include <QCloseEvent>
#include <QResource>
#include <QtWidgets>
#include <QDialog>
#include <QSettings>


#include <QtConcurrent/QtConcurrent>
#include <QDebug>

#include <QObject>
#include <QString>
#include <QQueue>
#include <QProcess>
#include <QThread>
//网络
#include <QTcpSocket>
// json
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>

// 向前声明 uic 生成的 UI 定义类，这是最佳实践
namespace Ui {
class MainWindow; // 这个名字必须与你在 Designer 中设置的 objectName 一致
}
// 多线程
class CommandWorker : public QObject {
    Q_OBJECT
public:
    explicit CommandWorker(QObject *parent = nullptr);

    // 添加任务
    void addTask(const QString &cmd);

signals:
    void taskFinished(const QString &output);
    void allTasksFinished();
    void showProgress();

public slots:
    void start(); // 启动任务队列
    void processNext(); // 处理下一个任务

private:
    QQueue<QString> taskQueue;
    QProcess *proc;
    bool isRunning = false;
};
//自定义控件类
class CustomButton : public QWidget {
    Q_OBJECT    
public:
    explicit CustomButton(const QString &text, const QString &iconPath,QString executePath, QWidget *parent = nullptr);
    bool startSoftware(); // 启动软件
    QString softName;
    int dccIndex;
    int dccId;
    QJsonObject dccInfoDic;
    QString executePath; // 可执行文件路径
    QStringList dccArgs; // 启动参数
    QProcess *process = new QProcess(this);

protected:
    // void paintEvent(QPaintEvent *event) override;
    // void mousePressEvent(QMouseEvent *event) override;
signals:
    void clicked(); // 点击信号

private:    
    QPushButton *qbutton;
    QLabel *qlabel;

};
// 设置窗口类
class  SettingWindow : public QDialog {
    Q_OBJECT 
public:
    explicit SettingWindow(QWidget *parent = nullptr);
    void saveSettings(); // 保存设置

    QString svnUserName() const; // SVN用户名
    QString svnPassword() const; // SVN密码
private:
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
};

// 主窗口类
class MadOnionBox : public QMainWindow
{
    Q_OBJECT // 如果要使用信号和槽，必须包含此宏

public:    
    QProcess *process=new QProcess(this);
    explicit MadOnionBox(QWidget *parent = nullptr);
    ~MadOnionBox(); // 析构函数是必要的，用来释放 UI 资源
    
    void init();
    // 声明下载配置文件的函数
    void downloadConfig();
    // 下载工具箱
    void downloadTools();
    // svn操作
    bool svnUpdate(const QString &svnPath, const QString &svnUrl);
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void createTitleBar(); // 创建更新,最小化,关闭按钮
    void closeEvent(QCloseEvent *event) override; // 托盘相关，重载关闭事件
    void createTrayIcon(); // 创建托盘图标和菜单
    // 执行命令行(阻塞)     
    bool runCommand(const QString &command, QStringList &output);
    // 检查网络端口是否能够访问
    bool checkPortAccessible(const QString &host, quint16 port, int timeoutMs = 3000);
    
    // 窗口缩放
    void resizeEvent(QResizeEvent *event) override;
    // 根据配置文件加载dcc
    void loadDccInfo();
    // 创建自动布局
    void createGroupBoxLayout(QWidget* groupBox);
    // 排布dcc按钮
    void layoutButtons(QWidget* groupBox,QList<CustomButton*> buttonList);    
    // 根据json数据,从注册表获取软件信息,并创建按钮
    void loadSoftwareInfoFromRegistry(QString dccName, QJsonObject dccObj,int dccIndex);
    // 创建工具按钮
    void createAppButtons();
    // 创建按钮
    void createButton(const QString &dccName, const QString &version, const QString &installPath,QJsonObject &dccInfoDic);
    // 切换工具箱,要修改按钮的启动信息
    void switchToolbox();
    // 修改设置
    void modifySettings();
    // 进度条开关
    //void showProgressBar();
    //void hideProgressBar();
private:
    // 一个指向 uic 生成的 UI 类的指针成员
    Ui::MainWindow *ui;
    QString svnUserName; // SVN用户名
    QString svnPassword; // SVN密码
    QString svnIp;  // SVN仓库URL
    QString svnPath; // SVN仓库路径
    QString userSettingsPath; // 用户配置文件路径
    QString pyEvnPath; // python脚本路径
    QSettings *userSettings = nullptr; // 用于保存用户配置
    QJsonObject boxConfigJson; // 存储下载的配置文件
    QPoint startPos; // 用于窗口拖动的起始位置
    QPoint endPos;   // 用于窗口拖动的结束位置
    bool isDragging = false; // 标记是否正在拖动窗口
    QProgressBar *progressBar = nullptr; // 进度条
    // 托盘相关成员
    QSystemTrayIcon *trayIcon = nullptr;
    QMenu *trayMenu = nullptr;
    // 窗体按钮
    QPushButton *userSettingButton = nullptr;
    QPushButton *updateButton = nullptr;
    QPushButton *minimizeButton = nullptr;
    QPushButton *closeButton = nullptr;
    // dcc按钮列表
    QList<CustomButton*> dccButtons;
    // 工具按钮列表
    QList<CustomButton*> appButtons;
    // commandworker
    CommandWorker *worker = nullptr;
    QThread *workerThread = nullptr;
public slots:
    void showProgressBar();
    void hideProgressBar();
private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onTrayShow();
    void onTrayQuit();
};


#endif // MADONIONBOX_H