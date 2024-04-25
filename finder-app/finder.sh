#!/bin/sh
# Author: Jorge Catarino

# Check if both arguments were passed
if [ -z "$1" ] || [ -z "$2" ]
then 
    echo "too few arguments"
    exit 1
fi

filesdir=$1
searchstr=$2

# Check if filesdir is a valid directory
if [ ! -d $filesdir ]
then
    echo "$filesdir not a valid dir"
    exit 1
fi

total_nr_files=$(find $filesdir -type f | wc -l)
total_matches=$(grep -Ro $filesdir -e $searchstr | wc -l)

echo "The number of files are $total_nr_files and the number of matching lines are $total_matches"
