#!/bin/bash

SILNIARES=ressilnia.txt

function testujsilnie () {
  for i in `cat "/home/adam/CLionProjects/c_asynchronic/test/dane/$1"`; do
	echo $i
	echo $i | "/home/adam/CLionProjects/c_asynchronic/test/silnia" >> $SILNIARES
  done
  if diff $SILNIARES "/home/adam/CLionProjects/c_asynchronic/test/dane/res$1"; then
	  return 1
  else
	  return 0
  fi
}


if [ $# -le 0 ]; then
	echo "Powinien być jeden argument"
	exit 1
fi

if [ -f $SILNIARES ]; then
	rm $SILNIARES
fi

if [ $1 -ge 1 ]; then
	if testujsilnie stud; then
		exit 1
	fi
fi

if [ -f $SILNIARES ]; then
	rm $SILNIARES
fi

if [ $1 -ge 2 ]; then
       if testujsilnie thorough; then
	       exit 1
       fi
fi

if [ -f $SILNIARES ]; then
	rm $SILNIARES
fi
