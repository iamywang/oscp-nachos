#!/bin/bash
rm DISK
./nachos -f #建立格式化磁盘

# 拷贝文件
./nachos -cp test/small small_file
./nachos -cp test/big big_file

./nachos -cp test/medium medium_file

#测试NAppend
./nachos -hap test/big small_file
./nachos -hap test/small big_file

./nachos -hap test/empty medium_file

./nachos -D