# Argus++

A high-performance C++ network monitoring daemon that provides real-time monitoring of network connectivity, services, and endpoints with a web-based dashboard.

Originally AI-translated from Java-based [Argus](https://github.com/rezdm/Argus), now developed as a standalone C++ project.

## Features

- **Multi-Protocol Monitoring**: ICMP ping, TCP/UDP port connectivity, HTTP/HTTPS endpoints
- **Configurable Ping Implementation**: Choose between system ping or unprivileged ICMP sockets
- **Web Dashboard**: Real-time status monitoring via HTTP interface
- **Flexible Deployment**: Run as foreground process, daemon, or systemd service
- **Robust Logging**: Configurable logging to stdout, files, or systemd journal
- **Security Hardened**: Input validation, shell injection protection
- **OpenSSL Support**: HTTPS monitoring with full TLS support

## Quick Start

### Build Requirements
- C++23 compatible compiler
- CMake 3.20+
- OpenSSL development libraries
- systemd development libraries (optional)

### Build
```bash
cmake -B build src
make -C build
```

### Run
```bash
# Copy and edit sample configuration
cp src/sample_config.json my_config.json

# Run with default settings
./build/bin/arguspp my_config.json
```

## Command Line Arguments

```bash
arguspp config.json                              # stdout logging
arguspp -l /tmp/debug.log config.json            # file logging
arguspp -d config.json                           # daemon + default log file
arguspp -d -l /custom/path.log config.json       # daemon + custom log file
arguspp -s config.json                           # systemd mode
arguspp -s -l /custom/path.log config.json       # systemd mode + custom log file
```

### Options
- `-d, --daemon`: Run as daemon (detach from terminal)
- `-s, --systemd`: Run in systemd mode (no fork, journal logging)
- `-l, --log-file <path>`: Log to specified file (overrides config/systemd settings)

## Configuration

### Basic Configuration
```json
{
  "name": "My Network Monitor",
  "listen": "127.0.0.1:8080",
  "log_file": "/var/log/arguspp.log",
  "ping_implementation": "system_ping",
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

## Ping Implementation Options

### System Ping (Default - Recommended)
```json
{
  "ping_implementation": "system_ping"
}
```

**Requirements:** None (uses system `ping` command)

### Unprivileged ICMP Sockets
```json
{
  "ping_implementation": "unprivileged_icmp"
}
```

**Requirements:** System configuration needed
```bash
# Enable unprivileged ICMP sockets (requires root)
sudo sysctl -w net.ipv4.ping_group_range="0 65535"

# Make persistent across reboots
echo 'net.ipv4.ping_group_range = 0 65535' | sudo tee -a /etc/sysctl.conf
```

**Note:** If unprivileged ICMP fails to initialize, arguspp will log a warning and the ping tests will fail. Consider using `system_ping` for production deployments.

## Deployment

### Systemd Service

1. **Install binary and config:**
```bash
sudo cp build/bin/arguspp /usr/local/bin/
sudo mkdir -p /etc/arguspp
sudo cp my_config.json /etc/arguspp/config.json
```

2. **Install service file:**
```bash
sudo cp arguspp.service /etc/systemd/system/
sudo systemctl daemon-reload
```

3. **Create user and directories:**
```bash
sudo useradd -r -s /bin/false argus
sudo mkdir -p /var/lib/arguspp /var/log
sudo chown argus:argus /var/lib/arguspp
```

4. **Enable and start:**
```bash
sudo systemctl enable arguspp
sudo systemctl start arguspp
```

5. **View logs:**
```bash
journalctl -u arguspp -f
```

