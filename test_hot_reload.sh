#!/bin/bash

# Test script for Argus++ configuration hot-reload functionality
# This script demonstrates how to use the SIGHUP signal to reload configuration

set -e

echo "=== Argus++ Hot-Reload Test Script ==="

# Check if arguspp binary exists
if [ ! -f "build-debug/bin/arguspp" ] && [ ! -f "build/bin/arguspp" ]; then
    echo "Error: arguspp binary not found. Please build the project first."
    echo "Run: ./build.sh"
    exit 1
fi

# Find the binary
BINARY=""
if [ -f "build-debug/bin/arguspp" ]; then
    BINARY="build-debug/bin/arguspp"
elif [ -f "build/bin/arguspp" ]; then
    BINARY="build/bin/arguspp"
fi

# Check if config file exists
if [ ! -f "config/example_config.json" ]; then
    echo "Error: Example config file not found at config/example_config.json"
    exit 1
fi

echo "Using binary: $BINARY"
echo "Using config: config/example_config.json"

# Copy example config to test config
cp config/example_config.json /tmp/test_config.json
echo "Created test config at /tmp/test_config.json"

# Start arguspp in background
echo "Starting Argus++ with test configuration..."
$BINARY /tmp/test_config.json &
ARGUS_PID=$!

echo "Argus++ started with PID: $ARGUS_PID"
echo "Waiting 5 seconds for startup..."
sleep 5

# Check if process is still running
if ! kill -0 $ARGUS_PID 2>/dev/null; then
    echo "Error: Argus++ process died during startup"
    exit 1
fi

echo "Argus++ is running successfully"

# Modify the configuration
echo "Modifying configuration (changing name)..."
sed -i 's/"name": ".*"/"name": "Hot-Reloaded Argus++ Monitor"/' /tmp/test_config.json

echo "Sending SIGHUP to trigger configuration reload..."
kill -SIGHUP $ARGUS_PID

echo "Waiting 3 seconds for reload to complete..."
sleep 3

# Check if process is still running after reload
if ! kill -0 $ARGUS_PID 2>/dev/null; then
    echo "Error: Argus++ process died during reload"
    exit 1
fi

echo "Configuration reload successful!"

# Test with invalid configuration
echo "Testing rollback with invalid configuration..."
echo "INVALID JSON" > /tmp/test_config_invalid.json
cp /tmp/test_config_invalid.json /tmp/test_config.json

kill -SIGHUP $ARGUS_PID
echo "Waiting 3 seconds for rollback..."
sleep 3

# Check if process is still running after failed reload
if ! kill -0 $ARGUS_PID 2>/dev/null; then
    echo "Error: Argus++ process died during failed reload"
    exit 1
fi

echo "Rollback test successful - process survived invalid config"

# Cleanup
echo "Stopping Argus++..."
kill -TERM $ARGUS_PID
sleep 2

# Force kill if still running
if kill -0 $ARGUS_PID 2>/dev/null; then
    echo "Force killing process..."
    kill -KILL $ARGUS_PID
fi

rm -f /tmp/test_config.json /tmp/test_config_invalid.json

echo "=== Hot-Reload Test Complete ==="
echo ""
echo "To use hot-reload in production:"
echo "1. Edit your configuration file"
echo "2. Send SIGHUP: kill -SIGHUP <pid>"
echo "3. Or with systemd: sudo systemctl reload arguspp"