#include "MadOnionBox.h"
#include "ui/ui_main.h" // 包含由 uic 根据 main.ui 生成的头文件,必须和cmake一致

// 构造函数
MadOnionBox::MadOnionBox(QWidget *parent)
    : QMainWindow(parent),      // 调用父类构造函数
      ui(new Ui::MainWindow)    // 创建 UI 类的实例
{
    // 1. 实例化 .ui 文件中所有的控件。
    // 2. 设置所有控件的属性（如大小、文本等）。
    // 3. 创建并应用所有布局。
    // 4. 将所有控件的信号连接到在 Designer 中设置的槽。
    // 5. 将整个 UI 设置到 'this' (MadOnionBox 实例) 上。    
    ui->setupUi(this);
    this->init();
    this->createTrayIcon(); // 确保托盘图标初始化
    // --- 从这里开始，UI 已经完全加载 ---

    // 现在可以通过 ui 指针安全地访问任何在 .ui 文件中命名的控件
    // 例如，连接一个名为 'loginButton' 的按钮的点击信号到一个槽函数
    // connect(ui->loginButton, &QPushButton::clicked, this, &MadOnionBox::handleLogin);
    
    // 测试多线程
    // if (worker) {
    //     qDebug() << "Adding tasks to CommandWorker";
    //     worker->addTask("echo Task 0");
    //     worker->addTask("echo Task 1");
    //     QMetaObject::invokeMethod(worker, "start", Qt::QueuedConnection);
    //     //worker->start(); // ★启动任务队列
    // }
}
void MadOnionBox::init()
{
    // 初始化SVN信息
    svnIp = "47.94.221.103";
    userSettingsPath = QCoreApplication::applicationDirPath() + "/config/userSettings.ini";
    userSettings = new QSettings(userSettingsPath, QSettings::IniFormat);
    svnPath = QCoreApplication::applicationDirPath() + "/svn";
    svnPassword = userSettings->value("svn/password", "").toString();
    svnUserName = userSettings->value("svn/username", "").toString();
    // 如果用户名和密码为空,则弹窗提示输入
    if (svnUserName =="" && svnPassword =="")
    {
        SettingWindow *settingWindow = new SettingWindow(this);
        if (settingWindow->exec() == QDialog::Accepted) {
            qDebug() << "Settings accepted.";
            svnUserName = settingWindow->svnUserName();
            svnPassword = settingWindow->svnPassword();
        }
        delete settingWindow;
    }
    //保存到配置文件
    userSettings->setValue("svn/username", svnUserName);
    userSettings->setValue("svn/password", svnPassword);
    userSettings->sync();   


    // 创建进度条
    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 0); // 不确定进度时显示为动画

    progressBar->setGeometry(5, this->height()-20, this->width()+20, 15);
    progressBar->setVisible(false);

    

    // 实例化 CommandWorker 和 QThread 开启多线程
    worker = new CommandWorker();
    workerThread = new QThread(this);
    worker->moveToThread(workerThread);

    // 当所有任务完成时，发射 allTasksFinished 信号.
    connect(worker, &CommandWorker::allTasksFinished, this, [this](){
        qDebug() << "All tasks finished!";
    });
    connect(worker, &CommandWorker::showProgress, this, &MadOnionBox::showProgressBar);
    connect(worker, &CommandWorker::allTasksFinished, this, &MadOnionBox::hideProgressBar);

    // 调用svn下载更新配置文件
    this->downloadConfig();    
    // 设置无边框窗口
    // this->setWindowFlags(Qt::Window|Qt::CustomizeWindowHint); // 设置无边框窗口
    this->setWindowFlags(Qt::Window|Qt::FramelessWindowHint); // 设置无边框窗口
    // 读取系统用户名
    QString systemUserName = qgetenv("USERNAME");
    //修改文本框内容
    ui->label_user->setText(systemUserName);

    // 下载工具箱
    this->downloadTools();
    // 加载下拉菜单上次选择对象
    QString lastSelectedToolbox = userSettings->value("lastSelectedToolbox", "").toString();
    int index = ui->comboBox_dccPlug->findText(lastSelectedToolbox);
    if (index != -1) {
        ui->comboBox_dccPlug->setCurrentIndex(index);
    }   

    // 启动 worker 线程
    workerThread->start();
    // ui相关
    this->createTitleBar(); // 创建更新,最小化,关闭按钮
    // 根据配置文件加载dcc,并创建按钮
    this->loadDccInfo();
    this->createGroupBoxLayout(ui->groupBox_dcc); // 创建dcc自动布局
    this->createAppButtons(); // 创建工具按钮
    this->createGroupBoxLayout(ui->groupBox_app); // 创建工具自动布局
    // 信号槽连接
    // 当 workerThread 线程结束时，自动释放 worker 对象，防止内存泄漏。
    connect(workerThread, &QThread::finished, worker, &QObject::deleteLater);
    // 当 worker 完成一个任务时，发射 taskFinished 信号。这里用 lambda 作为槽函数，收到信号后可以安全地在主线程更新 ui
    connect(worker, &CommandWorker::taskFinished, this, [this](const QString &output){
        qDebug() << "Task Output:" << output;
        // 可以在这里更新UI
    // 链接下来菜单
    connect(ui->comboBox_dccPlug, &QComboBox::currentTextChanged, this, &MadOnionBox::switchToolbox);

    });

}
// svn下载配置文件
void MadOnionBox::downloadConfig()
{
    QString boxSetting= QApplication::applicationDirPath() + "/config/config.ini";
    // 检查网站端口是否能够访问
    if (!checkPortAccessible(svnIp, 3690)) {
        qDebug() << "SVN 端口不可访问，请检查网络连接或防火墙设置。";
        return;
    }
    // 判断本地配置文件是否存在,如果存在,则尝试直接使用svn更新
    QFile configFile(boxSetting);
    if (configFile.exists()) {
        qDebug() << "配置文件已存在，尝试更新...";
        QString command = QString("%1/svn.exe update --username %2 --password %3  %4")
                        .arg(MadOnionBox::svnPath)  // SVN 客户端路径                    
                        .arg(MadOnionBox::svnUserName) // SVN 用户名
                        .arg(MadOnionBox::svnPassword) // SVN 密码
                        .arg(boxSetting); // 本地配置文件路径
        QStringList output;
        runCommand(command, output);        
    }
    else {
        qDebug() << "配置文件不存在，尝试下载...";
        // 如果配置文件不存在,则尝试下载
        QString svnURL = "svn://47.94.221.103/evenPro/MOB_Cpp/config";
        QString command = QString("%1/svn.exe checkout --username %2 --password %3 --non-interactive --trust-server-cert %4 %5")
                        .arg(MadOnionBox::svnPath)  // SVN 客户端路径                    
                        .arg(MadOnionBox::svnUserName) // SVN 用户名
                        .arg(MadOnionBox::svnPassword) // SVN 密码
                        .arg(svnURL) // SVN 仓库URL
                        .arg(QApplication::applicationDirPath() + "/config/"); // 本地配置文件路径
        QStringList output;
        qDebug() << "执行命令:" << command;
        runCommand(command, output);        
    }
    QFile file(boxSetting);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QByteArray data = file.readAll(); // 读取全部内容
        file.close();

        // 解析 JSON
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(data, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()) {
            this->boxConfigJson = doc.object();
            // 访问字段
            QString version = this->boxConfigJson["versionNumber"].toString();
            qDebug() << "版本号:" << version;

        } else {
            qDebug() << "JSON解析失败:" << err.errorString();
        }
    } else {
        qDebug() << "文件打开失败";
    }
}

// SVN多线程更新
bool MadOnionBox::svnUpdate(const QString &localSvnPath, const QString &svnUrl)
{
    // 1检查端口是否可以连接
    if (!checkPortAccessible(svnIp, 3690)) {
        qDebug() << "SVN 端口不可访问，请检查网络连接或防火墙设置。";
        return false;
    }
    // 2检查传本地svn是否可用
    if (!QFile::exists(this->svnPath + "/svn.exe")) {
        qDebug() << "SVN 客户端丢失，请联系技术支持，并检查路径:" << this->svnPath + "/svn.exe";
        return false;
    }
    // 3检查svnPath本地路径是否为svn工作副本
    QStringList output;
    QString command = QString("%1/svn.exe --username %2 --password %3 log -l 1 %4")
                    .arg(this->svnPath)  // SVN 客户端路径
                    .arg(this->svnUserName) // SVN 用户名
                    .arg(this->svnPassword) // SVN 密码
                    .arg(localSvnPath);  // SVN 仓库URL
    runCommand(command, output);
    // 返回结果错误，更新终止
    if (output.size() != 3) {
        qDebug() << "命令执行错误，更新终止";
        return false;
    }
    // 根据反馈判断是否为svn工作副本，如果是则进行更新，否则进行checkout，判断reruncode
    if (output[2]!='0') {
        // 不是工作副本，进行checkout，如果目录存在，则删除后重新checkout
        QDir dir(localSvnPath);
        if (dir.exists() and !localSvnPath.endsWith("/bin/")) {
            qDebug() << "本地路径存在，删除旧目录:" << localSvnPath;
            dir.removeRecursively();
        }
        qDebug() << "本地路径不是SVN工作副本，尝试进行checkout...";
        QString checkoutCommand = QString("%1/svn.exe checkout --username %2 --password %3 --non-interactive --trust-server-cert %4 %5")
                        .arg(this->svnPath)  // SVN 客户端路径                    
                          .arg(this->svnUserName) // SVN 用户名
                        .arg(this->svnPassword) // SVN 密码
                        .arg(svnUrl) // SVN 仓库URL
                        .arg(localSvnPath); // 本地配置文件路径
        // 使用多线程执行checkout
        if (worker) {
            qDebug() << "Adding tasks to CommandWorker";
            worker->addTask(checkoutCommand);
            return true;
        } else {
            qDebug() << "CommandWorker 未启动";
            return false;
        }
    } else {
        // 是工作副本，进行update
        qDebug() << "本地路径是SVN工作副本，尝试进行update...";
        QString updateCommand = QString("%1/svn.exe update --username %2 --password %3  %4")
                        .arg(this->svnPath)  // SVN 客户端路径                    
                        .arg(this->svnUserName) // SVN 用户名
                        .arg(this->svnPassword) // SVN 密码
                        .arg(localSvnPath); // 本地配置文件路径
        // 使用多线程执行update
        if (worker) {
            qDebug() << "Adding tasks to CommandWorker";
            worker->addTask(updateCommand);
            return true;
        } else {
            qDebug() << "CommandWorker 未启动";
            return false;
        }
    }

    return false;
}
void MadOnionBox::downloadTools()
{
    // 这里是下载工具箱的逻辑,所有dcc工具放到dccTools目录,第三方app放到bin目录
    QString toolBoxPath= QApplication::applicationDirPath() + "/dccTools";
    QDir dir(toolBoxPath);
    if (!dir.exists()) {
        bool ok=dir.mkpath(toolBoxPath); // 创建目录
        qDebug() << "创建工具箱目录:" << toolBoxPath << "结果:" << ok;
    }
    QString pyScriptPath = QApplication::applicationDirPath() + "/pythonScripts";
    QDir pyDir(pyScriptPath);   
    if (!pyDir.exists()) {
        bool res=pyDir.mkpath(pyScriptPath); // 创建目录
        qDebug() << "创建py工具目录:"<< pyScriptPath << "结果:"<< res;
    } 
    // 从boxConfigJson字典中获取url
    if (this->boxConfigJson.isEmpty()) {
        qDebug() << "配置文件未加载，无法创建按钮。";
        return;
    }
    // 下载python环境
    QJsonObject pythonEnvInfo = boxConfigJson["pythonEnv"].toObject();
    QString pyVersion =  pythonEnvInfo.isEmpty() ? QString() : pythonEnvInfo.keys().first();
    qDebug() << "Python环境版本:" << pyVersion;
    QString pyEnvUrl = pythonEnvInfo[pyVersion].toObject().value("url").toString();
    qDebug() << "Python环境URL:" << pyEnvUrl;
    this->svnUpdate(QApplication::applicationDirPath() + "/"+pyVersion, pyEnvUrl);
    this->pyEvnPath = QApplication::applicationDirPath() + "/"+pyVersion+"/python.exe";
    qDebug() << "Python环境路径:" << this->pyEvnPath;
    // 更新dcc工具
    QJsonObject dccToolInfo = boxConfigJson["dccTools"].toObject();
    for (const QString &dccToolName : dccToolInfo.keys()) {
        qDebug() << "工具名称:" << dccToolInfo;
        qDebug() << "工具URL:" << dccToolInfo[dccToolName].toObject().value("url").toString();
        this->svnUpdate(toolBoxPath + "/" + dccToolName, dccToolInfo[dccToolName].toObject().value("url").toString());
        //工具仓库添加到下拉菜单
        ui->comboBox_dccPlug->addItem(dccToolName);
    }
    // 更新第三方app
    QJsonObject thirdAppInfo = boxConfigJson["thirdPartyApps"].toObject();
    for (const QString &appInfo : thirdAppInfo.keys()) {
        qDebug() << "第三方App URL:" << thirdAppInfo[appInfo].toObject().value("url").toString();
        this->svnUpdate(QApplication::applicationDirPath() + "/" + appInfo, thirdAppInfo[appInfo].toObject().value("url").toString());
    }
    // python 环境,以及工具脚本
    QJsonObject pytoolInfo = boxConfigJson["pythonScripts"].toObject();
    for (const QString &py_tool : pytoolInfo.keys()) {
        qDebug() << "Python脚本 URL:" << pytoolInfo[py_tool].toObject().value("url").toString();
        this->svnUpdate(pyScriptPath+'/'+ py_tool, pytoolInfo[py_tool].toObject().value("url").toString());
    }
    QString python_script_path = QApplication::applicationDirPath() + "/python_script";
    qDebug() << "Python脚本路径:" << python_script_path;
}

// 检查网络端口是否可以访问
bool MadOnionBox::checkPortAccessible(const QString &host, quint16 port, int timeoutMs)
{
    QTcpSocket socket;
    socket.connectToHost(host, port);
    bool connected = socket.waitForConnected(timeoutMs);
    socket.abort(); // 立即断开
    return connected;
}
// 根据配置文件加载dcc,并创建按钮
void MadOnionBox::loadDccInfo()
{
    if (boxConfigJson.isEmpty()) {
        qDebug() << "配置文件未加载，无法创建按钮。";
        return;
    }
    qDebug() << "配置文件已加载，开始创建按钮。";
    // 遍历dccinfo中的工具设置,如果数据为空则跳过
    QJsonObject dccInfo = boxConfigJson["dccInfo"].toObject();
    int dccIndex = 0;
    for (const QString &softName : dccInfo.keys()) {
        QJsonObject dccInfoDic = dccInfo[softName].toObject();
        if (dccInfoDic.isEmpty()) continue; // 跳过空对象
        qDebug() << "加载 app:" << softName;        
        this->loadSoftwareInfoFromRegistry(softName, dccInfoDic,dccIndex);
        dccIndex++;
    }
}

// 根据json数据,从注册表获取软件信息
void MadOnionBox::loadSoftwareInfoFromRegistry(QString dccName, QJsonObject dccInfoDic,int dccIndex)
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
        int buttonIndex = 0;
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
                buttonText=re.match(version).captured(0); // 提取版本号中的数字部分
            CustomButton *button = new CustomButton(buttonText, iconPath,executePath, this);
            button->softName = dccName;
            button->dccIndex = dccIndex;
            button->dccId = buttonIndex;
            if (!dccInfoDic.value("appArgs").toString().isEmpty()) {
                button->dccArgs.append(dccInfoDic.value("appArgs").toString());
            }

            //根据下拉菜单选项和dccinfo字典,构造启动参数
            QString comboBoxText = this->ui->comboBox_dccPlug->currentText();
            if (dccName == "maya") {
                QString scriptPath = QString(QCoreApplication::applicationDirPath().replace("\\", "/") + "/dccTools/" + comboBoxText + "/maya");
                qDebug() << "Maya脚本路径:" << scriptPath;
                button->dccArgs.append("-command");
                button->dccArgs.append("python(\"import sys\\nsys.path.append('" + scriptPath + "')\\nimport Jpy\")");
                qDebug() << "Maya启动参数:" << button->dccArgs;
            }
            button->dccInfoDic = dccInfoDic;
            dccButtons.append(button);
            buttonIndex++;
            //}
        }
    } catch (...) {
        qDebug() << "Error accessing registry.";
    }
}

// 创建python脚本按钮
void MadOnionBox::createAppButtons()
{
    // 遍历pythonScripts字典,创建按钮
    if (boxConfigJson.isEmpty()) {
        qDebug() << "配置文件未加载，无法创建按钮。";
        return;
    }
    QJsonObject pytoolInfo = boxConfigJson["pythonScripts"].toObject();
    int dccIndex = 0;
    for (const QString &py_tool : pytoolInfo.keys()) {
        QString scriptPath = QApplication::applicationDirPath() + "/pythonScripts/" + py_tool;
        QString buttonText = pytoolInfo[py_tool].toObject().value("toolName").toString();
        QString iconPath =":/icon/"+ pytoolInfo[py_tool].toObject().value("iconPath").toString();
        if (!QFile::exists(iconPath)) {
            iconPath = ":/icon/default.png"; // 默认图标
        }
        QString pythonExePath = this->pyEvnPath;
        CustomButton *button = new CustomButton(buttonText, iconPath, pythonExePath, this);
        button->softName = py_tool;
        button->dccIndex = 0;
        button->dccId = dccIndex;
        dccIndex++;
        button->dccArgs.append(scriptPath + "/main.py");
        appButtons.append(button);
    }
}
// 根据当前窗口大小,排布dcc按钮
void MadOnionBox::createGroupBoxLayout(QWidget *groupBox_widget)
{
    if (dccButtons.isEmpty()) return;
    // 先添加布局到groupbox
    QVBoxLayout *boxLayout = new QVBoxLayout(groupBox_widget);

    // 创建滚动区域    
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setStyleSheet("QScrollBar:vertical { width: 10px; }");
    boxLayout->addWidget(scrollArea);    
    QWidget *groupBoxWidget = new QWidget();
    QGridLayout *gridLayout = new QGridLayout(groupBoxWidget);    
    gridLayout->setAlignment(Qt::AlignTop); // 顶部对齐
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(groupBoxWidget);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    qDebug() << "创建groupbox布局完成";
}
void MadOnionBox::layoutButtons(QWidget *groupBox_widget, QList<CustomButton*> buttons)
{
    // 先获取groupbox的布局
    if (buttons.isEmpty()) return;
    // 第一层 boxLayout
    QVBoxLayout *boxLayout = qobject_cast<QVBoxLayout*>(groupBox_widget->layout());
    if (!boxLayout) return;
    // 第二层 scrollArea
    QScrollArea *scrollArea = qobject_cast<QScrollArea*>(boxLayout->itemAt(0)->widget());
    if (!scrollArea) return;
    // 第三层 groupBoxWidget
    QWidget *groupBoxWidget = scrollArea->widget();
    if (!groupBoxWidget) return;
    // 第四层 gridLayout
    QGridLayout *gridLayout = qobject_cast<QGridLayout*>(groupBoxWidget->layout());
    if (!gridLayout) return;

    // 重新排布按钮
    for (auto button : buttons) {
        //判断是否在布局中，如果在则移除
        if (gridLayout->indexOf(button) != -1) {
            gridLayout->removeWidget(button); // 先从布局中移除
            button->setParent(nullptr); // 解除父子关系
        }
    }
    //获取groupbox的宽度
    int boxWidth = groupBox_widget->width();
    int columns = boxWidth/66; // 每行按钮数
    int rowcount = 0;
    for (int i = 0; i < buttons.size(); ++i) {
        int row = (i - buttons[i]->dccId%columns) / columns + buttons[i]->dccIndex; // 每个dcc分组从新的一行开始
        int col = buttons[i]->dccId % columns;
        gridLayout->addWidget(buttons[i], row, col);
    }
}
// 执行命令行(阻塞),返回输出结果  1 执行输出 2 错误输出 3 退出码
bool MadOnionBox::runCommand(const QString &command, QStringList &output)
{
    QProcess proc;
    proc.start("cmd.exe", QStringList() << "/c" << command);
    // qDebug()<<"Running command in thread: " << command;
    proc.waitForFinished(-1); // 等待进程完成

    QString stdOutput = QString::fromLocal8Bit(proc.readAllStandardOutput());
    QString stdError  = QString::fromLocal8Bit(proc.readAllStandardError());
    int returnCode = proc.exitCode();

    qDebug() << "Standard Output:" << stdOutput;
    qDebug() << "Standard Error:" << stdError;
    qDebug() << "Return Code:" << returnCode;

    output << stdOutput;
    output << stdError;
    output << QString::number(returnCode);

    return (returnCode == 0);
}

// 析构函数
MadOnionBox::~MadOnionBox()
{
    // 当 MadOnionBox 对象被销毁时，删除 ui 指针以释放所有 UI 控件占用的内存
    delete ui;
    workerThread->quit();      // 请求线程退出
    workerThread->wait();      // 等待线程安全退出
}

// 窗口拖动相关函数
void MadOnionBox::mousePressEvent(QMouseEvent *event)
{
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
// 创建更新,最小化,关闭按钮
void MadOnionBox::createTitleBar()
{
    QString btSyl = "QPushButton {text-align: center;padding-top: 0px;padding: 5px;border-radius:6px;border:0px groove gray;border-style:outset;}\n\
                    QPushButton:hover {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #ffffff, stop: 1 #bababa);}\n\
                    QPushButton:pressed {border-style: inset;}";
    // 用户设置按钮
    userSettingButton = new QPushButton(this);
    QPixmap userPixmap(":/icon/setting.png");
    userSettingButton->setIcon(QIcon(userPixmap)); // 设置图标
    userSettingButton->setToolTip("User Settings");
    userSettingButton->setGeometry(this->width() - 120, 6, 28, 28); // 设置位置和大小
    connect(userSettingButton, &QPushButton::clicked, this, []() {
        // 这里添加用户设置逻辑 
        qDebug() << "User Settings button clicked";
    });
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
    QPixmap pixmap(":/icon/update.png");
    updateButton->setIcon(QIcon(pixmap)); // 设置更新图标
    updateButton->setToolTip("Update");
    updateButton->setGeometry(this->width() - 90, 6, 28, 28); // 设置位置和大小
    connect(updateButton, &QPushButton::clicked, this, &MadOnionBox::downloadTools); // 触发下载工具箱更新
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
    QPixmap minPixmap(":/icon/minimize.png");
    minimizeButton->setIcon(QIcon(minPixmap)); // 设置最小化图标
    minimizeButton->setToolTip("Minimize");
    minimizeButton->setGeometry(this->width() - 60, 6, 28, 28); // 设置位置和大小
    connect(minimizeButton, &QPushButton::clicked, this, [this]() {
        this->showMinimized(); // 最小化窗口
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
    QPixmap closePixmap(":/icon/close.png");
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
}
// 创建托盘图标和菜单
void MadOnionBox::createTrayIcon()
{
    trayIcon = new QSystemTrayIcon(this);
    QPixmap pixmap(":/icon/mo.png");
    trayIcon->setIcon(QIcon(pixmap));
    // trayIcon->setIcon(QIcon(":/icon/launcher.ico")); // 设置托盘图标
    // qDebug() << QResource(":/icon/launcher.ico").isValid();
    QMenu *trayMenu = new QMenu(this);
    QAction *restoreAction = new QAction("Restore", this);
    QAction *quitAction = new QAction("Quit", this);

    connect(restoreAction, &QAction::triggered, this, &QWidget::showNormal);
    connect(quitAction, &QAction::triggered, qApp, &QCoreApplication::quit);

    trayMenu->addAction(restoreAction);
    trayMenu->addAction(quitAction);

    trayIcon->setContextMenu(trayMenu);
    trayIcon->show();
    // 创建托盘按钮以及相关事件
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MadOnionBox::onTrayIconActivated);
    connect(restoreAction, &QAction::triggered, this, &MadOnionBox::onTrayShow);
    connect(quitAction, &QAction::triggered, this, &MadOnionBox::onTrayQuit);
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
// 窗口缩放
void MadOnionBox::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    int w = this->width();
    if (userSettingButton) userSettingButton->setGeometry(w - 120, 6, 28, 28);
    if (updateButton) updateButton->setGeometry(w - 90, 6, 28, 28);
    if (minimizeButton) minimizeButton->setGeometry(w - 60, 6, 28, 28);
    if (closeButton) closeButton->setGeometry(w - 30, 6, 28, 28);
    MadOnionBox::layoutButtons(ui->groupBox_dcc,this->dccButtons); // 重新排布dcc按钮
    MadOnionBox::layoutButtons(ui->groupBox_app,this->appButtons); // 重新排布第三方app按钮
}

void MadOnionBox::onTrayQuit()
{
    qApp->quit();
}
//进度条显示
void MadOnionBox::showProgressBar()
{
    if (this->progressBar) {
        qDebug() << "显示进度条";
        this->progressBar->setVisible(true);
        this->setEnabled(false); // 禁用主窗口交互
    }
}
//进度条隐藏
void MadOnionBox::hideProgressBar()
{
    if (this->progressBar) {
        qDebug() << "隐藏进度条";
        this->progressBar->setVisible(false);
        this->setEnabled(true); // 恢复主窗口交互
    }
}

// 切换工具箱
void MadOnionBox::switchToolbox()
{
    // 这里添加切换工具箱的逻辑
    qDebug() << "Switch Toolbox button clicked";
    QString comboBoxText = this->ui->comboBox_dccPlug->currentText();
    for (auto button : dccButtons) {
        QString dccName = button->softName;
        if (dccName == "maya") {
            QString scriptPath = QString(QCoreApplication::applicationDirPath().replace("\\", "/") + "/dccTools/" + comboBoxText + "/maya");
            qDebug() << "Maya脚本路径:" << scriptPath;
            button->dccArgs.clear(); // 清除旧参数
            button->dccArgs.append("-command");
            button->dccArgs.append("python(\"import sys\\nsys.path.append('" + scriptPath + "')\\nimport Jpy\")");
            qDebug() << "Maya启动参数:" << button->dccArgs;
        }
    }
    // 配置文件记录选择项目方便加载    
    this->userSettings->setValue("lastSelectedToolbox", comboBoxText);
}




// 多线程类
CommandWorker::CommandWorker(QObject *parent) : QObject(parent) {}

void CommandWorker::addTask(const QString &cmd) {
    qDebug() << "Add task :" << cmd;
    taskQueue.enqueue(cmd);
    if (!isRunning) {
        //QMetaObject::invokeMethod(this, "start", Qt::QueuedConnection);
        this->start();
    }
}

void CommandWorker::start() {
    if (isRunning) return; // 防止重复启动
    isRunning = true;
    qDebug() << "CommandWorker started";
    emit showProgress();
    qDebug() << "Emitted showProgress signal";
    processNext();
}

void CommandWorker::processNext() {
    if (taskQueue.isEmpty()) {
        isRunning = false;
        emit allTasksFinished();
        return;
    }
    QString cmd = taskQueue.dequeue();
    proc = new QProcess(); // 这里才可以安全创建
    connect(proc, &QProcess::finished, this, [=](int, QProcess::ExitStatus){
        QString output = QString::fromLocal8Bit(proc->readAllStandardOutput());
        emit taskFinished(output);
        proc->deleteLater();
        processNext(); // 递归处理下一个
    });
    proc->start("cmd.exe", QStringList() << "/c" << cmd);
}


//自定义控件类
CustomButton::CustomButton(const QString &text, const QString &iconPath,QString executePath, QWidget *parent):QWidget(parent)
{
    // 设置按钮样式
    QString buttonStyle = "QPushButton {text-align: center;padding-top: 0px;padding: 5px;border-radius:6px;border:1px groove gray;border-style:outset;}\n\
                    QPushButton:hover {background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,stop: 0 #ffffff, stop: 1 #aaaaaa);}\n\
                    QPushButton:pressed {border-style: inset;}";
    // 按钮
    qbutton = new QPushButton(this);
    QPixmap butPixmap(iconPath);
    QPixmap rounded(butPixmap.size());
    rounded.fill(Qt::transparent);
    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    // 圆角遮罩
    QPainterPath path;
    path.addRoundedRect(butPixmap.rect(), 18, 18); // 圆角半径8
    painter.setClipPath(path);
    // 绘制图标
    painter.drawPixmap(0, 0, butPixmap);
    // 半透明渐变
    QLinearGradient gradient(0, 0, 0, butPixmap.height());
    gradient.setColorAt(0, QColor(255, 255, 255, 255));
    gradient.setColorAt(1, QColor(255, 255, 255, 180));
    painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
    painter.fillRect(butPixmap.rect(), gradient);


    painter.end();

    qbutton->setIcon(QIcon(rounded));
    qbutton->setIconSize(QSize(44, 44)); // 图标大小
    qbutton->setFixedSize(48, 48); // 按钮大小
    qbutton->setFlat(true); // 去除边框
    qbutton->setStyleSheet(buttonStyle);
    qbutton->move(1, 0); // 按钮位置
    // 阴影
    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(8);
    shadow->setOffset(2);
    shadow->setColor(QColor(0, 0, 0, 160));
    qbutton->setGraphicsEffect(shadow);
    // 标签
    qlabel = new QLabel(this);
    qlabel->setText(text);
    qlabel->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    qlabel->setStyleSheet("color: black;");
    qlabel->setFixedSize(56, 20); // 标签大小
    qlabel->move(0, 56); // 标签位置
    // 阴影
    QGraphicsDropShadowEffect *shadow2 = new QGraphicsDropShadowEffect(this);
    shadow2->setBlurRadius(6);
    shadow2->setOffset(1);
    shadow2->setColor(QColor(0, 0, 0, 120));
    qlabel->setGraphicsEffect(shadow2);

    //qlabel->setWordWrap(true); // 自动换行
    setFixedSize(66, 76); // 控件大小   

    this->executePath = executePath;
    softName = text;
    connect(qbutton, &QPushButton::clicked, this, &CustomButton::startSoftware);

}
bool CustomButton::startSoftware()
{
    qDebug() << "准备启动软件:" << softName << "路径:" << executePath;
    qDebug() << "启动参数:" << dccArgs;
    if (executePath.isEmpty()) {
        qDebug() << "执行路径为空，无法启动软件。";
        return false;
    }
    bool res=process->startDetached (executePath,dccArgs);
    if (res){
        qDebug() << "启动软件:" << executePath;
    }
    else {
        qDebug() << "启动软件失败:" << executePath;
    }

    return true;
}

SettingWindow::SettingWindow(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Settings");
    setFixedSize(400, 300);

    // 成员变量而不是局部变量
    usernameEdit = new QLineEdit(this);
    usernameEdit->setGeometry(150, 50, 200, 30);

    passwordEdit = new QLineEdit(this);
    passwordEdit->setGeometry(150, 100, 200, 30);
    passwordEdit->setEchoMode(QLineEdit::Password); // 密码模式

    QLabel *usernameLabel = new QLabel("Username:", this);
    usernameLabel->setGeometry(50, 50, 100, 30);
    QLabel *passwordLabel = new QLabel("Password:", this);
    passwordLabel->setGeometry(50, 100, 100, 30);

    // 确认按钮,取消按钮
    QPushButton *okButton = new QPushButton("OK", this);
    okButton->setGeometry(100, 200, 80, 30);
    QPushButton *cancelButton = new QPushButton("Cancel", this);
    cancelButton->setGeometry(220, 200, 80, 30);

    // 将按钮连接到对话框的 accept/reject，从而 exec() 返回 QDialog::Accepted / Rejected
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    // 可选：回车触发 OK
    connect(usernameEdit, &QLineEdit::returnPressed, okButton, &QPushButton::click);
    connect(passwordEdit, &QLineEdit::returnPressed, okButton, &QPushButton::click);
}
QString SettingWindow::svnUserName() const { return usernameEdit ? usernameEdit->text() : QString(); }
QString SettingWindow::svnPassword() const { return passwordEdit ? passwordEdit->text() : QString(); }
