#!/bin/sh

echo "Entering src/cmake_del_artifacts.sh"

rm -rf \
	CMakeCache.txt \
	CMakeFiles \
	cmake_install.cmake \
	sdparm \
	Makefile

