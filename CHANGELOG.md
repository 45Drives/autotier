## Autotier 1.2.0-1

* Switch from unique config parser class to lib45d ConfigParser.
* Use lib45d Bytes and Quota classes to clean up math while tiering.
* Make CLI commands more reliable with lib45d Unix domain socket classes in place of FIFOs.
* Fixed bug where statfs() improperly reported fs size and usage.
* Fix deadlock issue while tiering files.
* Overhauled tiering algorithm to better fill high priority tiers.
* Fixed reporting in statfs() and df.
* Made file creation and opening more reliable.
* Tiering triggered by being over quota in release() now happens asynchronously.
* Accounts for files being opened by more than one process while preventing movement across tiers.
* Fix abort on unmount from rocksdb being opened in main thread by deferring opening to fuse init method.