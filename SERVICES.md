# Multi-Platform Service Management Guide

Argus++ supports multiple init systems and service managers across different Unix-like operating systems, with full support for custom installation paths.

## Supported Service Management Systems

| Platform | Service System | Configuration |
|----------|----------------|---------------|
| Linux (modern) | systemd | | `/etc/systemd/system/arguspp.service` |
| Linux (legacy) | SysV Init | `/etc/init.d/arguspp` |
| FreeBSD | rc.d | `/usr/local/etc/rc.d/arguspp` |
| OpenBSD/NetBSD | rc.d | `/etc/rc.d/arguspp` |
| Solaris | SMF | `/var/svc/manifest/network/arguspp.xml` |

## Installation

### Automatic Detection
The build system automatically detects your platform's service management system:

```bash
mkdir build && cd build
cmake ../src
make install
```

### Custom Installation Prefix
All service files support custom installation paths:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/opt/arguspp ../src
make install
```

## Platform-Specific Usage

### Linux with systemd

**Installation:**
```bash
# Service file: /etc/systemd/system/arguspp.service
sudo systemctl daemon-reload
```

**Service Management:**
```bash
# Enable auto-start
sudo systemctl enable arguspp

# Start service
sudo systemctl start arguspp

# Check status
sudo systemctl status arguspp

# View logs
journalctl -u arguspp -f

# Reload configuration
sudo systemctl reload arguspp

# Stop service
sudo systemctl stop arguspp
```

### Linux with SysV Init

**Installation:**
```bash
# Service file: /etc/init.d/arguspp
sudo chkconfig --add arguspp
```

**Service Management:**
```bash
# Enable auto-start
sudo chkconfig arguspp on

# Start service
sudo service arguspp start

# Check status
sudo service arguspp status

# Reload configuration
sudo service arguspp reload

# Stop service
sudo service arguspp stop
```

### FreeBSD

**Installation:**
```bash
# Service file: /usr/local/etc/rc.d/arguspp
# Automatically configured during installation
```

**Service Management:**
```bash
# Enable auto-start
sudo sysrc arguspp_enable="YES"

# Optional: Set custom config file
sudo sysrc arguspp_config="/usr/local/etc/arguspp/custom.json"

# Optional: Set custom log file
sudo sysrc arguspp_logfile="/var/log/arguspp/custom.log"

# Start service
sudo service arguspp start

# Check status
sudo service arguspp status

# Reload configuration
sudo service arguspp reload

# Stop service
sudo service arguspp stop
```

**Advanced FreeBSD Configuration:**
Add to `/etc/rc.conf`:
```bash
arguspp_enable="YES"
arguspp_user="argus"
arguspp_group="argus"
arguspp_config="/usr/local/etc/arguspp/config.json"
arguspp_logfile="/var/log/arguspp.log"
arguspp_flags=""  # Additional command line flags
```

### OpenBSD/NetBSD

**Installation:**
```bash
# Service file: /etc/rc.d/arguspp
# Automatically configured during installation
```

**Service Management:**
```bash
# Enable auto-start (add to /etc/rc.conf)
echo "arguspp=YES" | sudo tee -a /etc/rc.conf

# Start service
sudo /etc/rc.d/arguspp start

# Check status
sudo /etc/rc.d/arguspp status

# Reload configuration
sudo /etc/rc.d/arguspp reload

# Stop service
sudo /etc/rc.d/arguspp stop
```

### Solaris (SMF)

**Installation:**
```bash
# Service manifest: /var/svc/manifest/network/arguspp.xml
sudo svccfg import /var/svc/manifest/network/arguspp.xml
```

**Service Management:**
```bash
# Enable auto-start
sudo svcadm enable arguspp

# Start service immediately (temporary)
sudo svcadm enable -t arguspp

# Check status
svcs arguspp

# View detailed status
svcs -p arguspp

# View service logs
svcs -L arguspp

# Reload configuration
sudo svcadm refresh arguspp

# Restart service
sudo svcadm restart arguspp

# Disable service
sudo svcadm disable arguspp
```

**Advanced Solaris Configuration:**
```bash
# Modify service properties
sudo svccfg -s arguspp setprop argus/config_file = astring: "/opt/arguspp/etc/custom.json"
sudo svccfg -s arguspp setprop argus/log_file = astring: "/var/log/arguspp/custom.log"
sudo svccfg -s arguspp setprop argus/daemon_args = astring: "-d -v"

# Apply changes
sudo svcadm refresh arguspp
sudo svcadm restart arguspp
```

## Custom Installation Paths

### Standard Paths
```bash
Installation Prefix: /usr/local          (Linux/BSD) or /opt/arguspp (Solaris)
Executable: ${PREFIX}/bin/arguspp
Config: ${PREFIX}/etc/config.json
Templates: ${PREFIX}/share/templates/
Logs: /var/log/arguspp.log
Runtime: /var/run/arguspp.pid
```

### Custom Paths Example
```bash
# Install to /opt/monitoring/arguspp
cmake -DCMAKE_INSTALL_PREFIX=/opt/monitoring/arguspp ../src
make install

# All service files automatically use custom paths:
# - /opt/monitoring/arguspp/bin/arguspp
# - /opt/monitoring/arguspp/etc/config.json
# - /opt/monitoring/arguspp/share/templates/
```

### Configuration Override
Each service system supports configuration file override:

**Environment Variables:**
- `ARGUSPP_CONFIG`: Override config file path
- `ARGUSPP_LOG`: Override log file path
- `ARGUSPP_USER`: Override service user

**systemd:**
```bash
sudo systemctl edit arguspp
# Add:
# [Service]
# ExecStart=
# ExecStart=/custom/path/bin/arguspp /custom/config.json
```

**SysV Init:**
```bash
# Edit /etc/sysconfig/arguspp (RHEL/CentOS)
# or /etc/default/arguspp (Debian/Ubuntu)
ARGUSPP_CONFIG="/custom/path/config.json"
ARGUSPP_LOG="/custom/path/arguspp.log"
```

## Security Considerations

### User Permissions
All service files run Argus++ as the `argus` user for security:
- Creates dedicated system user during installation
- Minimal privileges (no shell, locked account)
- Proper file ownership and permissions

### Network Capabilities
For ICMP ping functionality:

**Linux:**
```bash
# Option 1: Set capabilities (preferred)
sudo setcap cap_net_raw+ep /path/to/arguspp

# Option 2: Enable unprivileged ICMP
sudo sysctl -w net.ipv4.ping_group_range="0 65535"
echo 'net.ipv4.ping_group_range = 0 65535' | sudo tee -a /etc/sysctl.conf
```

**FreeBSD:**
```bash
# Enable unprivileged ICMP (if needed)
sudo sysctl net.inet.icmp.icmplim_output=0
echo 'net.inet.icmp.icmplim_output=0' >> /etc/sysctl.conf
```

**Solaris:**
```bash
# Usually works out of the box with proper user privileges
# May need to adjust network privileges if required
```

## Troubleshooting

### Service Won't Start
1. Check configuration file exists and is readable
2. Verify user permissions and ownership
3. Check system logs for detailed error messages
4. Ensure network capabilities are set (for ping tests)

### Permission Issues
```bash
# Fix ownership
sudo chown -R argus:argus /path/to/installation
sudo chown -R argus:argus /var/log/arguspp
sudo chown -R argus:argus /var/lib/arguspp

# Fix permissions
sudo chmod 755 /path/to/installation/bin/arguspp
sudo chmod 644 /path/to/installation/etc/config.json
```

### Service Detection Issues
Force specific service type during build:
```bash
# Force systemd
cmake -DSYSTEMD_FOUND=ON ../src

# Force SysV (disable systemd detection)
cmake -DSYSTEMD_FOUND=OFF ../src
```

## Migration Between Systems

When moving between different init systems, the configuration and data remain compatible. Only the service management commands change. The core Argus++ daemon and its configuration files work identically across all platforms.

## Custom Service Integration

For systems not covered by the standard service files, you can create custom integration using the manual start command:

```bash
# Manual start with custom paths
/custom/path/bin/arguspp -d -l /custom/log/path.log /custom/config/path.json
```

All service files are templates and can be customized for specific deployment requirements.
