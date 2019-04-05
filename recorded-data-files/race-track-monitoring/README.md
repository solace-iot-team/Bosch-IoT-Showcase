# Monitoring slot cars on the race track

## Scenario:

Monitoring the cars on the race track.

* devices are mounted on the two slot cars: red car and blue car
* devices are set to 'race-mode'
* cars start racing
* devices are re-set into 'non-race-mode' - i.e. a different mode

## Goals

### Display
* display real-time data for each car as they race

### Analytics

The gyro shows how a car goes through a curve - left/right.

The light sensor detects a car going through a tunnel - 1 tunnel on the track.

* count the laps for each car and send out notification for each lap
* detect the winner and send out notification
* detect a crash and send out alert

## Scenario Details

* devices are in 'normal mode'
* devices are set into 'race-mode'
  - 100 samples per second
    - 25 messages per second
    - 4 samples per message
  - sensors:
    - gyroscope
    - light
* multiple laps
  - 1 lap about 0.9 seconds
  - we may have a tunnel - light sensor detects 'darkness'
* devices are re-set to 'normal-mode'

## Playback

See Wiki: https://github.com/solace-iot-team/Bosch-IoT-Showcase/wiki/Message-Simulator

### Sample Topic
$create/iot-event/simulation/solace_racing/racetrack/device/{deviceId}/metrics

### Sample Message

````
[
  {
    "timestamp": "2019-04-03T15:04:16.821Z",
    "deviceId": "race-car-device-id-change-me",
    "light": 37440,
    "gyroX": -9272,
    "gyroY": 0,
    "gyroZ": -347639
  },
  {
    "timestamp": "2019-04-03T15:04:16.831Z",
    "deviceId": "race-car-device-id-change-me",
    "light": 37440,
    "gyroX": 67161,
    "gyroY": 38918,
    "gyroZ": -310490
  },
  {
    "timestamp": "2019-04-03T15:04:16.841Z",
    "deviceId": "race-car-device-id-change-me",
    "light": 40320,
    "gyroX": 44042,
    "gyroY": 4514,
    "gyroZ": -283467
  },
  {
    "timestamp": "2019-04-03T15:04:16.851Z",
    "deviceId": "race-car-device-id-change-me",
    "light": 40320,
    "gyroX": -47946,
    "gyroY": -7808,
    "gyroZ": -238571
  }
]
````
