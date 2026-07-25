#!/bin/bash
# macOS launcher — loads the payload dylib into Minecraft via DYLD_INSERT_LIBRARIES.
#
# Usage:
#   ./run.sh [path/to/Minecraft.jar]
#
# Requirements:
#   - Third-party JDK (Oracle, Adoptium, etc.) — NOT Apple's /usr/bin/java
#   - SIP must be disabled OR the JDK must not be Apple-signed
#   - Build the project first: mkdir build && cd build && cmake .. && make

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
DYLIB_PATH="${SCRIPT_DIR}/build/lib/libmcpayload.dylib"
JAR_PATH="${1:-Minecraft.jar}"

if [ ! -f "$DYLIB_PATH" ]; then
    echo "ERROR: Dylib not found at: $DYLIB_PATH"
    echo "Build the project first:"
    echo "  mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

echo "[*] Dylib: $DYLIB_PATH"
echo "[*] Target: java -jar $JAR_PATH"
echo "[*] Launching with DYLD_INSERT_LIBRARIES..."

DYLD_INSERT_LIBRARIES="$DYLIB_PATH" java -jar "$JAR_PATH"
