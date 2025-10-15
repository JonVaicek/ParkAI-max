# ONNX Vehicle Detection Pipeline

A multi-threaded vehicle detection system using ONNX Runtime for car detection, license plate detection, and OCR.

## Features

- **Car Detection**: YOLO-based vehicle detection
- **License Plate Detection**: YOLO-based license plate detector
- **OCR**: LPRNet for license plate text recognition
- **Multi-threading**: Configurable number of worker threads
- **Real-time Processing**: Continuous image processing from directory

## Requirements
- ubuntu-20.04
- C++17 or later
- OpenCV 4.x
- ONNX Runtime
- CMake 3.10+
- CUDA Toolkit
- cuDNN

## Models Required

Place these ONNX models in the project root:
- `yolo11_indoor.onnx` - Vehicle detection model
- `license_plate_detector.onnx` - License plate detection model  
- `us_lprnet_baseline18_deployable.onnx` - OCR model

## Build

# Set up libraries

 -install CMake
 ```bash
 sudo apt install build-essential cmake
 ```

 - get onnxruntime prebuilt lib:
 ```bash
 wget https://github.com/microsoft/onnxruntime/releases/download/v1.19.2/onnxruntime-linux-x64-gpu-1.19.2.tgz
 ```
 ``` bash
 tar -xzf onnxruntime-linux-x64-gpu-1.19.2.tgz
 ```
 ``` bash
 cp -r onnxruntime-linux-x64-gpu-1.19.2/include ./deepcount
 ```
 ```bash
 cp -r onnxruntime-linux-x64-gpu-1.19.2/lib ./deepcount
 ```
```bash
 sudo apt-get install libopencv-dev
 ```

cuDNN cuda 12:
```bash
 wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/x86_64/cuda-keyring_1.0-1_all.deb
 ```
 ```bash
sudo dpkg -i cuda-keyring_1.0-1_all.deb
```
``` bash
sudo apt-get update
```
```bash
sudo apt-get -y install cuda-toolkit-12-2
```
```bash
sudo apt-get install libcudnn9-cuda-12
```
```bash
./build.sh
```

## Usage
./detector [num_threads] [visualize]