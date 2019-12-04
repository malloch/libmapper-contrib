#include "signalplotter.h"
#include "ui_signalplotter.h"

void signalHandler(mapper_signal _sig, mapper_id instance,
                   const void *value, int count, mapper_timetag_t *timetag)
{
    mapper::Signal sig(_sig);
    SignalPlot* s = (SignalPlot*) sig.user_data();
    if (s && value) {
        double dtime = mapper_timetag_double(*timetag);
        double dval = ((double*)value)[0];
        s->plotter->graph(s->index)->addData(dtime, dval);
        s->plotter->graph(s->index)->removeDataBefore(dtime-8);
    }
}

void mapHandler(mapper_device dev, mapper_map _map, mapper_record_event e)
{
    mapper::Map map(_map);
    // if mapped involves current generic signal, create a new generic signal
    SignalPlotterData *data = (SignalPlotterData*)mapper_device_user_data(dev);
    if (!data) {
        printf("error in map handler: user_data is NULL");
        return;
    }
    if (!data->device->ready()) {
        printf("error in map handler: device not ready");
        return;
    }

    // check if we are the source, if so delete map
    for (int i = 0; i < map.num_slots(MAPPER_LOC_SOURCE); i++) {
        if (map.source(i).signal().device() == data->device) {
            printf("SignalPlotter signals cannot be the source of a map.");
            map.release();
            return;
        }
    }

    switch (e) {
    case MAPPER_ADDED: {
        // unmap the generic signal
        map.release();

        if (data->device->num_signals(MAPPER_DIR_INCOMING) >= MAX_LIST) {
            printf("Max inputs reached!");
            // unmap the generic signal
            map.release();
            return;
        }

        // create a matching input signal
        mapper::Signal sig = 0;
        mapper::Map::Slot slot = map.source(0);
        int length = slot.signal().length();
        float min[length], max[length];
        if (slot.minimum() && slot.maximum()) {
            switch (slot.signal().type()) {
            case 'f': {
                sig = data->device->add_input_signal(slot.signal().name(),
                                                     length, 'f', 0,
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
                sig = data->device->add_input_signal(slot.signal().name(),
                                                     length, 'f', 0, min, max,
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
                sig = data->device->add_input_signal(slot.signal().name(),
                                                     length, 'f', 0, min, max,
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
            sig = data->device->add_input_signal(slot.signal().name(),
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
        sig.set_user_data(&plot);
        sig.set_callback(&signalHandler);

        // connect the new signal
        mapper::Map newmap(slot.signal(), sig);
        newmap.set_mode(MAPPER_MODE_EXPRESSION);
        newmap.set_expression("y=x");
        newmap.push();
        break;
    }
    case MAPPER_REMOVED: {
        // check if we are the destination
        mapper::Signal dst = map.destination().signal();
        if (dst.name().compare("CONNECT_HERE") == 0)
            return;
        if (dst.name().compare(map.source(0).signal().name()) != 0)
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
    data.device->set_user_data(&data);
    data.database = new mapper::Database();

    // add a connection handler to the device
    data.device->set_map_callback(mapHandler);

    // add a dummy input mapper::Signal to start
    data.device->add_input_signal("plotme", 1, 'd', 0, 0, 0, 0, 0);

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

    mapper::Timetag now = mapper::Timetag();

    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customPlot->xAxis->setRange((double)now+0.25, 8, Qt::AlignRight);
    ui->customPlot->replot();
}
