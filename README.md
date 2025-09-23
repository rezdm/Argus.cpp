# Argus.cpp
I asked AI to translate my Java-based [Argus](https://github.com/rezdm/Argus) project -- simple network monitor from Java to C++.

## Further
I decided to continue this project as C++ version only

## Command line arguments
arguspp config.json                              # stdout logging
arguspp -l /tmp/debug.log config.json            # file logging
arguspp -d config.json                           # daemon + default log file
arguspp -d -l /custom/path.log config.json       # daemon + custom log file
arguspp -s config.json                           # systemd mode
arguspp -s -l /custom/path.log config.json       # systemd mode + custom log file

