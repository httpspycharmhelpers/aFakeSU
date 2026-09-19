#!/data/data/com.termux/files/usr/bin/bash
set -u

cd "$(dirname "$0")"
ROOT="$(pwd)"
PT="$ROOT/proot-termux"
TP="$ROOT/third_party"
BLD="$ROOT/build"
BASH_BIN="$ROOT/bash-5.3/bash"

mkdir -p "$BLD"

CC="clang"
COMMON="-O2 -fPIC -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DARG_MAX=131072"
PT_CFLAGS="$COMMON -DVERSION=\"5.1.0\" -DWITH_LIBANDROID_SHMEM -I$PT/src -I$PT"

echo "== talloc.o =="
$CC -c $COMMON \
    -DTALLOC_BUILD_VERSION_MAJOR=2 -DTALLOC_BUILD_VERSION_MINOR=4 -DTALLOC_BUILD_VERSION_RELEASE=3 \
    -I"$TP" -I"$TP/talloc-2.4.3" \
    "$TP/talloc-2.4.3/talloc.c" -o "$BLD/talloc.o" || exit 1

echo "== shmem.o =="
$CC -c $COMMON -I"$TP/libandroid-shmem-0.7" \
    "$TP/libandroid-shmem-0.7/shmem.c" -o "$BLD/shmem.o" || exit 1

echo "== cli.o (PROOT_EMBEDDED) =="
$CC -c $PT_CFLAGS -DPROOT_EMBEDDED \
    "$PT/src/cli/cli.c" -o "$BLD/cli.o" || exit 1

echo "== bash_data.o (objcopy) =="
cp "$BASH_BIN" "$BLD/bash.bin"
(cd "$BLD" && objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    bash.bin bash.bin.o) || exit 1

echo "== terhijack.bin (embedded hook engine, v1.2) =="
$CC -O2 -std=c99 -o "$BLD/terhijack.bin" "$ROOT/terhijack.c" || exit 1
(cd "$BLD" && objcopy -I binary -O elf64-littleaarch64 -B aarch64 \
    --rename-section .data=.rodata,alloc,load,readonly,data,contents \
    terhijack.bin terhijack.bin.o) || exit 1

echo "== su.o =="
$CC -c $COMMON -I"$PT" -I"$PT/src" \
    "$ROOT/su.c" -o "$BLD/su.o" || exit 1

echo "== link =="
OBJS=""
while IFS= read -r f; do
    case "$f" in
        *cli.o|*loader/loader.o|*loader/assembly.o|*loader-m32.o|*loader-m32-wrapped.o|*assembly-m32.o|*test_embed.o) continue ;;
    esac
    OBJS="$OBJS $f"
done < <(find "$PT/src" -name "*.o" | sort)

$CC -o "$ROOT/fakesu.elf" \
    "$BLD/cli.o" $OBJS "$BLD/talloc.o" "$BLD/shmem.o" "$BLD/su.o" "$BLD/bash.bin.o" "$BLD/terhijack.bin.o" \
    -Wl,-z,noexecstack -llog || exit 1

echo "== done: $ROOT/fakesu.elf =="
file "$ROOT/fakesu.elf"
mkdir -p /sdcard/termux 2>/dev/null
cp "$ROOT/fakesu.elf" /sdcard/termux/fakesu.elf 2>/dev/null
chmod 755 /sdcard/termux/fakesu.elf 2>/dev/null

SCRIPT_DIR="$ROOT/.."
cat > "$SCRIPT_DIR/afake-su" << 'SCRIPT'
#!/data/data/com.termux/files/usr/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
args=""
for arg in "$@"; do
    args="$args '$arg'"
done
"$SCRIPT_DIR/rish" -c "/sdcard/termux/fakesu.elf $args "
SCRIPT
chmod +x "$SCRIPT_DIR/afake-su"
echo "== generated: $SCRIPT_DIR/afake-su =="