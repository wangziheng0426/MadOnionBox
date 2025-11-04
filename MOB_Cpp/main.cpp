#include <QApplication>
#include "MadOnionBox.h"
#include <QSharedMemory>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    static QSharedMemory sharedMem;
    sharedMem.setKey("MyUniqueAppKey"); // 换成你的唯一标识

    if (!sharedMem.create(1)) {
        QMessageBox::warning(nullptr, "提示", "程序已在运行中！");
        return 0;
    }

    // 创建 MadOnionBox 实例
    MadOnionBox window;
    window.setWindowTitle("MadOnionBox"); // 设置窗口标题
    // 显示窗口
    window.show();

    return a.exec();
}