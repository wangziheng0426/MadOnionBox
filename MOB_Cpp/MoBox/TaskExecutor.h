#ifndef TASKEXECUTOR_H
#define TASKEXECUTOR_H

#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QThread>

#include <functional>

// 单个后台任务的执行结果（在工作线程中填写，再通过信号回到主线程）
struct TaskResult {
    bool success = false; // 任务是否成功
    QString message;      // 提示信息或错误详情
};

// 可入队的任务：在工作线程调用，返回 TaskResult；内部不要操作 QWidget
using TaskFunc = std::function<TaskResult()>;

// 通用串行任务执行器（单例）。
// - 构造时创建一条长寿命工作线程，运行期间复用，不按任务销毁
// - enqueue 入队；同一时间只跑一个任务
// - UI 更新请连接 taskFinished / allFinished
class TaskExecutor : public QObject
{
    Q_OBJECT

public:
    static TaskExecutor *instance();
    void enqueue(TaskFunc task);
    bool isRunning() const;

signals:
    void taskFinished(bool success, const QString &message);
    void allFinished();

private:
    explicit TaskExecutor(QObject *parent = nullptr);
    ~TaskExecutor() override;

    void pump();
    void runTaskOnWorker(TaskFunc task); // 仅在工作线程调用

    QThread workerThread_;               // 长寿命线程，不按任务销毁
    QObject *workerContext_ = nullptr;   // 活在 workerThread_ 上，用于投递任务
    QQueue<TaskFunc> queue_;
    mutable QMutex mutex_;
    bool running_ = false;
};

#endif // TASKEXECUTOR_H
