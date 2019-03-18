# Bosch-IoT-Showcase

## Overview
- The XDK 110 is a programmable sensor. See details here: http://xdk.bosch-connectivity.com/. "I’m a programmable sensor device & a prototyping platform for any IoT use case you can imagine."
- We have developed a client (the Solace MQTT XDK App) which connects via MQTT to the event mesh.
- with our partners we have created a few user / business applications:
  - **SL Corporation:RTView**, a real-time dashboard for visualizing the XDK sensor data (https://sl.com/products/rtview-cloud-for-iot/)
  - **Datawatch:Panopticon**, a streaming analytics tool to analyze the XDK sensor data in real-time (https://www.datawatch.com/our-platform/panopticon-streaming-analytics/)
  - **Dell Boomi:Flow**, a web-based user application to command & control the XDK App (https://boomi.com/platform/flow/)
  - **Solace:Solace Cloud**, to provide the event mesh (https://solace.com/cloud/)

For more details visit the Wiki page on this site.

## What you need
Here is what you need before you can get started:
- Bosch XDK 110 + Micro SD Card + SD Card Adapter
> - go to https://xdk.bosch-connectivity.com/buy-xdk to find a retailer.
> - XDK110
> - Micro SD card 32GB (not more)
> - SD Card Adapter to write / edit the config file onto the SD card from your computer
- XDK Workbench, version 3.5.x
> - once your XDK has arrived:
>   - register on https://xdk.bosch-connectivity.com/
>   - download and install the XDK Workbench (you need the serial number of your XDK110 device)
- A MQTT test client
> - for example, MQTT Box from http://workswithweb.com/mqttbox.html

## Install the Solace MQTT XDK App on the XDK110

### Clone the repository to your local system
This will create the following structure:

![project_file_structure](doc/images/project_file_structure.png?raw=true "Project file structure")

### Patch the XDK SDK
Go to the install directory of the XDK Workbench. On a Mac it is here:

![xdk_workbench_install_dir_mac](doc/images/xdk_workbench_install_dir_mac.png?raw=true "xdk_workbench_install_dir_mac")

1. go into the folder (mac: right-click, show package contents)
2. go to: ````Contents/Eclipse/SDK/xdk110/Common/include/Connectivity````
  replace XDK_MQTT.h with the version from the repository
3. go to: ````Contents/Eclipse/SDK/xdk110/Common/source/Connectivity````
replace MQTT.c with the version from the repository

Note: this adds the MQTT user/pwd authentication API to the SDK.

### Compile the SolaceBoschXDKApp

Open the XDK Workbench and open the project:

![open_project](doc/images/open_project.png?raw=true "open_project")

Click 'Directory ...' and select the directory with the XDK Workbench Eclipse Project
![import_project](doc/images/import_project.png?raw=true "import_project")

!!! missing .cproject and .project 


## View the sensor data

## View the Solace event mesh

## Interact with the XDK110
- using the Boomi Flow application (http://boomi.to/solace)
- details in the Wiki

## Analyze the sensor data
