# ParkAI

### Will be the main app of the AI Server integrating image aquisition, detections and pod/led control as well as API's

GUI Cross Compile app:
1. Sends heartbeat to parksolUSA cloud server.
2. Reads data from counting software and sends Gate Data to parksolUSA Cloud server for parking reports.

## Prerequisites
### Update to cmake 3.24
```bash
sudo apt remove --purge cmake -y
sudo apt update
sudo apt install -y build-essential libssl-dev

wget https://github.com/Kitware/CMake/releases/download/v3.24.3/cmake-3.24.3.tar.gz
tar -zxvf cmake-3.24.3.tar.gz
cd cmake-3.24.3

./bootstrap
make -j$(nproc)
sudo make install
sudo ln -s /usr/local/bin/cmake /usr/bin/cmake
cmake --version
```

### Install X11 support (IF BUILDING ON WSL)
```bash
sudo apt install -y libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
```
### Install CURL
```bash
sudo apt install -y libcurl4-openssl-dev
```
### Install GStreamer
```bash
sudo apt install -y \
  libgstreamer1.0-dev \
  libgstreamer-plugins-base1.0-dev \
  gstreamer1.0-tools \
  gstreamer1.0-plugins-base \
  gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad \
  gstreamer1.0-plugins-ugly \
  gstreamer1.0-libav
```
## Install opencv
```bash
sudo apt-get install libopencv-dev
```

## Install sqlite3
```bash
sudo apt-get install libsqlite3-dev
```

### Download ONNX-Runtime prebuilt libraries
```bash
wget https://github.com/microsoft/onnxruntime/releases/download/v1.19.2/onnxruntime-linux-x64-gpu-1.19.2.tgz
tar -xzf onnxruntime-linux-x64-gpu-1.19.2.tgz
cp -r onnxruntime-linux-x64-gpu-1.19.2/include ./libdeepvision
cp -r onnxruntime-linux-x64-gpu-1.19.2/lib ./libdeepvision
```



## build
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

