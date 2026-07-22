#include "MadOnionBox.h"
#include "ui_main.h"
#include "Downloader.h"
#include "TaskExecutor.h"
#include <QRegularExpression>

#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QColor>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QTextStream>
#include <QMessageBox>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QProcess>
#include <QAction>
#include <QProgressBar>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QResource>
#include <QScrollArea>
#include <QGridLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QFrame>
#include <windows.h>

namespace {

void clearGridLayout(QGridLayout *grid)
{
    if (!grid) {
        return;
    }
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (QWidget *w = item->widget()) {
            grid->removeWidget(w);
            w->setParent(nullptr);
        }
        delete item;
    }
}

// 确保 GroupBox → VBox → ScrollArea → content → GridLayout 结构存在
QGridLayout *ensureScrollGrid(QGroupBox *groupBox)
{
    if (!groupBox) {
        return nullptr;
    }

    auto *boxLayout = qobject_cast<QVBoxLayout *>(groupBox->layout());
    if (!boxLayout) {
        delete groupBox->layout();
        boxLayout = new QVBoxLayout(groupBox);
        boxLayout->setContentsMargins(4, 8, 4, 4);
    }

    QScrollArea *scrollArea = nullptr;
    if (boxLayout->count() > 0) {
        scrollArea = qobject_cast<QScrollArea *>(boxLayout->itemAt(0)->widget());
    }
    if (!scrollArea) {
        while (QLayoutItem *item = boxLayout->takeAt(0)) {
            if (QWidget *w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
        scrollArea = new QScrollArea(groupBox);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        boxLayout->addWidget(scrollArea);
    }

    QWidget *content = scrollArea->widget();
    if (!content) {
        content = new QWidget;
        scrollArea->setWidget(content);
    }

    auto *grid = qobject_cast<QGridLayout *>(content->layout());
    if (!grid) {
        delete content->layout();
        grid = new QGridLayout(content);
        grid->setContentsMargins(6, 6, 6, 6);
        grid->setHorizontalSpacing(8);
        grid->setVerticalSpacing(10);
        grid->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    }
    return grid;
}

// 按 softName 分组：每种软件新起一行；同软件从左到右，放不下换行
void layoutButtonsBySoftName(const QList<CustomButton *> &buttons, QGridLayout *grid, int maxCols)
{
    if (!grid || buttons.isEmpty() || maxCols < 1) {
        return;
    }

    clearGridLayout(grid);

    QStringList softOrder;
    QHash<QString, QList<CustomButton *>> groups;
    for (CustomButton *btn : buttons) {
        if (!btn) {
            continue;
        }
        const QString name = btn->softName();
        if (!groups.contains(name)) {
            softOrder.append(name);
        }
        groups[name].append(btn);
    }

    int row = 0;
    QWidget *parent = grid->parentWidget();
    for (const QString &name : softOrder) {
        int col = 0;
        bool placed = false;
        for (CustomButton *btn : groups.value(name)) {
            if (col >= maxCols) {
                ++row;
                col = 0;
            }
            if (parent) {
                btn->setParent(parent);
            }
            btn->show();
            grid->addWidget(btn, row, col);
            ++col;
            placed = true;
        }
        if (placed) {
            ++row; // 下一种软件从新行开始
        }
    }
}

// 不分类：从左到右顺序排列，放不下换行
void layoutButtonsFlow(const QList<CustomButton *> &buttons, QGridLayout *grid, int maxCols)
{
    if (!grid || buttons.isEmpty() || maxCols < 1) {
        return;
    }

    clearGridLayout(grid);

    int row = 0;
    int col = 0;
    QWidget *parent = grid->parentWidget();
    for (CustomButton *btn : buttons) {
        if (!btn) {
            continue;
        }
        if (col >= maxCols) {
            ++row;
            col = 0;
        }
        if (parent) {
            btn->setParent(parent);
        }
        btn->show();
        grid->addWidget(btn, row, col);
        ++col;
    }
}

int buttonColumnsForWidth(int availableWidth)
{
    constexpr int kBtnWidth = 66;
    constexpr int kSpacing = 8;
    const int cell = kBtnWidth + kSpacing;
    return qMax(1, availableWidth / cell);
}

} // namespace


MadOnionBox::MadOnionBox(const QString &userName, const QString &password, const QString &svnUrl,
                         bool offlineMode, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , offlineMode_(offlineMode)
{
    this->userName = userName;
    this->password = password;
    this->svnUrl = svnUrl;
    this->pythonEmbedPath = 
    QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("python_embed/python-3.11.2/python.exe"));
    // 在 setupUi 之前设置，避免之后改 flags 导致原生窗口重建变慢
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    ui->setupUi(this);
    setFixedSize(size()); // 锁定为 UI 设计尺寸，禁止拖拽改变窗口大小
    initUI();
}

MadOnionBox::~MadOnionBox()
{
    delete ui;
}

void MadOnionBox::initUI()
{
    setWindowTitle(tr("MadOnionBox"));
    const QString systemUserName = qgetenv("USERNAME");
    if (ui && ui->label_user) {
        ui->label_user->setText(systemUserName.isEmpty() ? tr("username") : systemUserName);
    }
    createTitleBar();

    // 任务结果信号只连一次，避免重复入队时重复弹窗
    connect(TaskExecutor::instance(), &TaskExecutor::taskFinished, this,
            &MadOnionBox::onDownloadTaskFinished, Qt::UniqueConnection);
    connect(TaskExecutor::instance(), &TaskExecutor::allFinished, this,
            &MadOnionBox::onAllDownloadTasksFinished, Qt::UniqueConnection);
    // 切换工具箱下拉列表绑定槽函数
    connect(ui->comboBox_dccPlug, &QComboBox::currentTextChanged, this, &MadOnionBox::switchToolbox);
    // 托盘放到事件循环里再建，避免挡住首次 show
    QTimer::singleShot(0, this, [this]() { createTrayIcon(); });

}
// 初始化数据
void MadOnionBox::initData()
{
    if (offlineMode_) {
        if (infoLabel) {
            infoLabel->setText(tr("离网模式：跳过服务器同步"));
        }
        if (!loadConfigFile()) {
            QMessageBox::warning(this, tr("离网模式"),
                                 tr("无法加载本地配置。请联网登录并完成一次同步后再试。"));
            return; // 只有加载失败才停
        }
    } else {
        if (!downLoadConfigFile()) {
            //QMessageBox::warning(this, tr("下载"), tr("下载配置文件失败"));
            return;
        }
        if (!loadConfigFile()) {
            //QMessageBox::warning(this, tr("加载"), tr("加载配置文件失败"));
            return;
        }
    }    
    // 在线/离网只要配置读成功，都继续后续初始化
    downLoadToolsInConfig();
    createDccButtons();
    createPythonScriptButtons();
    arrangeButtons();
}



void MadOnionBox::onDownloadClicked()
{
    initData();
}
// 下载配置文件
bool MadOnionBox::downLoadConfigFile()
{
    // 2) 把下载需要的数据拷出来（按值），后面 lambda 只带这些拷贝，不带 this
    const QString user = userName;
    const QString pwd = password;
    const QString url = svnUrl;
    const QString path =
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config"));
    Downloader downloader(Downloader::Mode::Svn);
    downloader.setUserName(user);
    downloader.setPassword(pwd);
    downloader.setSourceUrl(url);
    downloader.setLocalPath(path);
    const Downloader::Result result = downloader.download();
    return result.success;
}
// 加载配置文件
bool MadOnionBox::loadConfigFile()
{
    // 下载成功后，打开下载的config.ini文件
    QString configFile=QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/config.ini"));
    QFile configFileObj(configFile);
    if (configFileObj.exists()) {
        if (configFileObj.open(QIODevice::ReadOnly)) {
            QByteArray data = configFileObj.readAll();
            configFileObj.close();
            QJsonParseError parseError;
            boxConfigJson = QJsonDocument::fromJson(data, &parseError).object();
            if (parseError.error != QJsonParseError::NoError) {
                qDebug() << "配置文件解析错误:" << parseError.errorString();
                QMessageBox::critical(this, "错误", "配置文件解析错误，请检查配置文件格式是否正确。");
            }
            else{
                qDebug() << "配置文件加载成功。";   
            }
        } else {
            qDebug() << "无法打开配置文件:" << configFile;
            QMessageBox::critical(this, "错误", "无法打开配置文件，请检查文件权限。");
        }
    } else {
        qDebug() << "配置文件不存在:" << configFile;
        QMessageBox::critical(this, "错误", "配置文件不存在，请检查配置文件是否存在。");
    }
    qDebug() << "配置文件加载成功。";
    //加载所有工具箱到下来菜单
    QJsonObject dccTools = boxConfigJson.value(QStringLiteral("dccTools")).toObject();
    for (const QString &dccToolName : dccTools.keys()) {
        ui->comboBox_dccPlug->addItem(dccToolName);
    }
    return !boxConfigJson.isEmpty();

}
// 下载配置文件中记录的工具
void MadOnionBox::downLoadToolsInConfig()
{
    if (boxConfigJson.isEmpty()) {
        qDebug() << "配置文件为空，无法下载工具。";
        return;
    }
    if (offlineMode_) {
        qDebug() << "离网模式，跳过下载工具。";
        return;
    }

    const QString user = userName;
    const QString pwd = password;
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString dccToolsDir = QDir(appDir).filePath(QStringLiteral("dccTools"));
    const QString pythonScriptDir = QDir(appDir).filePath(QStringLiteral("pythonScript"));

    int enqueued = 0;
    auto enqueueSvnDownload = [&](const QString &url, const QString &localPath, const QString &label) {
        if (url.isEmpty() || localPath.isEmpty()) {
            return;
        }
        qDebug() << "入队下载:" << label << url << "->" << localPath;
        TaskExecutor::instance()->enqueue([user, pwd, url, localPath, label]() -> TaskResult {
            Downloader downloader(Downloader::Mode::Svn);
            downloader.setUserName(user);
            downloader.setPassword(pwd);
            downloader.setSourceUrl(url);
            downloader.setLocalPath(localPath);
            const Downloader::Result result = downloader.download();
            if (result.success) {
                return {true, QStringLiteral("%1：%2").arg(label, result.message)};
            }
            return {false, QStringLiteral("%1 下载失败：%2").arg(label, result.message)};
        });
        ++enqueued;
    };

    // pythonEnv → /python_embed（保留 URL 最后一层目录名）
    {
        const QJsonObject pythonEnvInfo = boxConfigJson.value(QStringLiteral("pythonEnv")).toObject();
        const QString pyEnvUrl = pythonEnvInfo.value(QStringLiteral("url")).toString().trimmed();
        if (!pyEnvUrl.isEmpty()) {
            const QString localPath = QDir(appDir).filePath( QStringLiteral("python_embed"));
            enqueueSvnDownload(pyEnvUrl, localPath, QStringLiteral("Python环境"));            
        }
    }

    // dccTools → /dccTools/<工具名>，每个工具单独检出
    {
        const QJsonObject dccTools = boxConfigJson.value(QStringLiteral("dccTools")).toObject();
        for (const QString &dccToolName : dccTools.keys()) {
            QString url = dccTools[dccToolName].toObject().value("url").toString();
            qDebug() << "dccToolName: " << dccToolName << " url: " << url;
            if (dccToolName.isEmpty() || url.isEmpty()) {
                qDebug() << "跳过无效 dccTools 项:" << dccToolName;
                continue;
            }
            enqueueSvnDownload(url, QDir(dccToolsDir).filePath(dccToolName),
                               QStringLiteral("DCC工具 %1").arg(dccToolName));
            
        }
    }

    // pythonScripts → /pythonScript/<工具名>，字典内每个工具单独检出
    {
        const QJsonObject pytoolInfo = boxConfigJson.value(QStringLiteral("pythonScripts")).toObject();
        for (const QString &py_tool : pytoolInfo.keys()) {
            QJsonObject py_tool_info = pytoolInfo[py_tool].toObject();
            QString url = py_tool_info.value("url").toString();
            if (py_tool.isEmpty() || url.isEmpty()) {
                qDebug() << "跳过无效 pythonScripts 项:" << py_tool;
                continue;
            }
            enqueueSvnDownload(url, QDir(pythonScriptDir).filePath(py_tool),
                               QStringLiteral("脚本 %1").arg(py_tool));
        }
    }

    if (enqueued == 0) {
        qDebug() << "配置中没有有效的下载地址。";
        return;
    }

    // 下载中：显示跑马灯、关闭窗口交互
    setDownloadBusy(true);
    if (infoLabel) {
        infoLabel->setText(tr("正在同步工具与环境..."));
    }
}
void MadOnionBox::createDccButtons()
{
    // 创建dcc按钮前先检查列表，如果列表不为空则清空
    if (!dccButtons.isEmpty()) {
        for (CustomButton *button : dccButtons) {
            delete button;
        }
        dccButtons.clear();
    }
    if (boxConfigJson.isEmpty()) {
        qDebug() << "配置文件未加载，无法创建按钮。";
        this->infoLabel->setText("配置文件未加载，无法创建按钮。");
        return;
    }
    qDebug() << "配置文件已加载，开始创建按钮。";
    // 遍历dccinfo中的工具设置,如果数据为空则跳过
    QJsonObject dccInfo = boxConfigJson["dccInfo"].toObject();
    for (const QString &softName : dccInfo.keys()) {
        QJsonObject dccInfoDic = dccInfo[softName].toObject();
        if (dccInfoDic.isEmpty()) continue; // 跳过空对象
        qDebug() << "加载 app:" << softName;
        this->loadSoftwareInfoFromRegistry(softName, dccInfoDic);
    }
    qDebug() << "创建按钮完成。"<< dccButtons.size();

}

// 根据json数据,从注册表获取软件信息
void MadOnionBox::loadSoftwareInfoFromRegistry(QString dccName, QJsonObject dccInfoDic)
{
    // 读取注册表中的软件信息
    try {
        QString regPath= dccInfoDic.value("keyX").toString(); // 例如 "HKEY_LOCAL_MACHINE\\SOFTWARE\\Side Effects Software"
        qDebug() << "读取注册表路径:" << regPath;
        QSettings reg(regPath, QSettings::NativeFormat);
        QStringList versions = reg.childGroups(); // 获取所有版本号(子项)
        QStringList validVersions;
        // 过滤版本号,只保留符合正则表达式的版本
        for (const QString &version : versions) {
            QRegularExpression re(dccInfoDic.value("reStr").toString());
            if (re.match(version).hasMatch())
                validVersions.append(version);
        }
        //添加随窗口大小自动调整布局
       
        qDebug() << "找到版本列表:" << validVersions;      
        // 遍历所有版本,获取安装路径,并创建按钮
        for (const QString &version : validVersions)
        {            
            //for (int i = 0; i < 2; ++i) {
            reg.beginGroup(version);
            QString keyPath = dccInfoDic.value("keyPath").toString().replace("@version@", version);
            //qDebug() << "正在处理版本:" << version << "注册表路径:" << keyPath;
            QSettings regPathSet(keyPath, QSettings::NativeFormat);
            QString installPath = regPathSet.value(dccInfoDic.value("keyLocation").toString()).toString();
            //qDebug() << "软件版本:" << version << "安装目录:" << installPath;
            reg.endGroup();
            // 创建按钮
            QString iconPath =installPath+ dccInfoDic.value("icon").toString();
            if (!QFile::exists(iconPath)) {
                iconPath = ":/icon/default.png"; // 默认图标
            }
            QString executePath = installPath + dccInfoDic.value("appPath").toString();
            //qDebug() << "软件版本:" << version << "可执行文件路径:" << executePath;
            QString buttonText = version;
            QRegularExpression re(dccInfoDic.value("reStr").toString());
            if (re.match(version).hasMatch())
                {
                    buttonText=re.match(version).captured(0); // 提取版本号中的数字部分
                }
            if (!QFile::exists(executePath)) {
                qDebug() << "可执行文件不存在,跳过按钮创建:" << executePath;
                continue; // 跳过不存在的可执行文件
            }
            qDebug() << "创建按钮:" << buttonText << " " << iconPath << " " << executePath;

            // appArgs: JSON 字符串数组，如 [] 或 ["--nukex"]
            QStringList arguments;
            const QJsonValue appArgsVal = dccInfoDic.value(QStringLiteral("appArgs"));
            if (appArgsVal.isArray()) {
                for (const QJsonValue &argItem : appArgsVal.toArray()) {
                    QString arg = argItem.toString().trimmed();
                    if (!arg.isEmpty()) {
                        // 把配置文件中记录的@**@替换为下拉菜单中选定的dcc工具目录
                        QString dccToolPath = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("dccTools"));
                        dccToolPath =dccToolPath + "/" + ui->comboBox_dccPlug->currentText();
                        dccToolPath = arg.replace("@**@", dccToolPath);
                        arguments.append(dccToolPath);
                    }
                }
            }

            // env: JSON 对象，如 {} 或 {"MAYA_MODULE_PATH":"D:/x","PATH":"+;D:/bin"}
            QMap<QString, QString> environment;
            const QJsonObject envObj = dccInfoDic.value(QStringLiteral("env")).toObject();
            for (auto it = envObj.constBegin(); it != envObj.constEnd(); ++it) {
                environment.insert(it.key(), it.value().toString());
            }

            // prePyScript: 启动前脚本路径，空字符串表示不跑
            QString prePyScript = dccInfoDic.value(QStringLiteral("prePyScript")).toString().trimmed();
            if (!prePyScript.isEmpty() && !QFileInfo(prePyScript).isAbsolute()) {
                prePyScript = QDir(QCoreApplication::applicationDirPath()).filePath(prePyScript);
            }

            CustomButton *button = new CustomButton(this);
            button->setSoftName(dccName);
            button->setVersion(buttonText);
            button->setIconPath(iconPath);
            button->setExecutePath(executePath);
            button->setArguments(arguments);
            button->setEnvironment(environment);
            button->setPrePythonScript(prePyScript);

            dccButtons.append(button);        
        }
    }
    catch (...) {
        qDebug() << "Error accessing registry.";
    }
}
// 创建python脚本按钮
void MadOnionBox::createPythonScriptButtons()
{
    if (boxConfigJson.isEmpty()) {
        qDebug() << "配置文件未加载，无法创建按钮。";
        return;
    }
    // 创建python脚本按钮前先检查列表，如果列表不为空则清空
    if (!appButtons.isEmpty()) {
        for (CustomButton *button : appButtons) {
            delete button;
        }
        appButtons.clear();
    }
    qDebug() << "配置文件已加载，开始创建按钮。";
    // 遍历pythonScripts中的脚本设置,如果数据为空则跳过
    QJsonObject pythonScripts = boxConfigJson["pythonScripts"].toObject();
    for (const QString &scriptName : pythonScripts.keys()) {
        QString scriptPath = QCoreApplication::applicationDirPath()+"/pythonScript/"+scriptName;
        QJsonObject pythonScriptsDic = pythonScripts[scriptName].toObject();
        if (pythonScriptsDic.isEmpty()) continue; // 跳过空对象
        CustomButton *button = new CustomButton(this);
        QString butName =pythonScriptsDic.value("toolName").toString();
        QString butIcon =pythonScriptsDic.value("iconPath").toString();
        butIcon=scriptPath + "/" + butIcon;
        qDebug() << "iconPath:" << butIcon;     
        if (!QFile::exists(butIcon)) {
            butIcon = ":/icon/default.png"; // 默认图标
        }
        
        QStringList arguments;
        scriptPath=scriptPath +"/main.py";
        arguments.append(scriptPath);
        
        button->setSoftName(scriptName);
        button->setVersion(butName);
        button->setIconPath(butIcon);
        button->setExecutePath(this->pythonEmbedPath);
        button->setArguments(arguments);

        appButtons.append(button);

    }
}

// 将按钮列表中的按钮添加到主窗口的控件中，并按软件类型分行排列
void MadOnionBox::arrangeButtons()
{
    // DCC：每种软件新起一行；同软件多版本从左到右，一行放不下则换行
    if (!dccButtons.isEmpty() && ui && ui->groupBox_dcc) {
        qDebug() << "开始布局 DCC 按钮，数量:" << dccButtons.size();
        if (QGridLayout *grid = ensureScrollGrid(ui->groupBox_dcc)) {
            const int cols = buttonColumnsForWidth(qMax(200, ui->groupBox_dcc->width() - 36));
            layoutButtonsBySoftName(dccButtons, grid, cols);
        }
    }

    // 工具箱脚本：不分类，直接从左到右顺序排列
    if (!appButtons.isEmpty() && ui && ui->groupBox_app) {
        qDebug() << "开始布局脚本按钮，数量:" << appButtons.size();
        if (QGridLayout *grid = ensureScrollGrid(ui->groupBox_app)) {
            const int cols = buttonColumnsForWidth(qMax(200, ui->groupBox_app->width() - 36));
            layoutButtonsFlow(appButtons, grid, cols);
        }
    }
}
//
// 切换工具箱
void MadOnionBox::switchToolbox()
{
    qDebug() << "切换工具箱:" << ui->comboBox_dccPlug->currentText();
    createDccButtons();
    createPythonScriptButtons();
    arrangeButtons();
}







///////////////////////////////////////////////////////////////////////////////////////////自动化逻辑
// 单个下载任务完成（可能还有后续任务在队列中）
void MadOnionBox::onDownloadTaskFinished(bool success, const QString &message)
{
    if (infoLabel) {
        infoLabel->setText(message);
    }
    if (!success) {
        // 排队弹窗，避免在 TaskExecutor 回调里直接模态对话框嵌套事件循环导致崩溃
        const QString text = message;
        QTimer::singleShot(0, this, [this, text]() {
            QMessageBox::warning(this, tr("下载"), text);
        });
    }
}

// 队列全部完成后：隐藏进度条并恢复交互
void MadOnionBox::onAllDownloadTasksFinished()
{
    setDownloadBusy(false);
}

void MadOnionBox::setDownloadBusy(bool busy)
{
    if (busyBar_) {
        busyBar_->setVisible(busy);
        if (busy) {
            busyBar_->raise();
        }
    }
    if (ui && ui->centralwidget) {
        ui->centralwidget->setEnabled(!busy);
    }
    if (userSettingButton) {
        userSettingButton->setEnabled(!busy);
    }
    if (updateButton) {
        updateButton->setEnabled(!busy);
    }
    if (minimizeButton) {
        minimizeButton->setEnabled(!busy);
    }
    if (closeButton) {
        closeButton->setEnabled(!busy);
    }
}
// 窗口置顶
bool MadOnionBox::nativeEvent(const QByteArray &eventType, void *message, qintptr *result)
{
    if (eventType == "windows_generic_MSG" || eventType == "windows_dispatcher_MSG") {
        auto *msg = static_cast<MSG *>(message);
        static const UINT kRaise = RegisterWindowMessageW(L"MadOnionBox_Raise");
        if (msg && msg->message == kRaise) {
            show();
            showNormal();
            raise();
            activateWindow();
            *result = 0;
            return true;
        }
    }
    return QMainWindow::nativeEvent(eventType, message, result);
}

//创建更新,最小化,关闭按钮
void MadOnionBox::createTitleBar()
{
    QString btSyl = "QPushButton {text-align: center;padding-top: 0px;padding: 5px;border-radius:6px;border:0px groove gray;border-style:outset;}\n\
                    QPushButton:hover {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #ffffff, stop: 1 #bababa);}\n\
                    QPushButton:pressed {border-style: inset;}";
    // 用户设置按钮
    userSettingButton = new QPushButton(this);
    QPixmap userPixmap(QStringLiteral(":/icons/setting.png"));
    userSettingButton->setIcon(QIcon(userPixmap)); // 设置图标
    userSettingButton->setToolTip("User Settings");
    userSettingButton->setGeometry(this->width() - 120, 6, 28, 28); // 设置位置和大小
    // connect(userSettingButton, &QPushButton::clicked, this, [this]() {      
    //     this->login(true); // 调用登录函数，并传递 true 参数
    // });
    //设置按钮样式
    userSettingButton->setStyleSheet(btSyl);
    // 投影
    QGraphicsDropShadowEffect *shadow0 = new QGraphicsDropShadowEffect(this);
    shadow0->setBlurRadius(6);
    shadow0->setOffset(2);
    shadow0->setColor(QColor(0, 0, 0, 120));
    userSettingButton->setGraphicsEffect(shadow0);
    // 更新按钮
    updateButton = new QPushButton(this);
    QPixmap pixmap(QStringLiteral(":/icons/update.png"));
    updateButton->setIcon(QIcon(pixmap)); // 设置更新图标
    updateButton->setToolTip("Update");
    updateButton->setGeometry(this->width() - 90, 6, 28, 28); // 设置位置和大小
    connect(updateButton, &QPushButton::clicked, this, &MadOnionBox::onDownloadClicked);
    //设置按钮样式
    updateButton->setStyleSheet(btSyl);
    // 投影
    QGraphicsDropShadowEffect *shadow1 = new QGraphicsDropShadowEffect(this);
    shadow1->setBlurRadius(6);
    shadow1->setOffset(2);
    shadow1->setColor(QColor(0, 0, 0, 120));
    updateButton->setGraphicsEffect(shadow1);
    // 最小化按钮
    minimizeButton = new QPushButton(this);
    QPixmap minPixmap(QStringLiteral(":/icons/minimize.png"));
    minimizeButton->setIcon(QIcon(minPixmap)); // 设置最小化图标
    minimizeButton->setToolTip("Minimize");
    minimizeButton->setGeometry(this->width() - 60, 6, 28, 28); // 设置位置和大小
    connect(minimizeButton, &QPushButton::clicked, this, [this]() {
        this->hide(); // 隐藏到托盘，不在任务栏保留
    });
    //设置按钮样式
    minimizeButton->setStyleSheet(btSyl);
    // 投影
    QGraphicsDropShadowEffect *shadow2 = new QGraphicsDropShadowEffect(this);
    shadow2->setBlurRadius(6);
    shadow2->setOffset(2);
    shadow2->setColor(QColor(0, 0, 0, 120));
    minimizeButton->setGraphicsEffect(shadow2);
    // 关闭按钮
    closeButton = new QPushButton(this);
    QPixmap closePixmap(QStringLiteral(":/icons/close.png"));
    closeButton->setIcon(QIcon(closePixmap)); // 设置关闭图标   
    closeButton->setToolTip("Close");
    closeButton->setGeometry(this->width() - 30, 6, 28, 28); // 设置位置和大小
    connect(closeButton, &QPushButton::clicked, qApp, &QCoreApplication::quit); // 关闭应用
    //设置按钮样式
    closeButton->setStyleSheet(btSyl);
    // 投影
    QGraphicsDropShadowEffect *shadow3 = new QGraphicsDropShadowEffect(this);
    shadow3->setBlurRadius(6);
    shadow3->setOffset(2);
    shadow3->setColor(QColor(0, 0, 0, 120));
    closeButton->setGraphicsEffect(shadow3);

    // 版本号
    versionLabel = new QLabel(this);
    versionLabel->setText(QStringLiteral("r1.0.0"));
    versionLabel->setStyleSheet(QStringLiteral("color:#888;font-size:10px;"));
    versionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    versionLabel->setGeometry(this->width() - 72, this->height() - 20, 64, 14);
    versionLabel->raise();
    // 信息显示label
    infoLabel = new QLabel(this);
    infoLabel->setText("加载中.....");
    infoLabel->setStyleSheet(QStringLiteral("color:#888;font-size:10px;"));
    infoLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    infoLabel->setGeometry(10, this->height() - 20, this->width()-30, 14);
    infoLabel->raise();


    // 底部跑马灯进度条（不确定进度）
    busyBar_ = new QProgressBar(this);
    busyBar_->setRange(0, 0);
    busyBar_->setTextVisible(false);
    busyBar_->setGeometry(8, height() - 34, width() - 16, 8);
    busyBar_->hide();

}

// 窗口拖动相关函数
void MadOnionBox::mousePressEvent(QMouseEvent *event)
{
    if (busyBar_ && busyBar_->isVisible()) {
        return; // 下载中禁止拖动窗口
    }
    if (event->button() == Qt::LeftButton) {
        isDragging = true;
        startPos = event->globalPosition().toPoint() - frameGeometry().topLeft();
        event->accept();
    }
}
void MadOnionBox::mouseMoveEvent(QMouseEvent *event)
{
    if (isDragging && (event->buttons() & Qt::LeftButton)) {
        endPos = event->globalPosition().toPoint() - startPos;
        move(endPos);
        event->accept();
    }
}
void MadOnionBox::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        isDragging = false;
        event->accept();
    }
}

// 系统托盘图标相关函数
void MadOnionBox::closeEvent(QCloseEvent *event)
{
    if (trayIcon) {
        hide(); // 隐藏窗口
        event->ignore(); // 忽略关闭事件，防止程序退出
    }
}

// 创建托盘图标和菜单
void MadOnionBox::createTrayIcon()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning("系统托盘不可用，跳过托盘图标创建");
        return;
    }

    trayIcon = new QSystemTrayIcon(this);
    // 资源路径须与 resources.qrc 的 prefix/alias 一致：:/icons/mo.png
    const QIcon icon(QStringLiteral(":/icons/mo.png"));
    if (icon.isNull()) {
        qWarning("托盘图标加载失败: :/icons/mo.png");
    }
    trayIcon->setIcon(icon);
    trayIcon->setToolTip(tr("MadOnionBox"));

    QMenu *trayMenu = new QMenu(this);
    QAction *restoreAction = new QAction(tr("显示"), this);
    QAction *quitAction = new QAction(tr("退出"), this);

    connect(restoreAction, &QAction::triggered, this, &MadOnionBox::onTrayShow);
    connect(quitAction, &QAction::triggered, this, &MadOnionBox::onTrayQuit);

    trayMenu->addAction(restoreAction);
    trayMenu->addAction(quitAction);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();

    connect(trayIcon, &QSystemTrayIcon::activated, this, &MadOnionBox::onTrayIconActivated);
}
void MadOnionBox::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            raise(); // 提升窗口到最前
            activateWindow(); // 激活窗口
        }
    }
}
void MadOnionBox::onTrayShow()
{
    showNormal();
    raise(); // 提升窗口到最前
    activateWindow(); // 激活窗口
}

void MadOnionBox::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    // 兜底处理系统最小化动作：最小化后直接隐藏到托盘
    if (event->type() == QEvent::WindowStateChange && isMinimized()) {
        QTimer::singleShot(0, this, [this]() {
            this->hide();
        });
    }
}

void MadOnionBox::onTrayQuit()
{
    qApp->quit();
}