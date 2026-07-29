#!/bin/bash

echo "========================================="
echo "      atto miner Configurator      "
echo "========================================="
echo


ARCH=$(uname -m)
case "$ARCH" in
    x86_64)        SUFFIX="x86_64" ;;
    aarch64)       SUFFIX="aarch64" ;;
    armv7l|armv6l) SUFFIX="armv7" ;;
    i686|i386)     SUFFIX="x86" ;;
    *)             SUFFIX="$ARCH" ;;
esac

EXPECTED_EXE="./atto-miner-linux-$SUFFIX"

if [ -f "$EXPECTED_EXE" ]; then
    EXECUTABLE="$EXPECTED_EXE"
else

    BINS=(./atto-miner-linux-*)
    if [ ${#BINS[@]} -eq 1 ] && [ -f "${BINS[0]}" ]; then

        EXECUTABLE="${BINS[0]}"
    else

        echo "Auto-detection failed or multiple binaries found."
        echo "Available executables in this folder:"
        ls -1 atto-miner-linux-* 2>/dev/null || echo "  [None found! Compile the C code first.]"
        echo
        read -p "Enter the exact name of the executable to use (e.g., atto-miner-linux-custom): " CUSTOM_EXE
        EXECUTABLE="./$CUSTOM_EXE"
    fi
fi

echo ">> Detected architecture: $ARCH"
echo ">> Selected executable: $EXECUTABLE"
echo "========================================="
echo

read -p "Enter your Duino-Coin username: " USERNAME
read -s -p "Enter your mining key (password): " PASSWORD
echo
echo
read -p "Enter Rig identifier [Default: atto-rig]: " RIG_ID
RIG_ID=${RIG_ID:-atto-rig}

read -p "Difficulty (LOW/MEDIUM/NET) [Default: MEDIUM]: " DIFFICULTY
DIFFICULTY=${DIFFICULTY:-MEDIUM}
case "$DIFFICULTY" in
    LOW|MEDIUM|NET) ;;
    *) DIFFICULTY=MEDIUM ;;
esac

read -p "Enter number of threads (1 to 128) [Default: 1]: " THREADS
THREADS=${THREADS:-1}


cat <<EOF > run.sh
#!/bin/bash

EXECUTABLE="$EXECUTABLE"

if [ ! -f "\$EXECUTABLE" ]; then
    echo "Error: Executable '\$EXECUTABLE' not found."
    echo "Compile your C code first. Example:"
    echo "gcc -o \$EXECUTABLE main.c sha1.c hal_linux.c -O3 -lpthread"
    exit 1
fi

echo "Starting miner for user '$USERNAME' on rig '$RIG_ID' with $THREADS thread(s) at $DIFFICULTY difficulty..."

echo -e "$RIG_ID\n$DIFFICULTY\n$THREADS" | \$EXECUTABLE "$USERNAME" "$PASSWORD"
EOF

chmod +x run.sh

echo
echo "========================================="
echo "Success! 'run.sh' has been created."
echo "To start mining, run:"
echo "  ./run.sh"
echo "========================================="
