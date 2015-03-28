#include "signalplotter.h"
#include "ui_signalplotter.h"

void signalHandler(mapper_signal sig, mapper_db_signal props, int instance_id,
                   void *value, int count, mapper_timetag_t *timetag)
{
    signalPlot* s = (signalPlot*) props->user_data;
    if (s && value) {
        double dtime = mapper_timetag_get_double(*timetag);
        double dval = ((double*)value)[0];
        s->plotter->graph(s->index)->addData(dtime, dval);
        s->plotter->graph(s->index)->removeDataBefore(dtime-8);
    }
}

SignalPlotter::SignalPlotter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SignalPlotter),
    device("SignalPlotter"),
    monitor()
{
    ui->setupUi(this);
    setGeometry(400, 250, 542, 390);

    ui->customPlot->xAxis->setTickLabelType(QCPAxis::ltDateTime);
    ui->customPlot->xAxis->setDateTimeFormat("hh:mm:ss");
    ui->customPlot->xAxis->setAutoTickStep(false);
    ui->customPlot->xAxis->setTickStep(2);
    ui->customPlot->axisRect()->setupFullAxesBox();

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->customPlot->yAxis2, SLOT(setRange(QCPRange)));

    // add a mapper::Signal to start
    // TODO: signals should be added/removed dynamically
    ui->customPlot->addGraph();
    signalPlot* plot = new signalPlot;
    plot->index = 0;
    plot->plotter = ui->customPlot;
    plots << plot;
    device.add_input("plotme", 1, 'd', 0, 0, 0, signalHandler, plot);

    // setup a timer that repeatedly calls MainWindow::realtimeDataSlot:
    connect(&dataTimer, SIGNAL(timeout()), this, SLOT(realtimeDataSlot()));
    dataTimer.start(0); // Interval 0 means to refresh as fast as possible
}

SignalPlotter::~SignalPlotter()
{
    delete ui;
}

void SignalPlotter::realtimeDataSlot()
{
    device.poll(10);
    foreach (signalPlot* plot, plots) {
        ui->customPlot->graph(plot->index)->rescaleValueAxis();
    }

    double now = device.now();

    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customPlot->xAxis->setRange(now+0.25, 8, Qt::AlignRight);
    ui->customPlot->replot();
}
