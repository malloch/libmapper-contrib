#include "signalplotter.h"
#include "ui_signalplotter.h"
#include <QDebug>

using namespace mapper;
void signalHandler(Signal&& sig, Signal::Event evt, Id inst, int len, Type type,
                   const void* value, Time&& time)
{
    SignalPlot* s = (SignalPlot*)(void*)sig["plot"];

    if (s && value) {
        double dtime = time;
        double dval = ((float*)value)[0];
        s->plotter->graph(s->index)->addData(dtime, dval);
        s->plotter->graph(s->index)->data()->removeBefore(dtime-8);
    }
    // TODO:
}

void mapHandler(Graph&& graph, Map&& map, Graph::Event evt)
{
    SignalPlotterData *data = static_cast<SignalPlotterData*>((void*)graph["plot"]);

    if (!data) {
        qInfo("error in map handler: user_data is NULL");
        return;
    }
    if (!data->device->ready()) {
        qInfo("error in map handler: device not ready");
        return;
    }

    mapper::Device* d = data->device;
    std::string my_name = (*d).property(Property::NAME);

    if (my_name.empty()) {
        return;
    }

    // check if we are the source, if so delete map
    for (Signal s : map.signals(Map::Location::SRC)) {
        std::string src_name = s.device().property(Property::NAME);
        if (my_name.compare(src_name) == 0) {
            qInfo("SignalPlotter signals cannot be the source of a map.");
            map.release();
            return;
        }
    }

    bool is_local = false;
    for (Signal&& s : map.signals(Map::Location::DST)) {
        std::string dst_name = s.device().property(Property::NAME);
        if (my_name.compare(dst_name) == 0) {
            is_local = true;
            break;
        }
    }

    if (!is_local) {
        qInfo("map is not local, ignoring.");
        return;
    }
    else
        qInfo("map is local, proceeding.");

    mapper::Signal src = map.signals(mapper::Map::Location::SRC)[0];
    std::string src_sig_name = src.property(Property::NAME);
    std::string src_dev_name = src.device().property(Property::NAME);

    mapper::Signal dst = map.signals(Map::Location::DST)[0];
    std::string dst_sig_name = dst[Property::NAME];

    switch (evt) {
    case Graph::Event::OBJ_NEW: {
        qInfo("NEW MAP!\n");
        std::string src_full_name = src_dev_name + "/" + src_sig_name;
        if (dst_sig_name.compare(src_full_name) == 0) {
            return;
        }

        // unmap the generic signal
        map.release();

        if (data->device->signals(Direction::INCOMING).size() >= MAX_LIST) {
            qInfo("Max inputs reached!");
            return;
        }

        qInfo("creating new input signal");
        // create a matching input signal
        mapper::Signal sig = 0;

        int length = src.property(Property::LENGTH);
        int numInst = src.property(Property::NUM_INSTANCES);
        float minArray[length], maxArray[length], *min = 0, *max = 0;
        if (src.property(Property::MIN) && src.property(Property::MAX)) {
            switch ((Type)src.property(Property::TYPE)) {
            case Type::FLOAT: {
                qInfo("FLOAT");
                min = (float*)src.property(Property::MIN);
                max = (float*)src.property(Property::MAX);
                for (int i = 0; i < length; i++)
                    qInfo("min=%p, max=%p", min, max);
                break;
            }
            case Type::INT32: {
                qInfo("INT32");
                int *src_mini = (int*)src.property(Property::MIN);
                int *src_maxi = (int*)src.property(Property::MAX);
                for (int i = 0; i < length; i++) {
                    minArray[i] = (float)src_mini[i];
                    maxArray[i] = (float)src_maxi[i];
                }
                min = minArray;
                max = maxArray;
                break;
            }
            case Type::DOUBLE: {
                double *src_mind = (double*)src.property(Property::MIN);
                double *src_maxd = (double*)src.property(Property::MAX);
                for (int i = 0; i < length; i++) {
                    minArray[i] = (float)src_mind[i];
                    maxArray[i] = (float)src_maxd[i];
                }
                min = minArray;
                max = maxArray;
                break;
            }
            default:
                qInfo("problem with connection src_type.");
                return;
            }
        }

        sig = data->device->add_signal(Direction::INCOMING, src_full_name, length, Type::FLOAT);
//        sig = data->device->add_signal(Direction::INCOMING, (const char*)src[Property::NAME],
//                                       length, Type::FLOAT, 0, min, max, &numInst);
        if (!sig)
            return;

        // add a corresponding plot
        QColor color;
        color.setHsvF(data->plot_index*0.1, 1.0, 1.0, 1.0);
        data->ui->customPlot->addGraph()->setPen(QPen(QBrush(color), 2));
        SignalPlot* plot = new SignalPlot;
        plot->index = data->plot_index;
        data->plot_index += 1;
        plot->plotter = data->ui->customPlot;
        data->plots << plot;
        sig.set_property("plot", (void*)plot);
        sig.set_callback(signalHandler);

        // connect the new signal
        mapper::Map newmap(src, sig);

        newmap.set_property(Property::EXPRESSION, "y=linear(x,?,?,0,5)");
        newmap[Property::PROCESS_LOCATION] = Map::Location::DST;
        newmap.push();
        break;
    }
    case mapper::Graph::Event::OBJ_REM: {
        qInfo("map removed");
        std::string src_full_name = src_dev_name + "/" + src_sig_name;
        if (dst_sig_name.compare(src_full_name) != 0) {
            return;
        }
        // remove corresponding signal. We need to look it up by name since dst is the graph's copy of the signal
        Signal loc = data->device->signals().filter(Property::NAME, dst_sig_name, Operator::EQUAL);
        data->device->remove_signal(loc);

        // remove plot/graph etc.
        break;
    }
    default:
        qInfo("Unhandled event type in graph handler");
        break;
    }
}

SignalPlotter::SignalPlotter(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::SignalPlotter),
    device("SignalPlotter"),
    graph(Type::OBJECT)
{
    ui->setupUi(this);
    setGeometry(400, 250, 542, 390);

    ui->customPlot->axisRect()->setupFullAxesBox();
    QBrush brush;
    brush.setColor(Qt::darkGray);
    ui->customPlot->setBackground(brush);

    // make left and bottom axes transfer their ranges to right and top axes:
    connect(ui->customPlot->xAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->customPlot->xAxis2, SLOT(setRange(QCPRange)));
    connect(ui->customPlot->yAxis, SIGNAL(rangeChanged(QCPRange)),
            ui->customPlot->yAxis2, SLOT(setRange(QCPRange)));

    data.plot_index = 0;
    data.ui = ui;
    data.device = &device;
    // TODO: use device graph instead
    data.graph = &graph;
//    mapper::Graph graph = device.graph();
    graph.set_property("plot", (void*)&data);
    qInfo("added user_data %p\n", &data);
    qInfo("  with device %p\n", &data.device);

//    New plan: device-local graphs should only call handlers for objects that relate to local stuff
//              e.g. when a local map is created...

    // add a map handler to the device
    graph.add_callback(mapHandler, Type::OBJECT);
    graph.subscribe(Type::OBJECT);

    // add a dummy input mapper::Signal to start
    device.add_signal(Direction::INCOMING, "plotme", 1, Type::FLOAT);

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
    graph.poll(10);
    for (auto const& plot : plots) {
        ui->customPlot->graph(plot->index)->rescaleValueAxis();
    }

    mapper::Time time;
    time.now();

    // make key axis range scroll with the data (at a constant range size of 8):
    ui->customPlot->xAxis->setRange((double)time + 0.25, 8, Qt::AlignRight);
    ui->customPlot->replot();
}
