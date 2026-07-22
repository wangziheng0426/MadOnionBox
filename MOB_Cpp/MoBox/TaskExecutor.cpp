#include "TaskExecutor.h"

#include <QCoreApplication>

TaskExecutor *TaskExecutor::instance()
{
    // 挂到 QCoreApplication，随应用有序退出，避免静态单例晚于 Qt 析构
    static TaskExecutor *executor = nullptr;
    if (!executor) {
        executor = new TaskExecutor(QCoreApplication::instance());
    }
    return executor;
}

TaskExecutor::TaskExecutor(QObject *parent)
    : QObject(parent)
{
    workerContext_ = new QObject();
    workerContext_->moveToThread(&workerThread_);
    // 线程真正结束时再删 context（正常运行期间线程不 quit）
    connect(&workerThread_, &QThread::finished, workerContext_, &QObject::deleteLater);
    workerThread_.start();
}

TaskExecutor::~TaskExecutor()
{
    // 仅应用退出时走到这里：先停线程再结束
    workerThread_.quit();
    workerThread_.wait(10000);
}

bool TaskExecutor::isRunning() const
{
    QMutexLocker lock(&mutex_);
    return running_;
}

void TaskExecutor::enqueue(TaskFunc task)
{
    if (!task) {
        return;
    }
    {
        QMutexLocker lock(&mutex_);
        queue_.enqueue(std::move(task));
    }
    pump();
}

void TaskExecutor::pump()
{
    TaskFunc task;
    {
        QMutexLocker lock(&mutex_);
        if (running_ || queue_.isEmpty()) {
            return;
        }
        running_ = true;
        task = queue_.dequeue();
    }

    // 投递到已启动的工作线程，复用同一条线程
    if (!workerContext_) {
        QMutexLocker lock(&mutex_);
        running_ = false;
        return;
    }

    QMetaObject::invokeMethod(
        workerContext_,
        [this, task = std::move(task)]() mutable { runTaskOnWorker(std::move(task)); },
        Qt::QueuedConnection);
}

void TaskExecutor::runTaskOnWorker(TaskFunc task)
{
    TaskResult result;
    try {
        result = task();
    } catch (...) {
        result = {false, QStringLiteral("任务执行异常")};
    }

    // 回到 GUI 线程发信号并调度下一任务；工作线程本身继续存活
    QMetaObject::invokeMethod(
        this,
        [this, result]() {
            {
                QMutexLocker lock(&mutex_);
                running_ = false;
            }
            emit taskFinished(result.success, result.message);

            bool hasMore = false;
            {
                QMutexLocker lock(&mutex_);
                hasMore = !queue_.isEmpty();
            }
            if (!hasMore) {
                emit allFinished();
            } else {
                pump();
            }
        },
        Qt::QueuedConnection);
}
