# pySignalPlotter

pySignalPlotter is a utility for plotting the live values of signals. To launch the program from the command line, simply type:

~~~
$ python ./pySignalPlotter.py
~~~

While running, pySignalPlotter will publish a dummy input signal to the libmapper distributed graph. Connecting a map to this signal will cause the creation of a new signal that matches the properties of the source (name, vector length, number of signal instances) and the map will be automatically re-routed to the new signal. A plot will appear for each connected signal, and vector elements (for vector signals) and instances (for multi-instanced signals) will be drawn on the same plot.

<img src="./pySignalPlotter_screenshot.png">

## Drag and drop 

This interface accepts drag and drop events with the libmapper Mime type.

## Roadmap / Todo list

* enable smooth switching between 2d plotting modes
* add a signal picker for use without a separate session manager?
    * add a '+' button to add plots
