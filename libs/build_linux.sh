#!/bin/sh

# ----------------
## init
# ----------------

# ensure current dir is .
LIB_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
cd $LIB_DIR

# fetch submodules
git submodule update --init --recursive

# ----------------
## build libmysofa
# ----------------

# install deps
sudo apt-get install libresample1
# unused? sudo apt-get install libcunit1
sudo apt-get install libcunit1-dev
sudo apt-get install zlib1g-dev

# compile 
cd libmysofa/build/
cmake ..
make && sudo make install
