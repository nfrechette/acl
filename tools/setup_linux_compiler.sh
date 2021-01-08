#!/usr/bin/env bash

# Extract our command line arguments
COMPILER=$1

# Convert our GCC compiler into a list of packages it needs
if [[ $COMPILER == gcc5 ]]; then
    PACKAGES="g++-5 g++-5-multilib g++-multilib"
elif [[ $COMPILER == gcc6 ]]; then
    PACKAGES="g++-6 g++-6-multilib g++-multilib"
elif [[ $COMPILER == gcc7 ]]; then
    PACKAGES="g++-7 g++-7-multilib g++-multilib"
elif [[ $COMPILER == gcc8 ]]; then
    PACKAGES="g++-8 g++-8-multilib g++-multilib"
elif [[ $COMPILER == gcc9 ]]; then
    PACKAGES="g++-9 g++-9-multilib g++-multilib"
elif [[ $COMPILER == gcc10 ]]; then
    PACKAGES="g++-10 g++-10-multilib g++-multilib"
fi

# If using clang, add our apt source key
if [[ $COMPILER == clang* ]]; then
    curl -sSL "http://apt.llvm.org/llvm-snapshot.gpg.key" | sudo -E apt-key add - ;
fi

# Convert our clang compiler into a list of packages it needs and its source
if [[ $COMPILER == clang4 ]]; then
    # clang4 isn't available after xenial
    PACKAGES="clang-4.0 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/xenial/ llvm-toolchain-xenial-4.0 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang5 ]]; then
    PACKAGES="clang-5.0 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-5.0 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang6 ]]; then
    PACKAGES="clang-6.0 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-6.0 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang7 ]]; then
    PACKAGES="clang-7 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-7 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang8 ]]; then
    PACKAGES="clang-8 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-8 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang9 ]]; then
    PACKAGES="clang-9 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-9 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang10 ]]; then
    PACKAGES="clang-10 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-10 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
elif [[ $COMPILER == clang11 ]]; then
    PACKAGES="clang-11 libstdc++-5-dev libc6-dev-i386 g++-5-multilib g++-multilib"
    echo "deb http://apt.llvm.org/bionic/ llvm-toolchain-bionic-11 main" | sudo tee -a /etc/apt/sources.list > /dev/null ;
fi

# Install the packages we need
sudo -E apt-add-repository -y "ppa:ubuntu-toolchain-r/test";
sudo -E apt-get -yq update;
sudo -E apt-get -yq --no-install-suggests --no-install-recommends --force-yes install $PACKAGES;
