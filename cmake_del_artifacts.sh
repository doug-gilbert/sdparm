#!/bin/sh

# Designed to remove 'cmake . ; cmake --build . ' artifacts from an in-tree
# build. For an out-of-tree build (e.g. 'cmake -S . -B build ; cd build ;
# cmake --build . ; cpack . ') simply do 'cd .. ; rm -rf build ' .

# set -x

cd src || exit
./cmake_del_artifacts.sh
cd ..

cd doc || exit
./cmake_del_artifacts.sh
cd ..

rm -rf \
	build \
	CMakeCache.txt \
	CMakeFiles \
	config.h \
	CPackConfig.cmake \
	CPackSourceConfig.cmake \
	cmake_install.cmake \
	CTestTestfile.cmake \
	DartConfiguration.tcl \
	install_manifest.txt \
	sdparm \
	sdparm.8.gz \
	sdparm_json.8.gz \
	Testing \
	_CPack_Packages \
	Makefile

