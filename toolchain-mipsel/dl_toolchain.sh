#!/bin/sh

DIR="toolchain-3.4.x"
DL_NAME="mipsel-linux-uclibc.tar.xz"
DL_URL="https://github.com/TurBoTse/padavan-toolchain/releases/download/3.4_kernal/$DL_NAME"

if [ -d $DIR ]; then
	echo "$DIR already exists!"
	exit
fi

curl -O -L $DL_URL && \
mkdir -p $DIR && \
tar -xvf $DL_NAME -C $DIR
