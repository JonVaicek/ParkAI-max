# ParkAI

### Will be the main app of the AI Server integrating image aquisition, detections and pod/led control as well as API's

GUI Cross Compile app:
1. Sends heartbeat to parksolUSA cloud server.
2. Reads data from counting software and sends Gate Data to parksolUSA Cloud server for parking reports.


### build
```bash
mkdir build \
cd build
```
```bash
cmake ..
```
```bash
cmake --build . --config Debug
```

