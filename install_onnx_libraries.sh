wget https://github.com/microsoft/onnxruntime/releases/download/v1.23.2/onnxruntime-linux-x64-gpu-1.23.2.tgz
# wget https://github.com/microsoft/onnxruntime/releases/download/v1.19.2/onnxruntime-linux-x64-gpu-1.19.2.tgz
# tar -xzf onnxruntime-linux-x64-gpu-1.19.2.tgz
# cp -r onnxruntime-linux-x64-gpu-1.19.2/include ./libdeepvision
# cp -r onnxruntime-linux-x64-gpu-1.19.2/lib ./libdeepvision

tar -xzf onnxruntime-linux-x64-gpu-1.23.2.tgz
cp -r onnxruntime-linux-x64-gpu-1.23.2/include ./libdeepvision
cp -r onnxruntime-linux-x64-gpu-1.23.2/lib ./libdeepvision