#!/bin/sh

# restart zerotier client

# am I root?
MYUID=$(id -u)
if [ $MYUID -eq 0 ] 
then 
	SUDO=
else
	SUDO=sudo
fi

$SUDO killall -9 zerotier-one 
$SUDO zerotier-one -d /opt/var/lib/zerotier-one

sleep 5
PIDs=$(/opt/bin/busybox pidof zerotier-one)
if [ -z "$PIDs" ] 
then
	echo "zerotier-one is NOT running!"
else
	echo "zerotier-one is running."
fi

