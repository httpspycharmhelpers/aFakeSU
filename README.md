#aFakeSU
##简介

aFakeSU是一个面向非root Android设备的伪`Su`实现。可以通过项目根目录的su文件用shell执行，不过这通常是汇报没有权限的所以你可以手动把产物移动到/data/local/tmp/，可以在任何拥有执行权限的用户下运行此二进制文件，算是为不能root的主机运行需要root的脚本,始终骗不过深度检查

##工作模式

`su.elf`有两种执行上下文，完全隔离：

|模式|UID|语境|触发条件|
|------|-----|---------|----------|
| **shell模式** | 2000 | `u:r：shell:s0` |shizuku连接成功|
| **降级模式** | 10380 | `u:r：untrusted_app_27:s0` |shizuku连接失败|

**执行流程：**
1.尝试通过Rish/shizuku获取shell权限
2.成功→以UID2000(shell)执行命令
3.失败→降级为当前用户(UID10380)执行命令

##目录结构

```
aFakeSU/
├--su.c#主程序源码
├--build_su.sh#构建脚本
├-rish#静祖连接脚本
├── rishq                # 用户 ID 查询脚本
├--rish_shizuku.dex#Shizuku连接用dex
├--README.md
├--src/
│├--su.c#主程序源码
│├--build_su.sh#构建脚本
│├--proot-termux/#proot(Termux分叉)
│├--bash-5.3/#bash源码（嵌入到二进制）
│└--第三方/#talloc+libandroid-shmem
```

##构建

```bash
cd src
bash build_su.sh
```

产物：`su.elf`(ARM64PIE，Android24+)

##依赖

-Android13+(SDK33)
-shizuku已安装且服务器运行中
-本应用已授权shizuku权限
-Termux+NDK
.deb包时常有问题
想要使用shell权限运行必须将可执行文件复制/data/local/tmp
本项目仅供娱乐
源码有缺陷去Release页面下载最新的.deb包或者.zip
