#!/bin/bash
# Stop all Chord nodes launched by launch.sh

PID_FILE="/tmp/chord_pids.txt"

if [ ! -f "$PID_FILE" ]; then
    echo "No PID file found. Nothing to stop."
    exit 0
fi

echo "Stopping Chord nodes..."
while read pid; do
    if kill -0 "$pid" 2>/dev/null; then
        kill "$pid"
        echo "  Stopped PID $pid"
    fi
done < "$PID_FILE"

rm -f "$PID_FILE"

rm -f /tmp/chord_node_*.sock

echo "All nodes stopped."