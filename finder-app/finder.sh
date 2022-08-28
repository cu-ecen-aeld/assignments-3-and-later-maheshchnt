#!/bin/sh
# Tester script for assignment 1 and assignment 2
# Author: Mahesh Gudipati

#Check if num arguments are 2 and return error if they are not 2
if [ $# -ne 2 ]
then
	echo "Invalid number of arguments"
	exit 1
else
	filesdir=$1
	searchstr=$2
fi

#search if directory exist
if [ ! -d "$filesdir" ]
then
        echo "$filesdir not exist"
        exit 1
fi


# check the total files recursively and matching lines
echo "The number of files are $(find $filesdir -type f | wc -l ) and the number of matching lines are $( grep -r $searchstr $filesdir | wc -l )"
echo ""
exit 0
