#include <QCoreApplication>
#include <QDebug>
#include "server.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    qInfo() << "Inspecting Drivers";
    Server* server = new Server ();
    server->start();
    return a.exec();
}