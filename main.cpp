#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QApplication::setApplicationName("YoloStudio");
    QApplication::setOrganizationName("YoloStudio");

    MainWindow w;
    w.show();
    return a.exec();
}
