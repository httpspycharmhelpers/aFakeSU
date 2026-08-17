#!/bin/sh
# aFakeSU gen_ids.sh
# 通过 rish(UID 2000, Shizuku) 获取真实的安卓用户名表, 生成 .id.csv
# .id.csv 格式: 用户名,uid,gid
#
# 数据来源:
#   1. Android AID 表 (android_filesystem_config.h 中的真实用户名)
#   2. pm list users         -> 真实的多用户
#   3. pm list packages -U   -> 真实的已安装应用及其 uid

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
RISH="$SCRIPT_DIR/rish"
CSV="$SCRIPT_DIR/.id.csv"
TMP_CSV="$SCRIPT_DIR/.id.csv.tmp"

# ---------- 1. 基础 AID 表 (真实安卓用户名) ----------
cat > "$TMP_CSV" << 'AID'
root,0,0
system,1000,1000
radio,1001,1001
bluetooth,1002,1002
graphics,1003,1003
input,1004,1004
audio,1005,1005
camera,1006,1006
log,1007,1007
nfc,1008,1008
gps,1009,1009
wifi,1010,1010
adb,1011,1011
mount,1012,1012
media,1013,1013
dhcp,1014,1014
sdcard_rw,1015,1015
vpn,1016,1016
keystore,1017,1017
usb,1018,1018
media_codec,1019,1019
media_drm,1020,1020
drm,1021,1021
mdnsr,1022,1022
sdcard_r,1023,1023
clat,1024,1024
everybody,9997,9997
misc,9998,9998
nobody,9999,9999
shell,2000,2000
cache,2001,2001
diag,2002,2002
oem_288,3001,3001
net_bt_admin,3001,3001
net_bt,3002,3002
inet,3003,3003
net_raw,3004,3004
net_admin,3005,3005
net_bw_stats,3006,3006
net_bw_acct,3007,3007
net_bt_stack,3008,3008
readproc,3009,3009
wakelock,3010,3010
uhid,3011,3011
readtracefs,3012,3012
AID

# ---------- 2. 真实多用户 (pm list users) ----------
bash -c "sh '$RISH' -c 'pm list users'" 2>/dev/null | sed -n 's/^[[:space:]]*UserInfo{\([0-9]*\):\([^:]*\):.*/\1,\2/p' | while IFS= read -r line; do
    user_id="${line%%,*}"
    user_name="${line#*,}"
    # 每个 Android 用户有一个独立 uid 空间, 记作 @user<id>
    printf '@user%s,%s,%s\n' "$user_id" "$user_id" "$user_id" >> "$TMP_CSV"
done

# ---------- 3. 真实应用包 (pm list packages -U) ----------
bash -c "sh '$RISH' -c 'pm list packages -U'" 2>/dev/null | sed -n 's/^package:\([^ ]*\) uid:\([0-9,]*\)$/\1 \2/p' | while IFS=' ' read -r pkg uids; do
    # 每个 uid 对应一个用户, 第一个是 user0
    user_no=0
    echo "$uids" | tr ',' '\n' | while IFS= read -r one_uid; do
        [ -z "$one_uid" ] && continue
        # 每用户 uid 偏移 = userId * 100000
        user_id=$(( one_uid / 100000 ))
        if [ "$user_id" -eq 0 ]; then
            printf '%s,%s,%s\n' "$pkg" "$one_uid" "$one_uid" >> "$TMP_CSV"
        else
            printf '%s@user%s,%s,%s\n' "$pkg" "$user_id" "$one_uid" "$one_uid" >> "$TMP_CSV"
        fi
        user_no=$((user_no + 1))
    done
done

# ---------- 去重并生成 .id.csv ----------
sort -u -t, -k2,2n "$TMP_CSV" -o "$TMP_CSV"
cp "$TMP_CSV" "$CSV"
rm -f "$TMP_CSV"
echo "generated $CSV ($(wc -l < "$CSV") entries)"
