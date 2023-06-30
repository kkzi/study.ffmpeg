#include "FfplayWidget.h"
#include <iostream>
#include <fstream>
#include <QApplication>

int main(int argc, char** argv)
{
    QApplication a(argc, argv);

    FfplayWidget player;
    //player.open("C:/Users/kizi/desktop/v1.mp4");
    player.open("rtp://127.0.0.1:1234");
    player.resize(800, 600);
    player.show();

    a.exec();
}
