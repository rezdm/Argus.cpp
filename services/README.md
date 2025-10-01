# Service Management Files

This directory contains service/init script templates for different Unix-like operating systems and service management systems.

## Directory Structure

```
services/
├── systemd/           # Linux systemd
│   └── argus.service.in
├── sysv/              # Linux SysV init
│   └── argus.init.in
├── freebsd/           # FreeBSD rc.d (also works for OpenBSD/NetBSD)
│   └── argus.rc.in
├── solaris/           # Solaris Service Management Facility (SMF)
│   └── argus.xml.in
└── README.md          # This file
```

## Template Variables

All service files use CMake template variables that are automatically substituted during the build process:

- `@CMAKE_INSTALL_PREFIX@` - Installation directory (e.g., `/usr/local`, `/opt/argus`)
- `@CMAKE_INSTALL_PREFIX@/bin/argus` - Executable path
- `@CMAKE_INSTALL_PREFIX@/etc/argus.json` - Default configuration file path
- `@CMAKE_INSTALL_PREFIX@/share/templates/` - Web templates directory

## Automatic Selection

The CMake build system automatically selects the appropriate service file based on:

1. **Platform detection** via `CMAKE_SYSTEM_NAME`
2. **Service system availability** (e.g., systemd presence on Linux)
3. **Fallback hierarchy** (systemd → SysV on Linux)

## Manual Selection

You can override automatic detection by setting CMake variables:

```bash
# Force systemd (even if not detected)
cmake -DSYSTEMD_FOUND=ON ../src

# Disable systemd (force SysV fallback)
cmake -DSYSTEMD_FOUND=OFF ../src
```

## Custom Installation Paths

All service files support custom installation paths:

```bash
# Install to custom location
cmake -DCMAKE_INSTALL_PREFIX=/opt/monitoring/argus ../src
make install

# Service files automatically use custom paths:
# /opt/monitoring/argus/bin/argus
# /opt/monitoring/argus/etc/argus.json
```

## Service File Destinations

During installation, service files are copied to platform-specific locations:

| Platform | Service System | Destination |
|----------|----------------|-------------|
| Linux | systemd | `/etc/systemd/system/argus.service` |
| Linux | SysV | `/etc/init.d/argus` |
| FreeBSD | rc.d | `/usr/local/etc/rc.d/argus` |
| OpenBSD/NetBSD | rc.d | `/etc/rc.d/argus` |
| Solaris | SMF | `/var/svc/manifest/network/argus.xml` |

## Service User

All service files run Argus as the `argus` system user, which is automatically created during installation with:

- No login shell (`/bin/false`)
- System user (no home directory login)
- Member of appropriate groups for network access
- Minimal privileges for security

## See Also

- `../SERVICES.md` - Comprehensive service management guide
- `../README.md` - General installation and usage instructions