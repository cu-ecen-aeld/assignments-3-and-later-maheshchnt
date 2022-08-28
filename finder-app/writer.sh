#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Mahesh Gudipati

#Check if num arguments are 2. return error if num args are not 2
if [ $# -ne 2 ]
then
	echo "Invalid number of arguments"
	exit 1
else
	writefile=$1
	writestr=$2
fi

#search if dir exist. If not, create one.
if [ ! -d "$(dirname $writefile)" ]
then
	mkdir -p "$(dirname $writefile)"
	#if dir is not created, return error
	if [ ! -d "$(dirname $writefile)" ]
	then
		echo "Couldn't create dir: "$(dirname $writefile)""
		exit 1
	fi
fi

#search if file exist. If not, create one.
if [ ! -f "$writefile" ]
then
	touch "$writefile"
	if [ ! -f "$writefile" ]
	then
		echo "Couldn't create file : "$writefile""
                exit 1
	fi
fi

 #write the string into file. Overwrite if the file already exist with some contents..
        echo "$writestr" > $writefile
	echo "written--"$writestr"--string into--"$writefile"--file"
        exit 0




