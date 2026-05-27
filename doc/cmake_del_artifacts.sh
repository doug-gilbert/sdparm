#!/bin/sh

echo "Entering doc/cmake_del_artifacts.sh"

rm -rf \
	CMakeCache.txt \
	CMakeFiles \
	cmake_install.cmake \
	sdparm.8.gz \
	sdparm_json.8.gz \
	Makefile

