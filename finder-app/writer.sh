#!/bin/sh

if [ $# -lt 2 ]
then
	echo "Not enough parameters"
	exit 1
fi

writefile=$1
writestr=$2
dir=$(dirname ${writefile})

if [ -d $dir ] 
then
	echo $writestr > $writefile
else
	mkdir -p $dir
	echo $writestr > $writefile
fi

echo $writestr > $writefile
exit 0
