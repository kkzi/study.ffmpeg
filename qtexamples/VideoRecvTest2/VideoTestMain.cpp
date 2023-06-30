#include "MainWin.h"
#include <QApplication>
#include <Windows.h>

int main(int argc, char **argv)
{
    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    QApplication a(argc, argv);

    MainWin w;
    w.show();

    auto ret = a.exec();

    CoUninitialize();
    return ret;
}
