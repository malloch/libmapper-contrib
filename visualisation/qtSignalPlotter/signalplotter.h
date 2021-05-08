#ifndef SIGNALPLOTTER_H
#define SIGNALPLOTTER_H

#include <QMainWindow>
#include <qcustomplot.h>

#include <mapper/mapper_cpp.h>

#define MAX_LIST 256

// function prototypes
void mapHandler(mapper::Map map, mpr_graph_evt e);

void signalHandler(mapper::Signal&& sig, mapper::Signal::Event evt, mapper::Id inst, int len,
                   mapper::Type type, const void *value, mapper::Time&& time);

namespace Ui {
class SignalPlotter;
}

class SignalPlot
{
public:
    QCPGraph* qcpGraph;
    double average;
};

typedef struct _SignalPlotterData {
    QList<SignalPlot *> plots;
    mapper::Device* device;
    mapper::Graph* graph;
    Ui::SignalPlotter *ui;
    int plot_index;
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
    QList<SignalPlot *> plots;
    mapper::Device device;
    mapper::Graph graph;
    SignalPlotterData data;
    QTimer dataTimer;
};

#endif // SIGNALPLOTTER_H
