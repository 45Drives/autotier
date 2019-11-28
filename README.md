# autotier
Moves files with an mtime older than the configured threshold to a lower tier of storage, et vice versa if the file is modified.

## What it does
If it finds a file in a defined storage tier that hasn't been written to in the specified expiry time, it moves the file to the next lowest storage tier and replaces the original with a symlink to the new location. Conversely, if this file is opened and modified (through the symlink or otherwise), the symlink is removed and the file is moved back to it's original tier. This behaviour cascades across as many storage tiers as you want to define in the configuration file.

## Usage
Create a [crontab](https://linux.die.net/man/5/crontab) entry to periodically run `autotier`. The default configuration file is `/etc/autotier.conf`, but this can be changed by passing the `-c`/`--config` flag followed by the path to the alternate configuration file. The first defined tier should be the working tier that is exported. So far, `samba` is the only sharing tool that seems to work with this software. `nfs` is too literal, and has no capability of following symlinks.

## Configuration
### Autotier Config
The layout of a single tier's configuration entry is as follows:
```
[<Tier name>]
DIR=/path/to/storage/tier
EXPIRES=<number of seconds until file expiry>
USAGE_WATERMARK=<0-100% of tier usage at which to tier down old files>
```
As many tiers as desired can be defined in the configuration, however they must be in order of fastest to slowest. The tier's name can be whatever you want, as it is just used for config diagnostics. Usage watermark can be disabled by omitting the option in the configuration file, causing `autotier` to always tier down files older than the defined expiry age.   
Below is an example of a configuration file:
```
# autotier.conf
[Fastest Tier]
DIR=/tier_1         # fast tier storage pool
EXPIRES=3600        # one hour
USAGE_WATERMARK=80  # tier down old files if tier is 80% full

[Medium Tier]
DIR=/tier_2
EXPIRES=7200   # two hours
USAGE_WATERMARK=50

[Slower Tier]
DIR=/tier_3
EXPIRES=14400  # four hours
USAGE_WATERMARK= # always tier down after four hours

# ... and so on
```
### Samba Config
Samba must be configured to follow symlinks outside of the storage pool. The following options need to be set for proper usage:
```
[global]
allow insecure wide links = yes
unix extensions = no
# ...
[<Share_Name>]
follow symlinks = yes
wide links = yes
path = /path/to/fastest/tier
# ...
```
## Acknowledgements
Credits to [Stephan Brumme](https://stephan-brumme.com/) for his single-header implementation of XXHash, which is used after a file is copied to verify that there were no errors.
