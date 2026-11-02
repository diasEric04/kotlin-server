#include <QCoreApplication>
#include <QDebug>
#include "MobileServer.hpp"
#include "ApiServer.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    qInfo() << "Inspecting Drivers";
    
    MobileServer* mobileServer = new MobileServer (5000);
    mobileServer->start();

    ApiServer* apiServer = new ApiServer (5001);
    apiServer->start();

    return a.exec();
}