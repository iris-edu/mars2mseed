
mars2mseed - convert MARS 88/lite waveform data to Mini-SEED.

**All data samples are scaled by default during this conversion, do not forget
to take this into account when using the resulting Mini-SEED.**

## Acknowledgements

Aladino Govoni provided most of the base code for reading and parsing
MARS data blocks and was very helpful, many thanks.

## Documentation

For usage infromation see the [mars2mseed manual](doc/mars2mseed.md) in the
'doc' directory.

## Downloading and building

The [releases](https://github.com/iris-edu/mars2mseed/releases) area
contains release versions.

In most Unix/Linux environments a simple 'make' will build the program.

The CC and CFLAGS environment variables can be used to configure
the build parameters.

In the Win32 environment the Makefile.win can be used with the nmake
build tool included with Visual Studio.

## Licensing

GNU GPL version 3.  See included LICENSE file for details.
