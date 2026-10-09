#!/usr/bin/env bash
# run_local.sh - Build and run the orderflow engine locally
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

echo "=== OrderFlow Engine - Local Runner ==="

# Build
echo "[1/4] Building (Release)..."
cmake -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release -S "$ROOT" -q
cmake --build "$ROOT/build" --parallel "$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

# Run tests
echo "[2/4] Running tests..."
ctest --test-dir "$ROOT/build" --output-on-failure -q

# Check for ITCH data
ITCH_FILE="${1:-$ROOT/data/sample.itch}"
if [ ! -f "$ITCH_FILE" ]; then
  echo ""
  echo "[3/4] No ITCH file found at: $ITCH_FILE"
  echo "      Download from: https://emi.nasdaq.com/ITCH/Nasdaq%20ITCH/"
  echo "      Then run: gunzip <file>.gz && mv <file> data/sample.itch"
  echo ""
  echo "      Or pass a path: ./scripts/run_local.sh /path/to/file.itch"
  exit 1
fi

# Run engine
echo "[3/4] Running engine on: $ITCH_FILE"
"$ROOT/build/orderflow" "$ITCH_FILE"

# Copy to dashboard
echo "[4/4] Updating dashboard..."
cp "$ROOT/results.json" "$ROOT/dashboard/results.json"

echo ""
echo "Done. Open dashboard/index.html in your browser to see results."
