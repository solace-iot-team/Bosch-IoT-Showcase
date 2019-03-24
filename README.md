# Bosch-IoT-Showcase

## Overview
- The XDK 110 is a programmable sensor. See details here: http://xdk.bosch-connectivity.com/. "I’m a programmable sensor device & a prototyping platform for any IoT use case you can imagine."
- We have developed a client (the **_Solace Bosch XDK110 App_**) which connects via MQTT to the event mesh.
- With our partners we have created a few user / business applications:
  - **SL Corporation:RTView**, a real-time dashboard for visualizing the XDK sensor data (https://sl.com/products/rtview-cloud-for-iot/) and a live visual representation of the event mesh
  - **Datawatch:Panopticon**, a streaming analytics tool to analyze the XDK sensor data in real-time (https://www.datawatch.com/our-platform/panopticon-streaming-analytics/)
  - **Dell Boomi:Flow**, a web-based user application to command & control the XDK App (https://boomi.com/platform/flow/)
  - **Solace:Solace Cloud**, to provide the event mesh (https://solace.com/cloud/)

For more details visit the [Wiki page](https://github.com/solace-iot-team/Bosch-IoT-Showcase/wiki) on this site.

## What you need before getting started

Here is what you need before you can get started:
- Bosch XDK 110 + Micro SD Card + SD Card Adapter
> - Go to https://xdk.bosch-connectivity.com/buy-xdk to find a retailer.
  >   - XDK110
  >   - Micro SD card 32GB (not more)
  >   - SD Card Adapter to write / edit the config file onto the SD card from your computer
- XDK Workbench, version 3.5.x
> - Once your XDK has arrived:
>   - register on https://xdk.bosch-connectivity.com/
>   - download and install the XDK Workbench (you need the serial number of your XDK110 device)
- A MQTT test client
> - For example, MQTT Box from http://workswithweb.com/mqttbox.html

## Installing the Solace Bosch XDK110 Application

[Go to the src directory](https://github.com/solace-iot-team/Bosch-IoT-Showcase/tree/master/src) for details on how to install the **_Solace Bosch XDK110 App_** on the Bosch XDK110.

## What's next

### View the Sensor Data

Using the **_SL RTView Event Dashboard_**, you can visualize the real-time sensor data the XDK sends periodically.

_Note: if your device Id has not been registered yet, select **All Devices** and search for the device Id._

Device IoT Data Visualization: http://bit.ly/2YdArPv
(the URL is subject to change. let us know if it does not work any more ...)

### View the Hybrid IoT Event Mesh

Using the **_SL RTView Monitorig Dashboard_**, you can visualize the Solace Event Mesh.

Hybrid IoT Event Mesh Visualization: http://bit.ly/2TeSC3q
(the URL is subject to change. let us know if it does not work any more ...)

### Interact with the XDK110

Using the **_Boomi Flow Command & Control Application_**, you can interact with the XDK device.

http://boomi.to/solace


### Analyze the Sensor Data

coming soon ...

(on Datawatch Panopticon)


----------
The End.
