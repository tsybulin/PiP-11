#!/bin/sh

tftp <<EOF
verbose
connect 172.16.103.101
mode binary
put /Users/pasha/Projects/_pi/pip-11/app/pip-11/kernel7l.img kernel7l.img
quit
EOF
