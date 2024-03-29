

#include <glog/logging.h>

int main(int argc, char **argv)
{
    google::InitGoogleLogging(argv[0]);

    FLAGS_log_dir = "./";
    FLAGS_alsologtostderr = 1;
    LOG(INFO) << "hello world";
    // google::ShutdownGoogleLogging();
    while (true)
    {
    }

    return 0;
}
