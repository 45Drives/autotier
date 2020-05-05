# autotier
Intelligently moves files between storage tiers based on frequency of use, file age, and tier fullness.

## What it does
`autotier` crawls through each tier's directory and queues up files, sorted by a combination of frequency of use and age. It fills the defined tiers to their watermarked capacity, starting at the fastest tier with the highest priority files, working its way down until no files are left. If you do a lot of writing, set a lower watermark for the highest tier to allow for more room. If you do mostly reading, set a higher watermark to allow for as much use as possible out of your available top tier storage.

## Installation
```
yum install https://github.com/45Drives/autotier/releases/download/v0.5-beta/autotier-0.5-1.el7.x86_64.rpm
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
The RPM install package includes a systemd unit and timer file. Configure `autotier` as described below and enable the daemon with `systemctl enable autotier.timer` The default configuration file is `/etc/autotier.conf`, but this can be changed by passing the `-c`/`--config` flag followed by the path to the alternate configuration file. The first defined tier should be the working tier that is exported. So far, `samba` is the only sharing tool that seems to work with this software. `nfs` is too literal, and has no capability of following wide symlinks.

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
WATERMARK=<0-100% of tier usage at which to stop filling tier>
```
As many tiers as desired can be defined in the configuration, however they must be in order of fastest to slowest. The tier's name can be whatever you want but it cannot be `global` or `Global`. Tier names are only used for config diagnostics.  
Below is a complete example of a configuration file:
```
# autotier.conf
[Global]
LOG_LEVEL=2

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
### Systemd Config
Edit `/etc/systemd/system/autotier.timer` to set the period at which to run `autotier`. The default period is every 30 minutes. Run `systemctl daemon-reload` after editing this file.
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
