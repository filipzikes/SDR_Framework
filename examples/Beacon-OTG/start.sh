#!/bin/sh
sleep 20
uptime >> $1/log/start.log
cd $1
nohup ./transmitter_beacon >> $1/log/out.log 2>&1 &
