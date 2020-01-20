#!/bin/bash
set -euo pipefail

base=$(basename "$1")
base="${base%.*}"

mkdir "$base"
ffmpeg -i "$1" "$base/%04d.png"

echo "wrote to $base"
