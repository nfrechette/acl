#!/usr/bin/env bash

# Extract our command line arguments
COMPILER=$1

# Convert our compiler string into our XCode path
# Paths must match Github Action virtual images
if [[ $COMPILER == xcode10 ]]; then
    XCODE_PATH="/Applications/Xcode_10.3.app"
elif [[ $COMPILER == xcode11 ]]; then
    XCODE_PATH="/Applications/Xcode_11.7.app"
elif [[ $COMPILER == xcode12 ]]; then
    XCODE_PATH="/Applications/Xcode_12.3.app"
fi

# Select our XCode version
sudo xcode-select -s $XCODE_PATH/Contents/Developer;
