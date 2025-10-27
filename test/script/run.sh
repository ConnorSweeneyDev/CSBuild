#!/bin/bash

executable=$(find build -name "test" -type f 2>/dev/null | head -n 1)
if [ -z "$executable" ]; then
  echo "Executable not found. Please run the build script first."
  exit 1
fi
echo "Running: $executable"
"$executable"
