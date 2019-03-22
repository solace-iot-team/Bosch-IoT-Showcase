# The Solace Bosch-XDK110 Application

## Overview
- The XDK 110 is a programmable sensor. See details here: http://xdk.bosch-connectivity.com/. "I’m a programmable sensor device & a prototyping platform for any IoT use case you can imagine."
- We have developed a client (the **_Solace Bosch XDK App_**) which connects via MQTT to the event mesh.

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

## Install the Solace MQTT XDK App on the XDK110

### Clone the repository to your local system
````
git clone https://github.com/solace-iot-team/Bosch-IoT-Showcase.git
````

This will create the following structure:
<p align="left"><img src="../doc/images/project_file_structure.png" width=500 /></p>

[comment]: #(![](../doc/images/project_file_structure.png?width=300))

### Patch the XDK SDK
Go to the install directory of the XDK Workbench. On a Mac it is here:

<p align="left"><img src="../doc/images/xdk_workbench_install_dir_mac.png" width=500 /></p>

[comment]: #(![](../doc/images/xdk_workbench_install_dir_mac.png))

1. go into the folder (mac: right-click, show package contents)
2. go to: ````Contents/Eclipse/SDK/xdk110/Common/include/Connectivity````
  replace ```` XDK_MQTT.h ```` with the version from the repository
3. go to: ````Contents/Eclipse/SDK/xdk110/Common/source/Connectivity````
replace ```` MQTT.c ```` with the version from the repository

_Note: this adds the MQTT user/pwd authentication API to the SDK._

### Compile the SolaceBoschXDKApp

Open the XDK Workbench and open the project:

<p align="left"><img src="../doc/images/open_project.png" width=500 /></p>

Click 'Directory ...' and select the directory with the XDK Workbench Eclipse Project

<p align="left"><img src="../doc/images/import_project.png" width=600 /></p>

Build the project:

<p align="left"><img src="../doc/images/build_project.png" width=400 /></p>

This creates the binary SolaceBoschXDKApp.bin. Note that it is compiled in debug mode, see the Makefile to change to release mode.

### Copy the Config file

Enter the connection info & passwords in the file:
Bosch-IoT-Showcase/files/config/config.json

<p align="left"><img src="../doc/images/config.png" width=600 /></p>

Change the following:
- "wlanSSID":"the-WIFI-SSID"
- "wlanPSK":"the-password"
- "brokerPassword":"the-solace-cloud-broker-password"

**_Note: Contact ricardo.gomez-ulmke@solace.com or swen-helge.huber@solace.com to get the password for the broker._**

... wait for the response ...

Now copy the ```` config.json ```` to the SD card and insert it into the XDK.

**_Note: be careful when inserting the SD card into the XDK. I just lost one and I don't think you can open it._**

### Flash the App onto the XDK

- Connect your XDK via the USB cable to your computer.
- Switch your device on.
- Refresh the device list.
- Make sure the device is in Bootloader mode and you can see the Flash button.

<p align="left"><img src="../doc/images/xdk_devices_panel.png" width=600 /></p>

- Click anywhere in your project in the Project Explorer (so Eclipse knows which project to flash)
- Click Flash in the XDK Devices panel.
- Check the console output
   - if it says "Invalid application", try the next steps to fix it
   - if it says "Jumping to application", then your device has successfully booted the application

### Dealing with "Invalid Application"
This seems to happen from time to time. Don't give up until it worked.

<p align="left"><img src="../doc/images/flashing_invalid_app.png" width=600 /></p>

Try the following:
- Add the XDK Nature to the project
- Clean project
- Build project
- Flash again...

<p align="left"><img src="../doc/images/add_xdk_nature.png" width=600 /></p>

### Check that the XDK boots

You want to look out for:
````
INFO | XDK DEVICE 1:  Jumping to application
````
Another small irritation of the XDK Workbench:
- refresh the XDK Devices panel ==> changes to Mode: Application
- now click the green link denoted COM in the XDK Devices panel ==> serial port is disconnected
- now click it again ==> serial port is connected again

Now you should see the XDK messages appearing on the console. Similar to this:

<p align="left"><img src="../doc/images/console_log_1.png" width=500 /></p>

### Find & Register your Device Id

At boot time, the app prints out the device id. Look for this:

````
INFO | XDK DEVICE 1: AppControllerEnable: ----------------------------------------
INFO | XDK DEVICE 1: AppControllerEnable: XDK Device Id: <your device id>
INFO | XDK DEVICE 1: AppControllerEnable: ----------------------------------------
````
Make a note of the device Id, choose a nickname and send us the Id + nickname to register them with the 'system'.


### Subscribe to the Metrics Event Stream

- Launch your MQTT test client.
- Connect to the same Solace Cloud broker
- Subscribe to the metrics stream from your device

The topic template is as follows:
````
$create/iot-event/<baseTopic>/<deviceId>/metrics

where baseTopic is defined in the config.json, for example:

	"baseTopic": "BCW/solacebooth/racetrack"

so, the resulting topic would look as follows:

$create/iot-event/BCW/solacebooth/racetrack/<deviceId>/metrics

````

You should receive a message similar to this:

````
[
  {
    "timestamp": "2019-03-19T14:19:01.657Z",
    "deviceId": "24d11f0358cd5d9a",
    "humidity": 30,
    "light": 123840,
    "temperature": 26.061,
    "acceleratorX": -10,
    "acceleratorY": -10,
    "acceleratorZ": 991,
    "gyroX": 732,
    "gyroY": -549,
    "gyroZ": -5612,
    "magR": 6058,
    "magX": -56,
    "magY": 10,
    "magZ": -70
  },
  {
    "timestamp": "2019-03-19T14:19:02.156Z",
    "deviceId": "24d11f0358cd5d9a",
    "humidity": 30,
    "light": 123840,
    "temperature": 26.051,
    "acceleratorX": -10,
    "acceleratorY": -15,
    "acceleratorZ": 989,
    "gyroX": 671,
    "gyroY": -549,
    "gyroZ": -5551,
    "magR": 6058,
    "magX": -55,
    "magY": 8,
    "magZ": -70
  }
]
````

----
The End.
