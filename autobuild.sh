#! /bin/bash

#发生错误终止运行
set -e

sudo rm -rf `pwd`/build/*
sudo rm -rf `pwd`/bin/*

cd `pwd`/build

sudo cmake ..
sudo make
cd ..
sudo cp -rf `pwd`/src/include  `pwd`/lib
TARGET_DIR="/usr/include/libmyrpc "
if [ ! -d "$TARGET_DIR" ];
then
    sudo mkdir -p "$TARGET_DIR"
fi
sudo cp -f `pwd`/lib/include/* /usr/include/libmyrpc
sudo cp -f `pwd`/lib/libprpc.a /usr/lib