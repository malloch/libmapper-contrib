#ifndef SIGNALPLOTTER_H
#define SIGNALPLOTTER_H

#include <QMainWindow>
#include <qcustomplot.h>

#include <mapper/mapper_cpp.h>
#include <mapper/mapper.h>

#define MAX_LIST 256

// function prototypes
void mapHandler(mapper::Map map, mpr_graph_evt e);

void signalHandler(mapper::Signal sig, mpr_sig_evt evt, mpr_id inst, int len, mpr_type type, const void *value, mpr_time *time);

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
    mapper::Graph *graph;
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
