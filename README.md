# aFakeSU

>本项目仅供娱乐

## 简介

aFakeSU 是一个面向非 root Android 设备的伪 `su` 实现。可以通过项目根目录的su文件用shell执行,不过这通常是汇报没有权限的所以你可以手动把产物移动到/data/local/tmp/,可以在任何拥有执行权限的用户下运行此二进制文件,算是为不能root的主机一个心里安慰,仅供娱乐
## 工作模式

`su.elf` 有两种执行上下文，完全隔离：

| 模式 | UID | Context | 触发条件 |
|------|-----|---------|----------|
| **Shell 模式** | 2000 | `u:r:shell:s0` | Shizuku 连接成功 |
| **降级模式** | 10380 | `u:r:untrusted_app_27:s0` | Shizuku 连接失败 |

**执行流程：**
1. 尝试通过 rish/shizuku 获取 shell 权限
2. 成功 → 以 UID 2000 (shell) 执行命令
3. 失败 → 降级为当前用户 (UID 10380) 执行命令

## 目录结构

```
aFakeSU/
├── su.c                 # 主程序源码
├── build_su.sh          # 构建脚本
├── rish                 # Shizuku 连接脚本
├── rishq                # 用户 ID 查询脚本
├── rish_shizuku.dex     # Shizuku 连接用 dex
├── README.md
├── src/
│   ├── su.c             # 主程序源码
│   ├── build_su.sh      # 构建脚本
│   ├── proot-termux/    # proot（termux fork）
│   ├── bash-5.3/        # bash 源码（嵌入到二进制）
│   └── third_party/     # talloc + libandroid-shmem
```

## 构建

```bash
cd src
bash build_su.sh
```

产物：`su.elf`（ARM64 PIE，Android 24+）

## 依赖

- Android 13+ (SDK 33)
- Shizuku Manager 已安装且 server 运行中
- 本应用已授权 Shizuku 权限
- Termux + clang (NDK)

本项目仅供娱乐
