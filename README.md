## DMP (Device mapper proxy) 


 A linux kernel module that creates virtual block devices on top of an existing device mapper and monitors the statistics of operations performed on the device. Statistics are available through the sysfs module. 
 
 Supported statistics:
 
+ Number of write requests
+ Number of read requests
+ Average block size per record
+ Average block size per read
+ Total number of requests
+ Average block size


---
## Installation

```sh
$ make
$ sudo insmod dmp.ko
```
--- 
## Demonstration
Devices setup:
```sh
$ dmsetup create zero1 --table "0 $size zero" #size - custom device size in pages
$ dmsetup create dmp1 --table "0 $size dmp /dev/mapper/zero1" #size = /dev/mapper/zero1 size
```

Some operations with devices:
```sh
$ dd if=/dev/random of=/dev/mapper/dmp1 bs=4k count=1
$ dd if=/dev/mapper/dmp1 of=/dev/null bs=4k count=1
```

Viewing statistics:
```sh
$ cat /sys/module/dmp/stat/volumes
```
File output:
```
read:
    reqs: 500
    avg size: 4096
write:
    reqs: 100
    avg size: 4096
total:
    reqs: 600  
    avg size: 4096
```