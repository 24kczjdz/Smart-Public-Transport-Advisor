#include "MainWindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("TransportAdvisor"));
    QApplication::setOrganizationDomain(QStringLiteral("local"));
    MainWindow w;
    w.show();
    return app.exec();
}
