#include "signalplotter.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    SignalPlotter w;
    w.show();

    return a.exec();
}
