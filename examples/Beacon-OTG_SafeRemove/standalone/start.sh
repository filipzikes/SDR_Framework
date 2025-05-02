#!/bin/sh
cd /tmp/
ps > ps.txt
if [ $(grep transmit ps.txt | wc -l) -eq 1 ]; then
    txpid=$(grep transmitter ps.txt | awk '{print $1}')
    kill -2 $txpid
    echo heartbeat > /sys/class/leds/led0:green/trigger
fi

sleep 1
ACTION=remove_all /lib/mdev/automounter.sh
echo timer > /sys/class/leds/led0:green/trigger

sleep 5
cd /tmp/standalone
mkdir log
uptime >> log/start.log
nohup ./transmitter_beacon >> log/tx.log 2>&1 &
echo default-on > /sys/class/leds/led0:green/trigger
