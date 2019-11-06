#!/bin/bash
rm DISK
./nachos -f #建立格式化磁盘

# 拷贝文件
./nachos -cp test/small small1
./nachos -cp test/medium medium1
./nachos -cp test/empty empty_file

#测试Append
./nachos -ap test/big small1
./nachos -ap test/medium empty_file
./nachos -ap test/small medium1

./nachos -D