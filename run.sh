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

if [ ! -f "$DYLIB_PATH" ]; then
    echo "ERROR: Dylib not found at: $DYLIB_PATH"
    echo "Build the project first via GitHub Actions or locally in build/"
    exit 1
fi

echo "[*] Dylib: $DYLIB_PATH"

if [ $# -eq 0 ]; then
    JAVA_CMD=(java -jar "Minecraft.jar")
elif [ $# -eq 1 ] && [[ "$1" == *.jar ]]; then
    JAVA_CMD=(java -jar "$1")
else
    JAVA_CMD=(java "$@")
fi

echo "[*] Target command: ${JAVA_CMD[*]}"
echo "[*] Launching with DYLD_INSERT_LIBRARIES..."

DYLD_INSERT_LIBRARIES="$DYLIB_PATH" "${JAVA_CMD[@]}"
