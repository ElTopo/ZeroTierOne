branch lxl-optware-ng is my port for my optware-ng devices.

The official Zerotier has several problems running on Optware-NG devices:
1. it saves keys and other persistent files under hard coded directory
   /var/lib/zerotier-one, /var on optware-ng systems are likely mounted
   as tmpfs and not persistent;
2. it does not follow $PERFIX rule and installs itself to /usr, while 
   optware-ng uses /opt, in fact, /usr may be in rootfs image and not
   changable;
3. some devices don't have tun with fixed MTU value.

I am trying to solve these issues on my optware-ng devices (a router, and
   a NAS)
