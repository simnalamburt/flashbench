flashbench
========
SSD firmware emulator for Linux, developed by Seoul National University,
[CARES] Lab.

[CARES]: http://davinci.snu.ac.kr

### Requirements
- Linux 3.13.0

```bash
# Build
make

# Start flashbench
sudo insmod flashBench.ko
# Format the created virtual SSD
sudo mkfs -t ext4 -b 4096 /dev/fbSSD
# Mount the virtual SSD
sudo mkdir -p /vSSD
sudo mount -o discard /dev/fbSSD /vSSD
# Check the status of the virtual SSD
cat /proc/summary

# If you want to clear the cache
# Reference: https://www.kernel.org/doc/Documentation/sysctl/vm.txt
echo 3 | sudo tee /proc/sys/vm/drop_caches

# Unmount the virtual SSD
sudo umount /dev/fbSSD
sudo rmdir /vSSD
# End flashbench
sudo rmmod flashBench
```

&nbsp;

--------

*flashbench* is distributed under the terms of the [GNU General Public License
v3.0] or any later version. See [COPYRIGHT] for details.

[GNU General Public License v3.0]: LICENSE
[COPYRIGHT]: COPYRIGHT
