#ifndef SIGNALPLOTTER_H
#define SIGNALPLOTTER_H

#include <QMainWindow>
#include <qcustomplot.h>

#include <mapper/mapper_cpp.h>
#include <mapper/mapper.h>

#define MAX_LIST 256

// function prototypes
void mapHandler(mapper_device dev, mapper_map map, mapper_record_event e);

void signalHandler(mapper_signal sig, mapper_id instance, const void *value,
                   int count, mapper_timetag_t *timetag);

namespace Ui {
class SignalPlotter;
}

class SignalPlot
{
public:
    QCustomPlot *plotter;
    int index;
    double average;
};

typedef struct _SignalPlotterData {
    QList<SignalPlot *> plots;
    mapper::Device *device;
    mapper::Database *database;
    Ui::SignalPlotter *ui;
} SignalPlotterData;

class SignalPlotter : public QMainWindow
{
    Q_OBJECT

public:
    explicit SignalPlotter(QWidget *parent = 0);
    ~SignalPlotter();

private Q_SLOTS:
  void realtimeDataSlot();

private:
    Ui::SignalPlotter *ui;
    SignalPlotterData data;
    QTimer dataTimer;
};

#endif // SIGNALPLOTTER_H
