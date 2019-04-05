# Throw Fragile Goods

## Scenario: Throw fragile content package
fragile, sensitive to shock content is thrown

## Goal
Detect throwing and generate alert message

## Scenario Details

* device is in normal mode
* device is set into 'transport mode'
  - 100 samples per second
    - 25 messages per second
    - 4 samples per message
  - sensors:
    - accelerator
* multiple repeated throws
  - between 1-2 meters
  - package lands on 'soft' surface

## Playback

See Wiki: https://github.com/solace-iot-team/Bosch-IoT-Showcase/wiki/Message-Simulator

### Sample Topic

$create/iot-event/simulation/solace_logistics/pallet/{deviceId}/metrics

### Sample Message

````
[
  {
    "timestamp": "2019-04-02T17:38:54.295Z",
    "deviceId": "ricardo_1_transport_monitor",
    "acceleratorX": -16,
    "acceleratorY": -3,
    "acceleratorZ": 987
  },
  {
    "timestamp": "2019-04-02T17:38:54.303Z",
    "deviceId": "ricardo_1_transport_monitor",
    "acceleratorX": -13,
    "acceleratorY": -1,
    "acceleratorZ": 987
  },
  {
    "timestamp": "2019-04-02T17:38:54.313Z",
    "deviceId": "ricardo_1_transport_monitor",
    "acceleratorX": -14,
    "acceleratorY": -4,
    "acceleratorZ": 986
  },
  {
    "timestamp": "2019-04-02T17:38:54.323Z",
    "deviceId": "ricardo_1_transport_monitor",
    "acceleratorX": -15,
    "acceleratorY": -3,
    "acceleratorZ": 985
  }
]
````

------
The End.
