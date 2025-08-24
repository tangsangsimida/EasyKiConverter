#include <QApplication>
#include "gui/MainWindow.h"
#include <QFile>
#include <QTextStream>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("EasyKiConverter");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("EasyKiConverter");
    
    // 创建并显示主窗口
    MainWindow window;
    window.show();
    
    return app.exec();
}