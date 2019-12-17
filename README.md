# autotier
Intelligently moves files between storage tiers based on frequency of use, file age, and tier fullness.

## What it does
`autotier` crawls through each tier's directory and queues up files, sorted by a combination of frequency of use and age. It moves the oldest/least used files to the next lowest storage tier and replaces the original with a symlink to the new location until the tier usage is below the defined upper watermark. Conversely, it also moves the newest/most frequently used files up to higher tiers until the higher tier usage is above the lower watermark, given that the file to be moved won't overshoot the max watermark. This behaviour cascades across as many storage tiers as you want to define in the configuration file.

## Usage
Create a [crontab](https://linux.die.net/man/5/crontab) entry to periodically run `autotier`. The default configuration file is `/etc/autotier.conf`, but this can be changed by passing the `-c`/`--config` flag followed by the path to the alternate configuration file. The first defined tier should be the working tier that is exported. So far, `samba` is the only sharing tool that seems to work with this software. `nfs` is too literal, and has no capability of following wide symlinks.

## Configuration
### Autotier Config
#### Global Config
For global configuration of `autotier`, options are placed below the `[Global]` header. Currently the only global option is log level, which can be set to either 0 (no logging), 1 (basic logging), or 2 (debug logging), and defaults to 1. Example:
```
[Global]
LOG_LEVEL=2
```
The global config section can be placed before, after, or between tier definitions.
#### Tier Config
The layout of a single tier's configuration entry is as follows:
```
[<Tier name>]
DIR=/path/to/storage/tier
MAX_WATERMARK=<0-100% of tier usage at which to tier down old files to lower tier>
MIN_WATERMARK=<0-100% of tier usage at which to tier up new files from lower tier>
```
As many tiers as desired can be defined in the configuration, however they must be in order of fastest to slowest. The tier's name can be whatever you want but it cannot be `global` or `Global`. Tier names are only used for config diagnostics. Usage watermark can be disabled by omitting the option in the configuration file, causing `autotier` to always tier down files older than the defined expiry age.   
Below is a complete example of a configuration file:
```
# autotier.conf
[Global]
LOG_LEVEL=0

[Fastest Tier]
DIR=/tier_1         # fast tier storage pool
MAX_WATERMARK=80    # tier down old files if tier is 80% full
MIN_WATERMARK=60    # try to keep tier at least 60% full

[Medium Tier]
DIR=/tier_2
MAX_WATERMARK=90
MIN_WATERMARK=80

[Slower Tier]
DIR=/tier_3
MAX_WATERMARK=50
MIN_WATERMARK=0

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
