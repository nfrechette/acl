#!/usr/bin/env bash

# Extract our command line arguments
COMPILER=$1

# See Github hosted runners:
# macos-12: https://github.com/actions/runner-images/blob/main/images/macos/macos-12-Readme.md
#   xcode 13.1, 12.2.1, 13.3.1, 13.4.1, 14.1, 14.2
# maxos-11: https://github.com/actions/runner-images/blob/main/images/macos/macos-11-Readme.md
#   xcode 11.7, 12.4, 12.5.1, 13.0, 13.1, 13.2.1
# maxos-10.15: https://github.com/actions/runner-images/blob/main/images/macos/macos-10.15-Readme.md
#   xcode 10.3, 11.2.1, 11.3.1, 11.4.1, 11.5, 11.6, 11.7, 12, 12.1, 12.1.1, 12.2, 12.3, 12.4

# Convert our compiler string into our XCode path
# Paths must match Github Action virtual images
if [[ $COMPILER == xcode10 ]]; then
    XCODE_PATH="/Applications/Xcode_10.3.app"
elif [[ $COMPILER == xcode11 ]]; then
    XCODE_PATH="/Applications/Xcode_11.7.app"
elif [[ $COMPILER == xcode12 ]]; then
    XCODE_PATH="/Applications/Xcode_12.5.1.app"
elif [[ $COMPILER == xcode13 ]]; then
    XCODE_PATH="/Applications/Xcode_13.2.1.app"
elif [[ $COMPILER == xcode14 ]]; then
    XCODE_PATH="/Applications/Xcode_14.2.app"
fi

# Select our XCode version
sudo xcode-select -s $XCODE_PATH/Contents/Developer;
