## aFakeSU

面向非 root 设备的伪 `su` / 伪 root 环境。基于 proot-termux 的 syscall 拦截与 terhijack 会话上下文,让需要 root 的脚本在本机跑通,同时尽量不给宿主留下 root 痕迹。本质仍是用户态伪造,**骗不过深度检测,仅供娱乐**。

## 能做什么

| 能力 | 说明 |
|------|------|
| `su -c 'cmd'` | 以 root 身份执行命令(uid/gid=0,workdir 内 `id` 显示 root) |
| 只读属性改写 | `getprop/setprop/resetprop` 走 workdir 包装器,键值持久化在 `.props` |
| 分区覆盖写 | `/system`、`/vendor`、`/product`、`/odm`、`/` 的写操作重定向到 `workdir/ov/`,stat 伪装 root:root、真实 mode/size |
| `mount -o remount,rw` | 接受并记忆 rw/ro 状态,`mount` 输出按需改写 |
| `chcon` / `restorecon` | 让 `ls -Z` 显示指定标签(存 `.xattrs/`,数量、非覆盖路径查询走真实 xattr) |
| SELinux 上下文 | `id -Z`→`u:r:shell:s0`/自定义,`ps -Z`/`ls -Z`/`getenforce` 可控 |

所有包装器在首次启动时生成到 `workdir/bin/(getprop|setprop|resetprop|mount|chcon|restorecon)`,`workdir/.mnt/` 里的标记控制覆盖层开关。

## 目录结构

```
aFakeSU/
└--src/
   ├--su.c                 # 主程序(fakesu + proot 启动参数 + 包装器生成)
   ├--build_su.sh          # 只编译 su.c 并链接(enter.o/exit.o 见下)
   ├--fakesu.elf           # 产物(ARM64 PIE,Android 24+)
   ├--terhijack            # 会话上下文/ -Z 输出替换
   ├--proot-termux/        # proot fork(enter.c/exit.c 手工 clang 编译)
   ├--bash-5.3/            # 嵌入 shell
   └--第三方/               # talloc + libandroid-shmem
```

## 构建

1. 重新编译 proot 改动(必须手工,`build_su.sh` 不编译这些):

```bash
cd src/proot-termux/src
clang -O2 -fPIC -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -DARG_MAX=131072 \
      -DVERSION=\"5.1.0\" -DWITH_LIBANDROID_SHMEM -I. -I../include \
      -c syscall/enter.c -o syscall/enter.o
clang -O2 ... -c syscall/exit.c -o syscall/exit.o
cd ../../ && bash build_su.sh
```

2. 部署:把 `fakesu.elf` 复制到 `/data/data/com.termux/files/usr/bin/fakesu.elf`,`/usr/bin/su` 用 shim 脚本(`#!/system/bin/sh\nexec .../fakesu.elf "$@"`)。

### ⚠ 已知限制

- **不能把 ELF 改名为 `su`**:本机 Termux/安全策略对**任何**名为 `su` 的 ELF 拒绝执行(EPERM,脚本不受限),`/usr/bin/su` 必须保持 shim 脚本形式。其余名字(`fakesu.elf`、`suxx.elf`)均可执行。
- `-c` 长脚本曾因 su.c 里包装串 `snprintf` 固定 1024 字节 + 格式串 10 个 `%s` 只有 9 个实参而截断/崩坏,已改为 `strlen(g_cmd)+1024` 容量并补齐实参。
- 覆盖层**不**覆盖 `/data`;目录读操作保持真实 inode。
- 套路始终骗不过 magisk 级深度检测。

## 依赖

- Android 7+ 沙箱环境 + Termux(shim 与 ELF 都必须在 termux 用户目录内执行)
- Shizuku/Rish 用于提 shell 权限(降级模式则以当前用户运行)
- 仅娱乐,别拿去做坏事