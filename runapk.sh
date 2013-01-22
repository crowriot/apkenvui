#!/bin/sh

# set nubs to joystick mode
NUB0=`cat /proc/pandora/nub0/mode`
NUB1=`cat /proc/pandora/nub1/mode`
echo absolute > /proc/pandora/nub0/mode
echo absolute > /proc/pandora/nub1/mode

./apkenv $1
$2

#restore nubs
echo $NUB0 > /proc/pandora/nub0/mode
echo $NUB1 > /proc/pandora/nub1/mode

