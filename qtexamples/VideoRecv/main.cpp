
#include "MainWin.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <combaseapi.h>
#include <cstdlib>
#include <fmt/format.h>
#include <libavutil/log.h>
#include <simple/str.hpp>
#include <simple/use_spdlog.hpp>

int main(int argc, char **argv)
{
    init_logger("TestVideoRecv.log");
    LOG_INFO("start");

    av_log_set_callback([](void *ctx, int level, const char *format, va_list args) {
        char buffer[1024]{ 0 };
        vsnprintf(buffer, 1024, format, args);
        std::string line(buffer);
        str::trim(line);
        if (level <= AV_LOG_ERROR)
            LOG_ERROR(line);
        else if (level <= AV_LOG_WARNING)
            LOG_WARN(line);
        else if (level <= AV_LOG_INFO)
            LOG_INFO(line);
        else
            LOG_DEBUG(line);
    });

    // HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    HRESULT hr = CoInitializeEx(0, COINIT_APARTMENTTHREADED);
    QApplication a(argc, argv);
    a.setStyle("fusion");
    QString test(argv[0]);
    a.setWindowIcon(QIcon{ ":/logo.ico" });

    av_log_set_level(AV_LOG_WARNING);

    MainWin w;
    w.show();

    auto ret = a.exec();
    CoUninitialize();
    return ret;
}
