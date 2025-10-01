# Multi-Platform Service Management Guide

Argus supports multiple init systems and service managers across different Unix-like operating systems, with full support for custom installation paths.

## Supported Service Management Systems

| Platform | Service System | Configuration |
|----------|----------------|---------------|
| Linux (modern) | systemd | | `/etc/systemd/system/argus.service` |
| Linux (legacy) | SysV Init | `/etc/init.d/argus` |
| FreeBSD | rc.d | `/usr/local/etc/rc.d/argus` |
| OpenBSD/NetBSD | rc.d | `/etc/rc.d/argus` |
| Solaris | SMF | `/var/svc/manifest/network/argus.xml` |

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
cmake -DCMAKE_INSTALL_PREFIX=/opt/argus ../src
make install
```

## Platform-Specific Usage

### Linux with systemd

**Installation:**
```bash
# Service file: /etc/systemd/system/argus.service
sudo systemctl daemon-reload
```

**Service Management:**
```bash
# Enable auto-start
sudo systemctl enable argus

# Start service
sudo systemctl start argus

# Check status
sudo systemctl status argus

# View logs
journalctl -u argus -f

# Reload configuration
sudo systemctl reload argus

# Stop service
sudo systemctl stop argus
```

### Linux with SysV Init

**Installation:**
```bash
# Service file: /etc/init.d/argus
sudo chkconfig --add argus
```

**Service Management:**
```bash
# Enable auto-start
sudo chkconfig argus on

# Start service
sudo service argus start

# Check status
sudo service argus status

# Reload configuration
sudo service argus reload

# Stop service
sudo service argus stop
```

### FreeBSD

**Installation:**
```bash
# Service file: /usr/local/etc/rc.d/argus
# Automatically configured during installation
```

**Service Management:**
```bash
# Enable auto-start
sudo sysrc argus_enable="YES"

# Optional: Set custom config file
sudo sysrc argus_config="/usr/local/etc/argus/argus.json"

# Optional: Set custom log file
sudo sysrc argus_logfile="/var/log/argus/custom.log"

# Start service
sudo service argus start

# Check status
sudo service argus status

# Reload configuration
sudo service argus reload

# Stop service
sudo service argus stop
```

**Advanced FreeBSD Configuration:**
Add to `/etc/rc.conf`:
```bash
argus_enable="YES"
argus_user="argus"
argus_group="argus"
argus_config="/usr/local/etc/argus/argus.json"
argus_logfile="/var/log/argus.log"
argus_flags=""  # Additional command line flags
```

### OpenBSD/NetBSD

**Installation:**
```bash
# Service file: /etc/rc.d/argus
# Automatically configured during installation
```

**Service Management:**
```bash
# Enable auto-start (add to /etc/rc.conf)
echo "argus=YES" | sudo tee -a /etc/rc.conf

# Start service
sudo /etc/rc.d/argus start

# Check status
sudo /etc/rc.d/argus status

# Reload configuration
sudo /etc/rc.d/argus reload

# Stop service
sudo /etc/rc.d/argus stop
```

### Solaris (SMF)

**Installation:**
```bash
# Service manifest: /var/svc/manifest/network/argus.xml
sudo svccfg import /var/svc/manifest/network/argus.xml
```

**Service Management:**
```bash
# Enable auto-start
sudo svcadm enable argus

# Start service immediately (temporary)
sudo svcadm enable -t argus

# Check status
svcs argus

# View detailed status
svcs -p argus

# View service logs
svcs -L argus

# Reload configuration
sudo svcadm refresh argus

# Restart service
sudo svcadm restart argus

# Disable service
sudo svcadm disable argus
```

**Advanced Solaris Configuration:**
```bash
# Modify service properties
sudo svccfg -s argus setprop argus/config_file = astring: "/opt/argus/etc/custom.json"
sudo svccfg -s argus setprop argus/log_file = astring: "/var/log/argus/custom.log"
sudo svccfg -s argus setprop argus/daemon_args = astring: "-d -v"

# Apply changes
sudo svcadm refresh argus
sudo svcadm restart argus
```

## Custom Installation Paths

### Standard Paths
```bash
Installation Prefix: /usr/local          (Linux/BSD) or /opt/argus (Solaris)
Executable: ${PREFIX}/bin/argus
Config: ${PREFIX}/etc/argus.json
Templates: ${PREFIX}/share/templates/
Logs: /var/log/argus.log
Runtime: /var/run/argus.pid
```

### Custom Paths Example
```bash
# Install to /opt/monitoring/argus
cmake -DCMAKE_INSTALL_PREFIX=/opt/monitoring/argus ../src
make install

# All service files automatically use custom paths:
# - /opt/monitoring/argus/bin/argus
# - /opt/monitoring/argus/etc/argus.json
# - /opt/monitoring/argus/share/templates/
```

### Configuration Override
Each service system supports configuration file override:

**Environment Variables:**
- `ARGUS_CONFIG`: Override config file path
- `ARGUS_LOG`: Override log file path
- `ARGUS_USER`: Override service user

**systemd:**
```bash
sudo systemctl edit argus
# Add:
# [Service]
# ExecStart=
# ExecStart=/custom/path/bin/argus /custom/config.json
```

**SysV Init:**
```bash
# Edit /etc/sysconfig/argus (RHEL/CentOS)
# or /etc/default/argus (Debian/Ubuntu)
ARGUS_CONFIG="/custom/path/config.json"
ARGUS_LOG="/custom/path/argus.log"
```

## Security Considerations

### User Permissions
All service files run Argus as the `argus` user for security:
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
sudo chown -R argus:argus /var/log/argus
sudo chown -R argus:argus /var/lib/argus

# Fix permissions
sudo chmod 755 /path/to/installation/bin/argus
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

When moving between different init systems, the configuration and data remain compatible. Only the service management commands change. The core Argus daemon and its configuration files work identically across all platforms.

## Custom Service Integration

For systems not covered by the standard service files, you can create custom integration using the manual start command:

```bash
# Manual start with custom paths
/custom/path/bin/argus -d -l /custom/log/path.log /custom/config/argus.json
```

All service files are templates and can be customized for specific deployment requirements.
