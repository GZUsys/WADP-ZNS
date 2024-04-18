## An Efficient Wear-Leveling-Aware Data Placement for LSM-Tree based Key-Value Store On ZNS SSDs

WADP is built as an extension of ZenFS.
Compile and run by referring to the introduction provided at https://github.com/westerndigitalcorporation/zenfs.

1.Download, build and install libzbd. See the libzbd README for instructions.https://github.com/westerndigitalcorporation/libzbd/blob/master/README.md

2.Build and install rocksdb with zenfs enabled:
```
$ DEBUG_LEVEL=0 ROCKSDB_PLUGINS=zenfs make -j48 db_bench install
```
3.Build the zenfs utility:
```
$ cd plugin/zenfs/util
$ make
```
If you want to use db_bench for testing zenfs performance, there is a a convenience script
that runs the 'long' and 'quick' performance test sets with a good set of parameters
for the drive.

`cd tests; ./zenfs_base_performance.sh <zoned block device name> [ <zonefs mountpoint> ]`



