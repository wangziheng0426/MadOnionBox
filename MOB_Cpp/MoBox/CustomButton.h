#ifndef CUSTOMBUTTON_H
#define CUSTOMBUTTON_H

#include <QLabel>
#include <QMap>
#include <QProcess>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QWidget>

// 自定义启动按钮：图标 + 下方软件名；启动逻辑内聚在按钮内。
// MadOnionBox 负责准备数据并 set* 进来，点击后由按钮完成：预脚本 → 环境变量 → 带参启动。
class CustomButton : public QWidget
{
    Q_OBJECT

public:
    explicit CustomButton(QWidget *parent = nullptr);
    CustomButton(const QString &softName, const QString &iconPath, const QString &executePath,
                 QWidget *parent = nullptr);

    // ---- MadOnionBox 注入配置 ----
    void setSoftName(const QString &name);
    void setVersion(const QString &version);
    void setIconPath(const QString &iconPath);
    void setExecutePath(const QString &path);
    void setArguments(const QStringList &args);
    void setEnvironment(const QMap<QString, QString> &env); // 覆盖/追加环境变量
    void setPrePythonScript(const QString &scriptPath);     // 启动前脚本，空则跳过
    void setPrePythonArgs(const QStringList &args);
    void setPythonExePath(const QString &pythonExe);        // 默认用 exe 旁 python_embed

    QString softName() const { return softName_; }
    QString executePath() const { return executePath_; }

    // 完整启动流程（也可被外部主动调用）
    bool startSoftware();

signals:
    void launchSucceeded(const QString &softName);
    void launchFailed(const QString &softName, const QString &reason);

private:
    void setupUi();
    void applyIcon(const QString &iconPath);
    QString resolvePythonExe() const;
    bool runPrePythonScript(QString *errorMessage);
    bool launchTarget(QString *errorMessage);
    static QProcessEnvironment buildEnvironment(const QMap<QString, QString> &extra);

    QPushButton *qbutton = nullptr;
    QLabel *qlabel = nullptr;

    QString softName_;
    QString version_;
    QString iconPath_;
    QString executePath_;
    QStringList arguments_;
    QMap<QString, QString> environment_;
    QString prePythonScript_;
    QStringList prePythonArgs_;
    QString pythonExePath_;
};

#endif // CUSTOMBUTTON_H
