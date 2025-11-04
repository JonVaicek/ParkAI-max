#!/bin/bash
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
