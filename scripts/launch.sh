#!/bin/bash
# Launch N Chord nodes as separate processes using IPC transport.
#
# Usage:
#   ./scripts/launch.sh <num_nodes>
#   ./scripts/launch.sh 5
#
# The first node creates the ring, the rest join via node 1.
# All nodes run in the background. Use stop.sh to shut them down.

set -e

NUM_NODES=${1:-5}
BINARY="./build/chord_node"
BOOTSTRAP_PATH="/tmp/chord_node_1.sock"
PID_FILE="/tmp/chord_pids.txt"

if [ ! -f "$BINARY" ]; then
    echo "Error: $BINARY not found. Run 'make' in the build directory first."
    exit 1
fi

for i in $(seq 1 $NUM_NODES); do
    rm -f "/tmp/chord_node_${i}.sock"
done
rm -f "$PID_FILE"

echo "Starting $NUM_NODES Chord nodes..."

echo "  Node 1: creating ring..."
$BINARY ipc --id 1 &
echo $! >> "$PID_FILE"

sleep 1

for i in $(seq 2 $NUM_NODES); do
    echo "  Node $i: joining via $BOOTSTRAP_PATH..."
    $BINARY ipc --id $i --bootstrap $BOOTSTRAP_PATH &
    echo $! >> "$PID_FILE"
    sleep 1
done

echo ""
echo "All $NUM_NODES nodes started. PIDs saved to $PID_FILE"
echo "Run './scripts/stop.sh' to shut them down."
echo "Or: kill \$(cat $PID_FILE)"