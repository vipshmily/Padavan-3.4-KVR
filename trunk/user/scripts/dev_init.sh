#!/bin/sh

mount -t proc proc /proc
mount -t sysfs sysfs /sys
[ -d /proc/bus/usb ] && mount -t usbfs usbfs /proc/bus/usb

size_tmp="24M"
size_var="4M"
size_etc="6M"

if [ "$1" == "-l" ] ; then
	size_tmp="8M"
	size_var="1M"
fi

mount -t tmpfs tmpfs /dev   -o size=8K
mount -t tmpfs tmpfs /etc   -o size=$size_etc,noatime
mount -t tmpfs tmpfs /home  -o size=1M
mount -t tmpfs tmpfs /media -o size=8K
mount -t tmpfs tmpfs /mnt   -o size=8K
mount -t tmpfs tmpfs /tmp   -o size=$size_tmp
mount -t tmpfs tmpfs /var   -o size=$size_var

mkdir /dev/pts
mount -t devpts devpts /dev/pts

mount -t debugfs debugfs /sys/kernel/debug

ln -sf /etc_ro/mdev.conf /etc/mdev.conf
mdev -s

# create dirs
mkdir -p -m 777 /var/lock
mkdir -p -m 777 /var/locks
mkdir -p -m 777 /var/private
mkdir -p -m 700 /var/empty
mkdir -p -m 777 /var/lib
mkdir -p -m 777 /var/log
mkdir -p -m 777 /var/run
mkdir -p -m 777 /var/tmp
mkdir -p -m 777 /var/spool
mkdir -p -m 777 /var/lib/misc
mkdir -p -m 777 /var/state
mkdir -p -m 777 /var/state/parport
mkdir -p -m 777 /var/state/parport/svr_statue
mkdir -p -m 777 /tmp/var
mkdir -p -m 777 /tmp/hashes
mkdir -p -m 777 /tmp/modem
mkdir -p -m 777 /tmp/rc_notification
mkdir -p -m 777 /tmp/rc_action_incomplete
mkdir -p -m 700 /home/root
mkdir -p -m 700 /home/root/.ssh
mkdir -p -m 755 /etc/storage
mkdir -p -m 755 /etc/ssl
mkdir -p -m 755 /etc/Wireless
mkdir -p -m 750 /etc/Wireless/RT2860
mkdir -p -m 750 /etc/Wireless/iNIC

# mount cgroupfs if kernel provides cgroups
if [ -e /proc/cgroups ] && [ -d /sys/fs/cgroup ]; then
	if ! mountpoint -q /sys/fs/cgroup; then
		mount -t tmpfs -o uid=0,gid=0,mode=0755 cgroup /sys/fs/cgroup
	fi
	(cd /sys/fs/cgroup
	for sys in $(awk '!/^#/ { if ($4 == 1) print $1 }' /proc/cgroups); do
		mkdir -p $sys
		if ! mountpoint -q $sys; then
			if ! mount -n -t cgroup -o $sys cgroup $sys; then
				rmdir $sys || true
			fi
		fi
	done)
	if [ -e /sys/fs/cgroup/memory/memory.use_hierarchy ]; then
		echo 1 > /sys/fs/cgroup/memory/memory.use_hierarchy
	fi
fi

# extract storage files
mtd_storage.sh load

touch /etc/resolv.conf

if [ -f /etc_ro/openssl.cnf ]; then
	cp -f /etc_ro/openssl.cnf /etc/ssl
fi

# create symlinks
ln -sf /home/root /home/admin
ln -sf /proc/mounts /etc/mtab
ln -sf /etc_ro/ethertypes /etc/ethertypes
ln -sf /etc_ro/protocols /etc/protocols
ln -sf /etc_ro/services /etc/services
ln -sf /etc_ro/shells /etc/shells
ln -sf /etc_ro/profile /etc/profile
ln -sf /etc_ro/e2fsck.conf /etc/e2fsck.conf
ln -sf /etc_ro/ipkg.conf /etc/ipkg.conf

# tune linux kernel
echo 65536        > /proc/sys/fs/file-max
echo "1024 65535" > /proc/sys/net/ipv4/ip_local_port_range

# fill storage
mtd_storage.sh fill

# prepare ssh authorized_keys
if [ -f /etc/storage/authorized_keys ] ; then
	cp -f /etc/storage/authorized_keys /home/root/.ssh
	chmod 600 /home/root/.ssh/authorized_keys
fi

# setup htop default color
if [ -f /usr/bin/htop ]; then
	mkdir -p /home/root/.config/htop
	echo "color_scheme=6" > /home/root/.config/htop/htoprc
fi

# clear cache for more memory
echo 3 > /proc/sys/vm/drop_caches

# optimize kernel / tcp
# https://developer.aliyun.com/article/661318
echo 0 > /proc/sys/kernel/sysrq
echo 65536 > /proc/sys/kernel/msgmnb
echo 65536 > /proc/sys/kernel/msgmax
echo 0 > /proc/sys/net/ipv4/ip_forward
echo 0 > /proc/sys/net/ipv4/conf/default/accept_source_route
echo 1 > /proc/sys/net/ipv4/tcp_syncookies
echo "4096 131072 1048576" > /proc/sys/net/ipv4/tcp_wmem
echo "4096 131072 1048576" > /proc/sys/net/ipv4/tcp_rmem
echo 16777216 > /proc/sys/net/core/wmem_max
echo 8388608 > /proc/sys/net/core/wmem_default
echo 8388608 > /proc/sys/net/core/rmem_default
echo 16777216 > /proc/sys/net/core/rmem_max
echo 262144 > /proc/sys/net/core/somaxconn
echo 0 > /proc/sys/net/ipv4/tcp_timestamps
echo 262144 > /proc/sys/net/ipv4/tcp_max_syn_backlog
echo 262144 > /proc/sys/net/core/netdev_max_backlog
echo 1 > /proc/sys/net/ipv4/tcp_syn_retries
echo 1 > /proc/sys/net/ipv4/tcp_synack_retries
echo 15 > /proc/sys/net/ipv4/tcp_fin_timeout
echo 30 > /proc/sys/net/ipv4/tcp_keepalive_time
echo "2048 65000" > /proc/sys/net/ipv4/ip_local_port_range

echo 15 > /proc/sys/net/ipv4/tcp_fin_timeout
echo 60 > /proc/sys/net/ipv4/tcp_keepalive_time

# perform start script
if [ -x /etc/storage/start_script.sh ] ; then
	/etc/storage/start_script.sh
fi

