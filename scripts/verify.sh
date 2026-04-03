#!/bin/bash
# Verify the Chord ring by checking that all socket files exist
# and all processes are still running.

PID_FILE="/tmp/chord_pids.txt"
NUM_NODES=${1:-5}

echo "Verifying Chord ring ($NUM_NODES nodes)..."

missing=0
for i in $(seq 1 $NUM_NODES); do
    sock="/tmp/chord_node_${i}.sock"
    if [ ! -S "$sock" ]; then
        echo "  MISSING: $sock"
        missing=$((missing + 1))
    fi
done

if [ $missing -eq 0 ]; then
    echo "  All $NUM_NODES socket files present."
else
    echo "  $missing socket files missing!"
fi

# Check processes
if [ ! -f "$PID_FILE" ]; then
    echo "  No PID file found."
    exit 1
fi

dead=0
alive=0
while read pid; do
    if kill -0 "$pid" 2>/dev/null; then
        alive=$((alive + 1))
    else
        echo "  DEAD: PID $pid"
        dead=$((dead + 1))
    fi
done < "$PID_FILE"

echo "  $alive processes alive, $dead dead."

if [ $missing -eq 0 ] && [ $dead -eq 0 ]; then
    echo "Ring looks healthy!"
else
    echo "Ring has issues."
    exit 1
fi