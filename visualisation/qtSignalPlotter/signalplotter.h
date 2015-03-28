#ifndef SIGNALPLOTTER_H
#define SIGNALPLOTTER_H

#include <QMainWindow>
#include <qcustomplot.h>

#include <mapper/mapper_cpp.h>

namespace Ui {
class SignalPlotter;
}

class signalPlot
{
public:
    QCustomPlot *plotter;
    int index;
    double average;
};

class SignalPlotter : public QMainWindow
{
    Q_OBJECT

public:
    explicit SignalPlotter(QWidget *parent = 0);
    ~SignalPlotter();

private slots:
  void realtimeDataSlot();

private:
    Ui::SignalPlotter *ui;
    QList<signalPlot *> plots;
    QTimer dataTimer;
    mapper::Device device;
    mapper::Monitor monitor;
};

void signalHandler(mapper_signal sig, mapper_db_signal props, int instance_id,
                   void *value, int count, mapper_timetag_t *timetag);

#endif // SIGNALPLOTTER_H
