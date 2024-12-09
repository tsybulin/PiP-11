#!/bin/sh

cd pip-11

tftp <<EOF
verbose
connect 172.16.103.101
mode binary
put kernel7l.img
quit
EOF

cd ..
