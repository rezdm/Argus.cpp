# Build
Regular build
```
cmake -S src/ -B build-release/ -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/argus
```

With tests
```
rm -rf build && cmake -S src -B build/ && cmake --build build/ -j$(nproc) && cmake --build build/ --target check

```

# Install
```
sudo cmake --install build-release/
```

# If using Ping tests and running from under systemd and systemd does not provide capabilities
Enable unprivileged ICMP sockets (better solution)
```
sudo sysctl -w net.ipv4.ping_group_range="0 65535"
```
Make it permanent
```
echo 'net.ipv4.ping_group_range = 0 65535' | sudo tee -a /etc/sysctl.conf
```

# Run
```
sudo systemctl enable arguspp
sudo systemctl start arguspp
```

#Monitor and restart
```
sudo journalctl -fu arguspp # follow logs
kill -SIGHUP $(pidof arguspp) - Hot-reload config
```

# Uninstall
```
sudo cmake -S src/ -B build-release/ -DCMAKE_BUILD_TYPE=Release
sudo cmake --build build-release/ --target uninstall
```

