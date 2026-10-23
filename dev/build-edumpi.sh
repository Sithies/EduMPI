#!/usr/bin/env bash
# Baut EduMPI (im Dev-Container oder nativ) und installiert nach $EDUMPI_PREFIX.
# bear erzeugt dabei compile_commands.json für clangd.
#   dev/build-edumpi.sh          # inkrementell
#   dev/build-edumpi.sh --clean  # autogen + configure neu
set -euo pipefail
cd "$(dirname "$0")/.."

if [[ "${1:-}" == "--clean" || ! -f Makefile ]]; then
    ./autogen.pl
    ./configure --prefix="$EDUMPI_PREFIX" \
        --enable-debug \
        --disable-sphinx \
        CC="ccache gcc" CXX="ccache g++" \
        CPPFLAGS="-DENABLE_ANALYSIS -I/usr/include/postgresql" \
        LIBS="-lpq"
fi

bear -- make -j"$(nproc)"
make install
