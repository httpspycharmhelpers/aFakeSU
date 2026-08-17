#!/data/data/com.termux/files/usr/bin/bash
# 恢复 Termux 风格的软链接结构

cd "$(dirname "$0")/lib" || exit 1

# bash 5.3 的 [[ =~ ]] 对正则里的 '->' 有解析 bug, 正则必须放变量里
re='^\./(.+)[[:space:]]+->[[:space:]]+(.+)$'

while IFS= read -r line; do
    # 解析格式：./libxxx.so -> libyyy.so.zzz
    if [[ "$line" =~ $re ]]; then
        src="${BASH_REMATCH[1]}"
        target="${BASH_REMATCH[2]}"
        # 创建软链接
        ln -sf "$target" "$src"
    fi
done < ../symlinks_list.txt
