#!/bin/bash

echo "========================================="
echo "      atto miner Configurator      "
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

EXECUTABLE="./atto-miner-linux-x64"

cat <<EOF > run.sh
#!/bin/bash

EXECUTABLE="$EXECUTABLE"

if [ ! -f "\$EXECUTABLE" ]; then
    echo "Error: Executable '\$EXECUTABLE' not found."
    echo "Compile your C code first. Example:"
    echo "gcc -o atto-miner main.c sha1.c hal_linux.c -O3 -lpthread"
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
