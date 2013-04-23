#!/bin/sh

# set nubs to joystick mode
# see http://pandorawiki.org/Kernel_interface
NUB0=`cat /proc/pandora/nub0/mode`
NUB1=`cat /proc/pandora/nub1/mode`


if [ -e /usr/pandora/scripts/op_nubchange.sh ]; then
  /usr/pandora/scripts/op_nubchange.sh absolute absolute
else
  echo absolute > /proc/pandora/nub0/mode
  echo absolute > /proc/pandora/nub1/mode
fi

# fix touchscreen coordinate drift
# see http://pandorawiki.org/SDL#Cursor_drift_in_fullscreen_mode
export SDL_MOUSE_RELATIVE=0

./apkenv "$1"
$2

#restore nubs
echo $NUB0 > /proc/pandora/nub0/mode
echo $NUB1 > /proc/pandora/nub1/mode

if [ -e /usr/pandora/scripts/op_nubchange.sh ]; then
  /usr/pandora/scripts/op_nubchange.sh $NUB0 $NUB1
else
  echo $NUB0 > /proc/pandora/nub0/mode
  echo $NUB1 > /proc/pandora/nub1/mode
fi


