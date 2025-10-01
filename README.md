# Argus
A simple network monitoring daemon that provides a quick overview of service availability via ICMP ping, TCP/UDP ports, or HTTP/HTTPS. 

The original project was Java-based — [Argus](https://github.com/rezdm/Argus). I worked on it as a playground to explore Java 25 features, and at some point needed AI help, and then decided... why not translate it to C++, back to the roots, so to speak.

I liked the result of the translation and decided to continue with the C++-based project.

## Output
The program exposes an endpoint and serves a static (configurable) HTML page:
![argus-dashboard-screenshot.png](argus-dashboard-screenshot.png)

## Features
- **Multi-Protocol**: Ping (ICMP, raw sockets, and calling the ping utility; the homemade ping implementation is still suffering from some issues); TCP/UDP port connectivity (TCP connect, UDP by sending an empty packet), HTTP/HTTPS (sending GET requests; ignores certificates)
- **IPv4/IPv6**: I tried to make it work with both v4 and v6, but I have somewhat limited networks to truly test this
- **Web Dashboard**: Apart from the API endpoint, the program also serves HTML; the contents are configurable. Obviously, it's possible to place an HTML file with any contents on any other web server. One template (glass-tty.html) uses the Glass TTY VT220 font (public domain) embedded as base64
- **Deployment**: Foreground, background/daemonized, SysV, systemd, FreeBSD rc.d, Solaris SMF — I've done my best to support these
- **Logging**: Uses spdlog library. Default is stdout/stderr and /var/log/argus.log

## Quick Start
### Build Requirements
- C++ compiler — I tried to use certain C++23 features, but compilers have different support for this standard, so in practice, C++20 should be enough for now 
- CMake (no specific version requirements)
- OpenSSL, systemd development libraries — detected and linked during the build
- httplib, spdlog — downloaded and statically linked during the build process 

### Build
There are a couple of tricks during the build process

#### Just to build
```bash
cmake -S src -B build/ && cmake --build build/ -j$(nproc)
```

#### To run tests
There is only one set of tests — detecting if an address is IPv4 vs IPv6
```bash
cmake --build build/ -j$(nproc) --target check
```

### Run
Just run `./build/bin/argus` — the tool will print out the available command-line arguments. To try it out, the simplest way is to run something like:
```
./build/bin/argus config/example_config.json
```

## Command Line Arguments
```bash
argus config.json                              # stdout logging
argus -l /tmp/debug.log config.json            # file logging
argus -d config.json                           # daemon + default log file
argus -d -l /custom/path.log config.json       # daemon + custom log file
argus -s config.json                           # systemd mode
argus -s -l /custom/path.log config.json       # systemd mode + custom log file
```

## Configuration
### Configuration Hot-Reload
Note: Argus supports configuration hot-reload using the SIGHUP signal:
```bash
# Send SIGHUP signal to reload configuration
kill -SIGHUP <pid>
# or using systemctl for systemd services
sudo systemctl reload argus
```
When processing SIGHUP, Argus reloads both the configuration and HTML dashboard. 

### Basic Configuration
```json
{
  "name": "My Network Monitor",
  "listen": "127.0.0.1:8080",
  "log_file": "/var/log/argus.log",
  "cache_duration_seconds": 30,
  "thread_pool_size": 8,
  "monitors": [
    {
      "sort": 1,
      "group": "Critical Services",
      "destinations": [
        {
          "sort": 1,
          "name": "Google DNS",
          "timeout": 3000,
          "warning": 2,
          "failure": 3,
          "reset": 2,
          "interval": 60,
          "history": 100,
          "test": {
            "method": "Ping",
            "host": "8.8.8.8"
          }
        }
      ]
    }
  ]
}
```

### Configuration Options
#### Global Settings
- `name`: Display name for this monitor instance
- `listen`: Web interface bind address (IP:port or just port)
- `log_file`: Log file path (optional, overrides defaults)
- `ping_implementation`: Ping method - `"system_ping"` or `"unprivileged_icmp"`
- `cache_duration_seconds`: Web page cache duration in seconds (default: 30, set to 0 to disable caching)
- `thread_pool_size`: Number of worker threads for monitoring (default: 0 = auto-calculate based on hardware and monitor count)

#### Monitor Groups
- `sort`: Display order
- `group`: Group name for organization
- `destinations`: Array of monitoring targets

#### Destination Settings
- `name`: Display name for this destination
- `timeout`: Test timeout in milliseconds
- `warning`: Consecutive failures before warning status
- `failure`: Consecutive failures before failure status
- `reset`: Consecutive successes needed to reset from warning/failure
- `interval`: Test interval in seconds
- `history`: Number of results to keep in memory

#### Test Types
**ICMP Ping**
```json
{
  "method": "Ping",
  "host": "hostname_or_ip"
}
```

**TCP/UDP Port Check**
```json
{
  "method": "Connect",
  "protocol": "TCP",
  "host": "hostname_or_ip",
  "port": 80
}
```

**HTTP/HTTPS Endpoint**
```json
{
  "method": "URL",
  "url": "https://example.com/health"
}
```

## Challenges
Depending on the Linux/UNIX/BSD flavor, ping functionality might not work. ICMP or raw sockets require some sort of elevated rights:
```bash
# Enable unprivileged ICMP sockets (requires root)
sudo sysctl -w net.ipv4.ping_group_range="0 65535"
# Make persistent across reboots
echo 'net.ipv4.ping_group_range = 0 65535' | sudo tee -a /etc/sysctl.conf
# Allow raw sockets for argus
sudo setcap cap_net_raw+ep argus
```

## Deployment/Installation
I've tried to maintain installation guides in [SERVICES.md](SERVICES.md) and [HowToRun.md](HowToRun.md), but overall:
```bash
cmake --install
```
should do the trick depending on your environment. My personal preference is to install it in `/opt/argus` and maintain `etc/` and `bin/` directories there. Since the tool statically links with httplib and spdlog, this works well for me.