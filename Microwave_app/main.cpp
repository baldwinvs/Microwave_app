#include "microwave.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Microwave w;
    w.show();
    return a.exec();
}
