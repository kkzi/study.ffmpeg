#include "MainWin.h"
#include <QApplication>
#include <QDir>
#include <Windows.h>
#include <libavutil/log.h>

int main(int argc, char **argv)
{
    //HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = CoInitializeEx( 0, COINIT_APARTMENTTHREADED );
    QApplication a(argc, argv);
    a.setWindowIcon(QIcon{ ":/logo.ico" });

    MainWin w;
    //w.resize(1400, 900);
    w.show();
    // w.showMaximized();

    auto ret = a.exec();

    CoUninitialize();
    return ret;
}

