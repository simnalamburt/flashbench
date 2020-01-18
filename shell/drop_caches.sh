#!/bin/bash

while true
do
	sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"
	sleep 1
done
