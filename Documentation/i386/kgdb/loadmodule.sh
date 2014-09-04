#/bin/sh
# This script loads a module on a target machine and generates a gdb script.
# source generated gdb script to load the module file at appropriate addresses
# in gdb.
#
# Usage: 
# Loading the module on target machine and generating gdb script)
#	[foo]$ loadmodule.sh <modulename>
#
# Loading the module file into gdb
#	(gdb) source <gdbscriptpath>
#
# Modify following variables according to your setup. 
#	TESTMACHINE - Name of the target machine
#	GDBSCRIPTS - The directory where a gdb script will be generated
#
# Author: Amit S. Kale (akale@veritas.com).
#
# If you run into problems, please check files pointed to by following
# variables.
#	ERRFILE - /tmp/<modulename>.errs contains stderr output of insmod
#	MAPFILE - /tmp/<modulename>.map contains stdout output of insmod
#	GDBSCRIPT - $GDBSCRIPTS/load<modulename> gdb script.

TESTMACHINE=foo
GDBSCRIPTS=/home/bar

if [ $# -lt 1 ] ; then {
	echo Usage: $0 modulefile
	exit
} ; fi

MODULEFILE=$1
MODULEFILEBASENAME=`basename $1`

if [ $MODULEFILE = $MODULEFILEBASENAME ] ; then {
	MODULEFILE=`pwd`/$MODULEFILE
} fi

ERRFILE=/tmp/$MODULEFILEBASENAME.errs
MAPFILE=/tmp/$MODULEFILEBASENAME.map
GDBSCRIPT=$GDBSCRIPTS/load$MODULEFILEBASENAME

function findaddr() {
	local ADDR=0x$(echo "$SEGMENTS" | \
		grep "$1" | sed 's/^[^ ]*[ ]*[^ ]*[ ]*//' | \
		sed 's/[ ]*[^ ]*$//')
	echo $ADDR
}

function checkerrs() {
	if [ "`cat $ERRFILE`" != "" ] ; then {
		cat $ERRFILE
		exit
	} fi
}

#load the module
echo Copying $MODULEFILE to $TESTMACHINE
rcp $MODULEFILE root@${TESTMACHINE}:

echo Loading module $MODULEFILE
rsh -l root $TESTMACHINE  /sbin/insmod -m ./`basename $MODULEFILE` \
	> $MAPFILE 2> $ERRFILE
checkerrs

SEGMENTS=`head -n 11 $MAPFILE | tail -n 10`
TEXTADDR=$(findaddr "\\.text[^.]")
LOADSTRING="add-symbol-file $MODULEFILE $TEXTADDR"
SEGADDRS=`echo "$SEGMENTS" | awk '//{
	if ($1 != ".text" && $1 != ".this" &&
	    $1 != ".kstrtab" && $1 != ".kmodtab") {
		print " -s " $1 " 0x" $3 " "
	}
}'`
LOADSTRING="$LOADSTRING $SEGADDRS"
echo Generating script $GDBSCRIPT
echo $LOADSTRING > $GDBSCRIPT
