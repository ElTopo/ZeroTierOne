branch lxl-optware-ng is my port for my optware-ng devices.

The official Zerotier has several problems running on Optware-NG devices:
1. it saves keys and other persistent files under hard coded directory
   /var/lib/zerotier-one, /var on optware-ng systems are likely mounted
   as tmpfs and not persistent (while $PREFIX/var is);
2. it does not follow $PERFIX rule and installs itself to /usr, while 
   optware-ng uses /opt, in fact, /usr may be in rootfs image and not
   changable;
3. some devices don't have tun with changable MTU value;
4. generally optware-ng devices don't have tools for making manpages.

I am trying to solve these issues on my optware-ng devices (a router, and
   a NAS)

NOTE: don't forget to update /jffs/scripts/post-mount or bootscript 
      with the following code:

        # zerotier-one
        if [ -x /opt/sbin/zerotier-one ]
        then
                modprobe tun
                # if /var/lib/zerotier-one is not persistent, recreate it
                rm -rf /var/lib/zerotier-one
                ln -nsf /opt/var/lib/zerotier-one /var/lib/zerotier-one
                /opt/sbin/zerotier-one -d

                # allow network access from zt0
                iptables -I INPUT -i zt0 -j ACCEPT
        fi

