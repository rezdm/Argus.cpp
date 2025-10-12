# Argus
A network monitoring daemon that provides a quick overview of service availability via ICMP ping, TCP/UDP ports, or HTTP/HTTPS. 

The original project was Java-based — [Argus](https://github.com/rezdm/Argus). I worked on it as a playground to explore Java 25 features, and at some point needed AI help, and then decided... why not translate it to C++, back to the roots, so to speak.

I liked the result of the translation and decided to continue with the C++-based project.

## Output
The program exposes an endpoint and serves a static (configurable) HTML page:
![argus-dashboard-screenshot.png](argus-dashboard-screenshot.png)

## Build
* Generate build and prepare for installation
```
cmake -S src/ -B build/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/argus
```
* Compile, link
```
cmake --build build/ -j$(nproc)
```
* Run tests
```
cmake --build build/ --target check
```
## Install 
```
sudo cmake --install build/
```
On Linux, depending no distribution, additional step is needed to enable unprivileged ICMP sockets (better solution):
```
sudo sysctl -w net.ipv4.ping_group_range="0 65535"
```
Make it permanent
```
echo 'net.ipv4.ping_group_range = 0 65535' | sudo tee -a /etc/sysctl.conf
```
Or
```bash
sudo setcap cap_net_raw+ep ./build/bin/argus
```

(installation scripts should take care of this, but not tested)

Install should also run on SysV, init.rc and Solaris SMF

## Uninstall
```
sudo cmake -S src/ -B build/ -DCMAKE_BUILD_TYPE=Release
sudo cmake --build build/ --target uninstall
```

## Run with systemd
```
sudo systemctl enable argus
sudo systemctl start argus
```

Monitor and restart
```
sudo journalctl -fu argus # follow logs
kill -SIGHUP $(pidof argus) - Hot-reload config
```

## Run manually
```bash
argus argus.json                               # stdout logging
argus -l /tmp/debug.log argus.json             # file logging
argus -d argus.json                            # daemon + default log file
argus -d -l /custom/path.log argus.json        # daemon + custom log file
argus -s argus.json                            # systemd mode
argus -s -l /custom/path.log argus.json        # systemd mode + custom log file
```

## PWA + notifications
* Generate VAPID keys
public
private
add them to back-end config
change pwa/app.js
* I am using fronting Apache with reverse proxy:
```xml
<VirtualHost *:443>
    ServerName ...
    DocumentRoot ...

    SSLEngine on
    SSLCertificateFile "....crt"
    SSLCertificateKeyFile "....key"

    ProxyPass /argus http://192.168.100.97:8080/argus
    ProxyPassReverse /argus http://192.168.100.97:8080/argus

    ProxyPass /argus.pwa http://192.168.100.32:8080
    ProxyPassReverse /argus.pwa http://192.168.100.32:8080   
</VirtualHost>
```
