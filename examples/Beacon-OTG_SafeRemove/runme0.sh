#!/bin/sh
cd $(dirname $i)
cp -r standalone/ /tmp
cd /root
/tmp/standalone/start.sh &
