#!/bin/bash

mkdir -p build
current_dir="$(pwd)"
cat > build/compile_commands.json << EOF
[
  {
    "directory": "$current_dir",
    "file": "$current_dir/csb.hpp",
    "command": "clang++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow-all -Wundef -Wdeprecated -Wtype-limits -Wcast-qual -Wcast-align -Wfloat-equal -Wunreachable-code-aggressive -Wformat=2 -D__linux__ -c \"$current_dir/csb.hpp\" -o \"build/csb.o\""
  }
]
EOF

clang_version="22.1.8"
clang_architecture="X64"
if [ "$(uname -m)" = "aarch64" ]; then clang_architecture="ARM64"; fi
current_version=""
if [ -x build/clang/clang ]; then
  current_version="$(./build/clang/clang --version | grep -m 1 'clang version' | awk '{print $3}')"
fi
if [ "$current_version" != "$clang_version" ]; then
  rm -rf build/clang
  archive="LLVM-$clang_version-Linux-$clang_architecture"
  url="https://github.com/llvm/llvm-project/releases/download/llvmorg-$clang_version/$archive.tar.xz"
  echo "Downloading archive at '$url'..."
  curl -f -L -C - -o build/clang.tar.xz "$url" || { echo "Failed to download clang archive."; exit 1; }
  echo "Extracting archive to 'build/clang'..."
  tar -xf build/clang.tar.xz -C build || { echo "Failed to extract clang archive."; exit 1; }
  rm build/clang.tar.xz
  if [ ! -d "build/$archive" ]; then
    echo "Failed to find build/$archive."
    exit 1
  fi
  mkdir -p build/clang
  mv "build/$archive/bin/"* build/clang/ || { echo "Failed to move clang binaries."; exit 1; }
  rm -rf "build/$archive"
fi

printf "Formatting csb.hpp... "
marker_line="$(grep -n -m 1 '^// CSB ' csb.hpp | cut -d : -f 1)"
if [ -z "$marker_line" ]; then
  echo "Failed to extract the CSB section of csb.hpp."
  exit 1
fi
tail -n +"$marker_line" csb.hpp > build/formatting.hpp || { echo "Failed to extract the CSB section of csb.hpp."; exit 1; }
./build/clang/clang-format -i build/formatting.hpp || { echo "Failed to format csb.hpp."; exit 1; }
head -n "$((marker_line - 1))" csb.hpp > build/csb.hpp.head || { echo "Failed to write the formatted section back to csb.hpp."; exit 1; }
cat build/csb.hpp.head build/formatting.hpp > csb.hpp || { echo "Failed to write the formatted section back to csb.hpp."; exit 1; }
rm build/csb.hpp.head build/formatting.hpp
echo "done."
