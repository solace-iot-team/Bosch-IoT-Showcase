# Updated Racetrack Recordings

New recordings aimed at facilitating machine learning data analysis.

## Data format

### Telemetry
As for previous recordings - multiple events in JSON array, one JSON payload per line
Sample frequency - 100 data points per second, 25 messages with 4 events each.

### New - race events

Each event is recorded separately, one each line. The event is wrapped in a JSON array for compatibility wirth the data recorder.

Lap:
```[{"event":"Lap","timestamp":"2019-07-26T15:16:01.538Z","deviceId":"Lap"}]```

Crash:
```[{"event":"Crash","timestamp":"2019-07-26T15:17:16.116Z","deviceId":"Crash"}]```

## Data recorded

The recordings are for one car on the track. There are four files:
* 24d4830458e86aec.json - Accelerometer sensor data
* 24d11f0258e8ba9f.json - Gyroscope sensor data
* Lap.json - Lap events - first lap event marks start of recording, followed by events for each subsequent lap
* Crash.json - Crash events - when a crash occurred. Different kinds of crashes - slide of the rail, topple over etc

### Correlation of events
All events contain a timestamp in GMT/UTC so events can be correlated based in the timestamp.

## Scenarios
Recorded data files are in two subfolders:
* 20190726-infrequent-crashes: Mostly good laps, some crashes
* 20190726-frequent-crashes: Mostly crash laps

## Playback of events
Sensor data cna be played back using the data player. Event data can not be played back as the player sends data at a regular frequency exactly like the original sensor.

