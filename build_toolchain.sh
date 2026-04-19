#!/bin/bash
set -e

# 설정
export TARGET=x86_64-elf
export PREFIX="$(pwd)/toolchain/install"
export PATH="$PREFIX/bin:$PATH"
export JOBS=$(nproc)

mkdir -p toolchain/src
cd toolchain/src

# 1. 소스 다운로드
if [ ! -d "binutils-2.42" ]; then
    echo "Downloading binutils..."
    curl -O https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.gz
    tar -xzf binutils-2.42.tar.gz
fi

if [ ! -d "gcc-13.2.0" ]; then
    echo "Downloading gcc..."
    curl -O https://ftp.gnu.org/gnu/gcc/gcc-13.2.0/gcc-13.2.0.tar.gz
    tar -xzf gcc-13.2.0.tar.gz
    cd gcc-13.2.0
    ./contrib/download_prerequisites
    cd ..
fi

if [ ! -d "newlib-4.4.0.20231231" ]; then
    echo "Downloading newlib..."
    curl -O ftp://sourceware.org/pub/newlib/newlib-4.4.0.20231231.tar.gz
    tar -xzf newlib-4.4.0.20231231.tar.gz
fi

# 2. Binutils 빌드
echo "Building binutils..."
mkdir -p build-binutils
cd build-binutils
../binutils-2.42/configure --target=$TARGET --prefix="$PREFIX" --with-sysroot --disable-nls --disable-werror
make -j$JOBS
make install
cd ..

# 3. GCC (Core) 빌드
echo "Building core gcc..."
mkdir -p build-gcc
cd build-gcc
../gcc-13.2.0/configure --target=$TARGET --prefix="$PREFIX" --disable-nls --enable-languages=c --without-headers
make -j$JOBS all-gcc
make -j$JOBS all-target-libgcc
make install-gcc
make install-target-libgcc
cd ..

# 4. Newlib 빌드
echo "Building newlib..."
mkdir -p build-newlib
cd build-newlib
../newlib-4.4.0.20231231/configure --target=$TARGET --prefix="$PREFIX"
make -j$JOBS
make install
cd ..

# 5. 최종 GCC (with Newlib) 빌드 - 필요 시 전체 빌드
# (여기서는 Core GCC로도 충분할 수 있으나, 표준을 위해 전체 빌드를 권장함)
echo "Toolchain build complete!"
echo "Add this to your PATH: $PREFIX/bin"
