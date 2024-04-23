#!/bin/bash
# Author: Jorge Catarino

command_successful(){
    if [ $1 -ne 0 ] ; then
        return 1
    else
        return 0
    fi
}

# Check if both arguments were passed
if [ -z "$1" ] || [ -z "$2" ]
then 
    echo "too few arguments"
    exit 1
fi

writefile=$1
writestr=$2

dirpath=$(dirname $writefile)

if [ ! -d $dirpath ]
then
    mkdir -p $dirpath
    if ! command_successful $?
    then
        echo "Could not create directory"
        exit 1
    fi
fi

echo $writestr > $writefile
if ! command_successful $?
then
        echo "Could not write to file"
        exit 1
fi
