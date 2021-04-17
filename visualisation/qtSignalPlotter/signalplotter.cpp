#include "signalplotter.h"
#include "ui_signalplotter.h"

using namespace mapper;
void signalHandler(Signal&& sig, Signal::Event evt, mpr_id inst, int len, Type type,
                   const void *value, Time&& time)
{
    SignalPlot* s = (SignalPlot*) sig[Property::DATA];
    if (s && value) {
        double dtime = time;
        double dval = ((double*)value)[0];
        s->plotter->graph(s->index)->addData(dtime, dval);
        s->plotter->graph(s->index)->removeDataBefore(dtime-8);
    }
}

void mapHandler(Graph&& graph, Map&& map, Graph::Event evt)
{
    // if mapped involves current generic signal, create a new generic signal
    SignalPlotterData *data = (SignalPlotterData*)graph[MPR_PROP_DATA];
    if (!data) {
        printf("error in map handler: user_data is NULL");
        return;
    }
    if (!data->device->ready()) {
        printf("error in map handler: device not ready");
        return;
    }

    // check if we are the source, if so delete map
    for (Signal s : map.signals(Map::Location::SRC)) {
        if (s[MPR_PROP_IS_LOCAL]) {
            printf("SignalPlotter signals cannot be the source of a map.");
            map.release();
            return;
        }
    }

    switch (evt) {
    case Graph::Event::OBJ_NEW: {
        // unmap the generic signal
        map.release();

        if (data->device->num_sigs(MPR_DIR_IN) >= MAX_LIST) {
            printf("Max inputs reached!");
            // unmap the generic signal
            map.release();
            return;
        }

        // create a matching input signal
        mapper::Signal sig = 0;
        mapper::Signal src = map.signals(mapper::Map::Location::SRC)[0];
        int length = slot.signal().length();
        float min[length], max[length];
        if (slot.minimum() && slot.maximum()) {
            switch (slot.signal().type()) {
            case 'f': {
                sig = data->device->add_input_signal(slot.signal().name(),
                                                     length, Type::FLOAT, 0,
                                                     slot.minimum(),
                                                     slot.maximum(),
                                                     signalHandler, 0);
                break;
            }
            case 'i': {
                int *src_mini = (int*)slot.minimum();
                int *src_maxi = (int*)slot.maximum();
                for (int i = 0; i < length; i++) {
                    min[i] = (float)src_mini[i];
                    max[i] = (float)src_maxi[i];
                }
                sig = data->device->add_signal(Direction.IN, slot.signal().name(),
                                               length, Type::FLOAT, 0, min, max,
                                               signalHandler, 0);
                break;
            }
            case 'd': {
                double *src_mind = (double*)slot.minimum();
                double *src_maxd = (double*)slot.maximum();
                for (int i = 0; i < length; i++) {
                    min[i] = (float)src_mind[i];
                    max[i] = (float)src_maxd[i];
                }
                sig = data->device->add_signal(Direction.IN, slot.signal().name(),
                                               length, Type::FLOAT, 0, min, max,
                                               signalHandler, 0);
                break;
            }
            default:
                printf("problem with connection src_type.");
                return;
            }
        }
        else {
            for (int i = 0; i < length; i++) {
                min[i] = 0.f;
                max[i] = 1.f;
            }
            sig = data->device->add_signal(slot.signal().name(),
                                                 length, 'f', 0, min, max,
                                                 signalHandler, 0);
        }
        if (!sig)
            return;

        // add a corresponding plot
        data->ui->customPlot->addGraph();
        SignalPlot* plot = new SignalPlot;
        plot->index = 0;
        plot->plotter = data->ui->customPlot;
        data->plots << plot;
        sig[Property::DATA] = &plot;
        sig.set_callback(signalHandler);

        // connect the new signal
        mapper::Map newmap(slot.signal(), sig);
        newmap[Property::EXPR] = "y=x";
        newmap.push();
        break;
    }
    case mapper::Graph::Event::OBJ_REM: {
        // check if we are the destination
        mapper::Signal dst = map.signals(Map::Location::DST)[0];
        String dst_name = dst[Property::NAME];
        if (dst_name.compare("CONNECT_HERE") == 0)
            return;
        if (dst_name.compare(map.signals(Map::Location::SRC)[0][Property::NAME]) != 0)
            return;
        // remove corresponding signal
        data->device->remove_signal(dst);
        break;
    }
    default:
        break;
    }
}

SignalPlotter::SignalPlotter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SignalPlotter)
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

    data.ui = ui;
    data.device = new mapper::Device("SignalPlotter");
    data.graph = new mapper::Graph();
    data.graph[MPR_PROP_DATA] = data;

    // add a map handler to the device
    data.graph->add_callback(mapHandler, Type::MAP);

    // add a dummy input mapper::Signal to start
    data.device->add_signal(Direction::IN, "plotme", 1, Type::DOUBLE);

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
    data.device->poll(10);
    for (auto const& plot : data.plots) {
        ui->customPlot->graph(plot->index)->rescaleValueAxis();
    }

    mapper::Time now = mapper::Time();

    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customPlot->xAxis->setRange((double)now+0.25, 8, Qt::AlignRight);
    ui->customPlot->replot();
}
