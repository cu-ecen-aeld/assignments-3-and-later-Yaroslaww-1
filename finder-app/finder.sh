#!/bin/sh

if [ $# -lt 2 ]
then
	echo "Not enough parameters"
	exit 1
fi

filesdir=$1
searchstr=$2

if [ ! -d $filesdir ] 
then
    echo "Not such directory"
	exit 1
fi

files_count=$(find $filesdir -type f | wc -l)
files_contain_string=$(grep -r $searchstr $filesdir | wc -l)

echo "The number of files are $files_count and the number of matching lines are $files_contain_string"
exit 0
