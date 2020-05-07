# autotier
Intelligently moves files between storage tiers based on frequency of use, file age, and tier fullness.

## What it does
`autotier` crawls through each tier's directory and queues up files, sorted by a combination of frequency of use and age. It fills the defined tiers to their watermarked capacity, starting at the fastest tier with the highest priority files, working its way down until no files are left. If you do a lot of writing, set a lower watermark for the highest tier to allow for more room. If you do mostly reading, set a higher watermark to allow for as much use as possible out of your available top tier storage.

## Installation
```
yum install https://github.com/45Drives/autotier/releases/download/v0.6.1-beta/autotier-0.6.1-1.el7.x86_64.rpm
```

For apt-get based systems, or building from source:

```
#Change to home directory
cd ~
#Clone autotier github to ~/autotier
git clone https://github.com/45Drives/autotier.git
#Change directory into ~/autotier
cd autotier
#Install requirements
sudo apt-get install libboost-all-dev -y
#Compile
make
```

You will need to create the `/etc/autotier.conf` config file, and setup a crontab entry to run autotier.


## Usage
The RPM install package includes a systemd unit file. Configure `autotier` as described below and enable the daemon with `systemctl enable autotier` The default configuration file is `/etc/autotier.conf`, but this can be changed by passing the `-c`/`--config` flag followed by the path to the alternate configuration file. The first defined tier should be the working tier that is exported. So far, `samba` is the only sharing tool that seems to work with this software. `nfs` is too literal, and has no capability of following wide symlinks.

### Command Line Tools
```
autotier usage:
  autotier <command> <flags> [{-c|--config} </path/to/config>]
commands:
  oneshot      - execute tiering only once
  run          - start tiering of files as daemon
  status       - list info about defined tiers
  pin <"tier name"> <"/path/to/file">...
               - pin file(s) to tier using tier name in config file or full path to *tier root*
               - if a path to a directory is passed, all top-level files will be pinned
  unpin </path/to/file>...
               - remove pin from file(s)
  config       - display current configuration file
  list-pins    - show all pinned files
  list-popularity
               - print list of all tier files sorted by frequency of use
  help         - display this message
flags:
  -c --config <path/to/config>
               - override configuration file path (default /etc/autotier.conf)
```
Examples:  
Run tiering of files in daemon mode:  
`autotier run`  
Run tiering of files only once:  
`autotier oneshot`  
Show status of configured tiers:  
`autotier status`  
Pin a file to a tier with \<Tier Name\>:  
`autotier pin "<Tier Name>" /path/to/file`  
Pin multiple files:  
`autotier pin "<Tier Name>" /path/to/file1 /path/to/dir/* /bash/expansion/**/*`  
`find /path -type f -print | xargs autotier pin "<Tier Name>"`  
Remove pins:  
`autotier unpin /path/to/file`  
`find /path -type f -print | xargs autotier unpin`  
List pinned files:  
`autotier list-pins`


## Configuration
### Autotier Config
#### Global Config
For global configuration of `autotier`, options are placed below the `[Global]` header. Currently the only global option is log level, which can be set to either 0 (no logging), 1 (basic logging), or 2 (debug logging), and defaults to 1. Example:
```
[Global]
LOG_LEVEL=2
TIER_PERIOD=1000    # number of seconds between file move batches
```
The global config section can be placed before, after, or between tier definitions.
#### Tier Config
The layout of a single tier's configuration entry is as follows:
```
[<Tier name>]
DIR=/path/to/storage/tier
WATERMARK=<0-100% of tier usage at which to stop filling tier>
```
As many tiers as desired can be defined in the configuration, however they must be in order of fastest to slowest. The tier's name can be whatever you want but it cannot be `global` or `Global`. Tier names are only used for config diagnostics.  
Below is a complete example of a configuration file:
```
# autotier.conf
[Global]
LOG_LEVEL=1
TIER_PERIOD=1000    # number of seconds between file move batches

[Fastest Tier]
DIR=/tier_1         # fast tier storage pool
WATERMARK=70        # keep tier usage just below 70%

[Medium Tier]
DIR=/tier_2
WATERMARK=90

[Slower Tier]
DIR=/tier_3
WATERMARK=100

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
```
    🕯️
  ╒════╕
  │ 45 │
 ╒╧════╧╕
 │DRIVES│
╒╧══════╧╕
│AUTOTIER│
╘════════╛
```
[![45Drives Logo](https://www.45drives.com/img/45-drives-brand.png)](https://www.45drives.com)
