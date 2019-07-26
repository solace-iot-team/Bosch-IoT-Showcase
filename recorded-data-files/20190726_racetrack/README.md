# Updated Racetrack Recordings

New recordings aimed at facilitating machine learning data analysis.

## Data format

### Telemetry
As for previous recordings - multiple events in JSON array, one JSON payload per line


### New - race events

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

## Scenarios
Recorded data files are in two subfolders:
* 20190726-infrequent-crashes: Mostly good laps, some crashes
* 20190726-frequent-crashes: Mostly crash laps
