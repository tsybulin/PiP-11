#!/bin/sh

if [ $# == 0 ]; then
  echo "Usage: netdeploy.sh host"
  exit
fi

tftp <<EOF
verbose
connect $1
mode binary
put /Users/pasha/Projects/_pi/pip-11/app/pip-11/kernel7l.img kernel7l.img
quit
EOF
