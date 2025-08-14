#!/bin/bash

if [ "$1" == "debug" ]; then
    echo "Building debug version..."
    clang -O0 -g -Wall -o test *.c
else
    echo "Building release version..."
    clang -O3 -march=native -flto=thin -fomit-frame-pointer -fno-exceptions -fno-rtti -fvisibility=hidden -mno-red-zone -ftls-model=initial-exec -Wl,-O3 -Wl -DNDEBUG -g0 -o test *.c
fi