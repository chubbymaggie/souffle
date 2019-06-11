#!/bin/bash

# Enable debugging and logging of shell operations
# that are executed.
set -e
set -x

# Install requirements of MAC OS X
. .travis/osx/install.sh

rm /usr/local/include/c++ || true

# Install gcc instead of gcc-x.x if a current version is preferred
brew install gcc@8

# Using 'g++' will call the xcode link to clang
g++-8 --version

export CC=gcc-8
export CXX=g++-8
