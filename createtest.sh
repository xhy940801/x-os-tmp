#!/bin/sh

if [ $# -lt 1 ]
then
    echo "Usage:$0 name [src1 src2 ...]"
    exit 1
fi

while [ -d "test/$1" ]
do
    echo "test/$1 has exist, are you want to rebuild it? [Y/N]"
    read
    if [ "$REPLY" = "N" ]
    then
        exit 0
    fi
    if [ "$REPLY" = "Y" ]
    then
        break
    fi
done

arrsrcs=($*)
unset arrsrcs[0]
srcs=${arrsrcs[*]}

mkdir -p test/$1
echo "EXPORTFILES=$srcs" | dos2unix > test/$1/makefile
cat mk.test.template | dos2unix >> test/$1/makefile
cd test/$1
mkdir -p obj
mkdir -p dep
mkdir -p src
mkdir -p inc
mkdir -p bin
