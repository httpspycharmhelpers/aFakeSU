
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include "proot-termux/src/cli/cli.h"
#include "proot-termux/src/cli/note.h"
extern const unsigned char _binary_bash_bin_start[];
extern const unsigned char _binary_bash_bin_end[];
#define EMBEDDED_BASH_SIZE ((size_t)(_binary_bash_bin_end - _binary_bash_bin_start))
extern const unsigned char _binary_libiconv_so_start[];
extern const unsigned char _binary_libiconv_so_end[];
extern const unsigned char _binary_libncursesw_so_6_5_start[];
extern const unsigned char _binary_libncursesw_so_6_5_end[];
#define VERSION_STR "2c6adbc6:AFAKESU"
#define VERSION_CODE "27000"
#define DEFAULT_SHELL "/system/bin/sh"
#define ANDROID_PATH "/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin"
/* Guest-visible mirror of <workdir>/bin (the getprop/mount/chcon wrappers).
 * It must NOT expose the Termux path in $PATH or `which`, so the host
 * workdir/bin is bound here and this clean path is what the session sees. */
#define GUEST_BIN_DIR "/data/local/tmp/.su/bin"
#define GUEST_PATH GUEST_BIN_DIR ":/sbin:/system/sbin:/system/bin:/system/xbin:/vendor/bin"
#define INT_MAX_VALUE 2147483647
#define WORK_DIR_NAME ".tmp"
#define REAL_FILE "real_uids.txt"
#define RISHQ_NAME "rishq"
static char g_self_exe[PATH_MAX];
static char g_self_dir[PATH_MAX];
char g_work_dir[PATH_MAX];
char g_proot_argv0[64];
int g_proot_argv0_fix = 0;
static const char *g_fake_name = "sh";
/* Export of the fake comm name for the proot side (proc.c/enter.c) so
 * /proc/<pid>/status "Name:" matches /system/bin/sh, not the real
 * Termux interpreter. */
char g_fake_comm[64] = "sh";
static char *g_cmd = NULL;
static size_t g_cmd_cap = 0, g_cmd_len = 0;
static const char *g_grp = NULL;
static const char **g_suppg = NULL;
static size_t g_suppg_cnt = 0;

static const char *g_tgt = NULL;
static const char *g_shell = DEFAULT_SHELL;
static bool g_login = false;
static bool g_preserve = false;
static bool g_mount = false;
/* -i: force a pseudo-terminal even with -c.  -d: drop every Linux
 * capability for the session, so a capability probe sees an unprivileged
 * root rather than a fully capable one. */
static bool g_interactive = false;
static bool g_drop_cap = false;
/* Set from -i and consumed by build_and_run_proot(). */
static bool force_pty = false;

/*
 * Per-UID root policy, mirroring Magisk's su policy table.
 * Magisk does not hand root to every caller.  Each requesting app UID gets
 * its own policy, the caller is identified by the UID of the process asking
 * for su, and the policy decides between four answers:
 *
 *   ALLOW     run the session as requested
 *   DENY      refuse outright, exactly like Magisk's "deny" prompt choice
 *   QUERY     no stored decision: ask.  Without a manager app to prompt,
 *              Magisk denies, and so do we.
 *   RESTRICT  allow, but force drop_cap, so the session is a root with an
 *             empty capability set
 *
 * RESTRICT is why the capability handling is not just a -d flag: Magisk
 * turns drop_cap on by itself when the policy says restrict.
 */
enum su_policy { SU_POLICY_ALLOW, SU_POLICY_DENY, SU_POLICY_QUERY, SU_POLICY_RESTRICT };

/* Global root access modes, matching Magisk's RootAccess setting. */
enum root_access { ROOT_ACCESS_PROMPT, ROOT_ACCESS_DISABLED,
		   ROOT_ACCESS_ADB_ONLY, ROOT_ACCESS_APPS_ONLY };
static enum root_access g_root_access = ROOT_ACCESS_PROMPT;

/* Multiuser modes, matching Magisk's MultiuserMode. */
enum multiuser_mode { MULTIUSER_OWNER_MANAGED, MULTIUSER_OWNER_ONLY,
		      MULTIUSER_GLOBAL };
static enum multiuser_mode g_multiuser = MULTIUSER_OWNER_MANAGED;

/* The UID of whoever invoked su; the policy lookup keys off this. */
static long g_caller_uid = -1;
static const char **g_pos = NULL;
static size_t g_pos_cnt = 0;
static long g_uid = -1;
static long g_gid = -1;
const char *g_selinux_ctx = NULL;

static bool valid_uint_str(const char *s, long *out);
static const char *aid_name_for_uid(long uid, char *name, size_t namesz);
/*
 * Persisted property overrides.  Real root flips read-only props with
 * resetprop/setprop (ro.debuggable, ro.build.*, ro.product.*, ...); a
 * fake root owns a store that shadows the real property service for every
 * process under the session's PATH (getprop/resetprop/setprop first).
 */
static void emit_prop_wrappers(const char *ctx_path)
{
	const char *wd = g_work_dir[0] != '\0' ? g_work_dir : "/data/local/tmp/termux";
	char st[PATH_MAX];
	char p[PATH_MAX];
	snprintf(st, sizeof(st), "%s/.props", wd);

	snprintf(p, sizeof(p), "%s/getprop", ctx_path);
	FILE *f = fopen(p, "w");
	if (f) {
		fprintf(f,
			"#!/system/bin/sh\n"
			"P=\"%s\"\n"
			"[ -f \"$P\" ] || exec /system/bin/getprop \"$@\"\n"
			"if [ \"$#\" -eq 0 ] || [ \"$1\" = \"-p\" ]; then\n"
			"  while IFS='=' read -r k v; do echo \"[$k]: [$v]\"; done < \"$P\"\n"
			"  exit 0\n"
			"fi\n"
			"while IFS='=' read -r k v; do\n"
			"  [ \"$k\" = \"$1\" ] && { echo \"$v\"; exit 0; }\n"
			"done < \"$P\"\n"
			"exec /system/bin/getprop \"$@\"\n",
			st);
		fclose(f);
		chmod(p, 0755);
	}

	static const char *names[] = { "setprop", "resetprop" };
	size_t i;
	for (i = 0; i < 2; i++) {
		snprintf(p, sizeof(p), "%s/%s", ctx_path, names[i]);
		f = fopen(p, "w");
		if (f) {
			fprintf(f,
				"#!/system/bin/sh\n"
				"P=\"%s\"\n"
				"W=\"%s\"\n"
				"name=\"\"; val=\"\"; args=0; del=0\n"
				"for a in \"$@\"; do\n"
				"  case \"$a\" in\n"
				"    -d|--delete) del=1 ;;\n"
				"    -p|--persistent) : ;;\n"
				"    -*) : ;;\n"
				"    *) args=$((args+1))\n"
				"       [ $args -eq 1 ] && name=\"$a\"\n"
				"       [ $args -eq 2 ] && val=\"$a\" ;;\n"
				"  esac\n"
				"done\n"
				"M=\"$W/.propsmod\"\n"
				"if [ $del -eq 1 ] && [ -n \"$name\" ]; then\n"
				"  [ -f \"$P\" ] && sed -i \"/^$name=/d\" \"$P\"\n"
				"  [ -f \"$M\" ] && sed -i \"/^$name$/d\" \"$M\"\n"
				"  exit 0\n"
				"fi\n"
				"if [ $args -ge 2 ] && [ -n \"$name\" ]; then\n"
				"  [ -f \"$P\" ] && sed -i \"/^$name=/d\" \"$P\"\n"
				"  echo \"$name=$val\" >> \"$P\"\n"
				"  mkdir -p \"$W\"\n"
				"  [ -f \"$M\" ] && sed -i \"/^$name$/d\" \"$M\"\n"
				"  echo \"$name\" >> \"$M\"\n"
				"  exit 0\n"
				"fi\n"
				"if [ $args -eq 1 ] && [ -n \"$name\" ] && [ -f \"$P\" ]; then\n"
				"  while IFS='=' read -r k v; do\n"
				"    [ \"$k\" = \"$name\" ] && { echo \"$v\"; exit 0; }\n"
				"  done < \"$P\"\n"
				"fi\n"
				"exec /system/bin/%s \"$@\"\n",
				st, wd, names[i]);
			fclose(f);
			chmod(p, 0755);
		}
	}
}
static void emit_mount_wrapper(const char *ctx_path)
{
	char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/mount", ctx_path);
	FILE *f = fopen(p, "w");
	if (!f)
		return;
	fprintf(f,
		"#!/system/bin/sh\n"
		"W=\"%s\"\n"
		"mkdir -p \"$W/.mnt\"\n"
		"if echo \" $* \" | grep -q 'remount'; then\n"
		"  rw=0; ro=0\n"
		"  echo \" $* \" | grep -qE '(^|,)rw(,|$)' && rw=1\n"
		"  echo \" $* \" | grep -qE '(^|,)ro(,|$)' && ro=1\n"
		"  tgt=\n"
		"  for a in \"$@\"; do\n"
		"    case \"$a\" in\n"
		"      -o|-t|*remount*|rw|ro|bind|rbind|none|ext4|f2fs|erofs) : ;;\n"
		"      *) tgt=\"$a\" ;;\n"
		"    esac\n"
		"  done\n"
		"  [ -n \"$tgt\" ] || tgt=/ \n"
		"  comp=$(echo \"$tgt\" | sed 's#^/*##; s#/.*$##')\n"
		"  [ -n \"$comp\" ] || comp=root\n"
		"  if [ $rw -eq 1 ] && [ $ro -eq 0 ]; then\n"
		"    echo 1 > \"$W/.mnt/$comp\"\n"
		"  elif [ $ro -eq 1 ]; then\n"
		"    rm -f \"$W/.mnt/$comp\"\n"
		"  fi\n"
		"  /system/bin/mount \"$@\" >/dev/null 2>&1\n"
		"  exit 0\n"
		"fi\n"
		"exec /system/bin/mount \"$@\"\n",
		g_work_dir[0] != '\0' ? g_work_dir : "/data/local/tmp/termux");
	fclose(f);
	chmod(p, 0755);
}

/*
 * chcon/restorecon: real root rewrites security.selinux labels; a fake
 * root records them into <workdir>/.xattrs/<path-with-'%'> which
 * enter.c replays for getxattr/lgetxattr (ls -Z) while the real
 * syscall keeps failing with EPERM.
 */
static void emit_chcon_wrappers(const char *ctx_path)
{
	char xd[PATH_MAX];
	char p[PATH_MAX];
	snprintf(xd, sizeof(xd), "%s/.xattrs", g_work_dir);

	snprintf(p, sizeof(p), "%s/chcon", ctx_path);
	FILE *f = fopen(p, "w");
	if (f) {
		fprintf(f,
			"#!/system/bin/sh\n"
			"X=\"%s\"\n"
			"mkdir -p \"$X\"\n"
			"ctx=; opts=1\n"
			"for a in \"$@\"; do\n"
			"  case \"$a\" in\n"
			"    -*) : ;;\n"
			"    *) if [ -z \"$ctx\" ]; then ctx=\"$a\"; else\n"
			"         k=$(echo \"$a\" | sed 's#/#%%#g')\n"
			"         case \"$k\" in %%*) k=${k#%%} ;; esac\n"
			"         echo \"$ctx\" > \"$X/$k\"\n"
			"       fi ;;\n"
			"  esac\n"
			"done\n"
			"/system/bin/chcon \"$@\" >/dev/null 2>&1\n"
			"exit 0\n",
			xd);
		fclose(f);
		chmod(p, 0755);
	}

	snprintf(p, sizeof(p), "%s/restorecon", ctx_path);
	f = fopen(p, "w");
	if (f) {
		fprintf(f,
			"#!/system/bin/sh\n"
			"X=\"%s\"\n"
			"for a in \"$@\"; do\n"
			"  case \"$a\" in\n"
			"    -*) : ;;\n"
			"    *) k=$(echo \"$a\" | sed 's#/#%%#g')\n"
			"       case \"$k\" in %%*) k=${k#%%} ;; esac\n"
			"       rm -f \"$X/$k\" ;;\n"
			"  esac\n"
			"done\n"
			"/system/bin/restorecon \"$@\" >/dev/null 2>&1\n"
			"exit 0\n",
			xd);
		fclose(f);
		chmod(p, 0755);
	}
}

static void print_help(FILE *f)
{
	fprintf(f,
		"FakeSU\n"
		"\n"
		"Usage: fakesu [options] [-] [user [argument...]]\n"
		"\n"
		"Options:\n"
		"  -c, --command COMMAND         Pass COMMAND to the invoked shell\n"
		"  -i, --interactive             Force pseudo-terminal allocation when using -c\n"
		"  -g, --group GROUP             Specify the primary group\n"
		"  -G, --supp-group GROUP        Specify a supplementary group.\n"
		"                                The first specified supplementary group is also used\n"
		"                                as a primary group if the option -g is not specified.\n"
		"  -Z, --context CONTEXT         Change SELinux context\n"
		"  -t, --target PID              PID to take mount namespace from\n"
		"  -d, --drop-cap                Drop all Linux capabilities\n"
		"  -h, --help                    Display this help message and exit\n"
		"  -, -l, --login                Pretend the shell to be a login shell\n"
		"  -m, -p,\n"
		"  --preserve-environment        Preserve the entire environment\n"
		"  -s, --shell SHELL             Use SHELL instead of the default /system/bin/sh\n"
		"  -v, --version                 Display version number and exit\n"
		"  -V                            Display version code and exit\n"
		"  -mm, -M,\n"
		"  --mount-master                Force run in the global mount namespace\n"
		"\n"
		"  -t and -mm are accepted for command-line compatibility.  A fake root\n"
		"  runs unprivileged, so it cannot really join another process's mount\n"
		"  namespace; both options report that and continue in the current one.\n"
		"\n");
}
static void usage_err(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	fputc('\n', stderr);
	fputc('\n', stderr);
	print_help(stderr);
	exit(2);
}
static void invalid_gid(const char *s)
{
	fprintf(stderr, "Invalid GID: %s\n", s);
	print_help(stderr);
	exit(1);
}
static void invalid_pid(const char *s)
{
	fprintf(stderr, "Invalid PID: %s\n", s);
	print_help(stderr);
	exit(1);
}
static const char *const AID_USERS =
	"root:0\n"
	"daemon:1\n"
	"bin:2\n"
	"sys:3\n"
	"system:1000\n"
	"radio:1001\n"
	"bluetooth:1002\n"
	"graphics:1003\n"
	"input:1004\n"
	"audio:1005\n"
	"camera:1006\n"
	"log:1007\n"
	"compass:1008\n"
	"mount:1009\n"
	"wifi:1010\n"
	"adb:1011\n"
	"install:1012\n"
	"media:1013\n"
	"dhcp:1014\n"
	"sdcard_rw:1015\n"
	"vpn:1016\n"
	"keystore:1017\n"
	"usb:1018\n"
	"drm:1019\n"
	"mdnsr:1020\n"
	"gps:1021\n"
	"media_rw:1023\n"
	"mtp:1024\n"
	"drmrpc:1026\n"
	"nfc:1027\n"
	"sdcard_r:1028\n"
	"clat:1029\n"
	"loop_radio:1030\n"
	"mediadrm:1031\n"
	"package_info:1032\n"
	"sdcard_pics:1033\n"
	"sdcard_av:1034\n"
	"sdcard_all:1035\n"
	"logd:1036\n"
	"shared_relro:1037\n"
	"dbus:1038\n"
	"tlsdate:1039\n"
	"mediaex:1040\n"
	"audioserver:1041\n"
	"metrics_coll:1042\n"
	"metricsd:1043\n"
	"webserv:1044\n"
	"debuggerd:1045\n"
	"mediacodec:1046\n"
	"cameraserver:1047\n"
	"firewall:1048\n"
	"trunks:1049\n"
	"nvram:1050\n"
	"dns:1051\n"
	"dns_tether:1052\n"
	"webview_zygote:1053\n"
	"vehicle_network:1054\n"
	"media_audio:1055\n"
	"media_video:1056\n"
	"media_image:1057\n"
	"tombstoned:1058\n"
	"media_obb:1059\n"
	"ese:1060\n"
	"ota_update:1061\n"
	"automotive_evs:1062\n"
	"lowpan:1063\n"
	"hsm:1064\n"
	"reserved_disk:1065\n"
	"statsd:1066\n"
	"incidentd:1067\n"
	"secure_element:1068\n"
	"lmkd:1069\n"
	"llkd:1070\n"
	"iorapd:1071\n"
	"gpu_service:1072\n"
	"network_stack:1073\n"
	"gsid:1074\n"
	"fsverity_cert:1075\n"
	"credstore:1076\n"
	"external_storage:1077\n"
	"ext_data_rw:1078\n"
	"ext_obb_rw:1079\n"
	"context_hub:1080\n"
	"virtualizationservice:1081\n"
	"artd:1082\n"
	"uwb:1083\n"
	"thread_network:1084\n"
	"diced:1085\n"
	"dmesgd:1086\n"
	"jc_weaver:1087\n"
	"jc_strongbox:1088\n"
	"jc_identitycred:1089\n"
	"sdk_sandbox:1090\n"
	"security_log_writer:1091\n"
	"prng_seeder:1092\n"
	"uprobestsats:1093\n"
	"cros_ec:1094\n"
	"mmd:1095\n"
	"shell:2000\n"
	"cache:2001\n"
	"diag:2002\n"
	"net_bt_admin:3001\n"
	"net_bt:3002\n"
	"inet:3003\n"
	"net_raw:3004\n"
	"net_admin:3005\n"
	"net_bw_stats:3006\n"
	"net_bw_acct:3007\n"
	"readproc:3009\n"
	"wakelock:3010\n"
	"uhid:3011\n"
	"readtracefs:3012\n"
	"virtualmachine:3013\n"
	"everybody:9997\n"
	"misc:9998\n"
	"nobody:9999";
static const char *const AID_GAPS =
	"oem_3000:3000\n"
	"oem_3008:3008\n"
	"oem_3020:3020";
static const struct { const char *user; const char *ctx; } AID_SELINUX_CTX[] = {
	/* uid 1000-2000: AOSP seapp_contexts + daemon domains */
	{ "root", "u:r:kernel:s0" },
	{ "system", "u:r:system_app:s0" },
	{ "radio", "u:r:radio:s0" },
	{ "bluetooth", "u:r:mtk_hal_bluetooth:s0" },
	{ "wifi", "u:r:wificond:s0" },
	{ "media", "u:r:mediametrics:s0" },
	{ "keystore", "u:r:keystore:s0" },
	{ "drm", "u:r:drm:s0" },
	{ "gps", "u:r:mnld:s0" },
	{ "shell", "u:r:shell:s0" },
	{ "log", "u:r:logd:s0" },
	{ "logd", "u:r:logd:s0" },
	{ "graphics", "u:r:surfaceflinger:s0" },
	{ "camera", "u:r:cameraserver:s0" },
	{ "input", "u:r:input:s0" },
	{ "audio", "u:r:audioserver:s0" },
	{ "vpn", "u:r:vpn:s0" },
	{ "install", "u:r:installd:s0" },
	{ "mount", "u:r:vold:s0" },
	{ "vold", "u:r:vold:s0" },
	{ "adb", "u:r:adbd:s0" },
	{ "dhcp", "u:r:dhcp:s0" },
	{ "nfc", "u:r:nfc:s0" },
	{ "mtp", "u:r:mtp:s0" },
	{ "clat", "u:r:clatd:s0" },
	{ "mediadrm", "u:r:mediadrm:s0" },
	{ "shared_relro", "u:r:shared_relro:s0" },
	{ "net_admin", "u:r:network_stack:s0" },
	{ "network_stack", "u:r:network_stack:s0" },
	{ "net_bt_admin", "u:r:bluetooth:s0" },
	{ "net_bt", "u:r:bluetooth:s0" },
	{ "net_bt_stack", "u:r:bluetooth:s0" },
	{ "audioserver", "u:r:audioserver:s0" },
	{ "cameraserver", "u:r:cameraserver:s0" },
	{ "debuggerd", "u:r:debuggerd:s0" },
	{ "nobody", "u:r:nobody:s0" },
};
/* Real daemon/app domains that init/zygote/device genuinely reachable ones,
 * allowed as the only valid domain namespace for an injected -Z/--context. */
static const char *const SELINUX_KNOWN_EXTRA_CTX[] = {
	"u:r:system_server:s0", "u:r:init:s0", "u:r:system_init:s0",
	"u:r:zygote:s0", "u:r:surfaceflinger:s0", "u:r:servicemanager:s0",
	"u:r:netd:s0", "u:r:vold:s0", "u:r:installd:s0", "u:r:adbd:s0",
	"u:r:untrusted_app:s0", "u:r:platform_app:s0", "u:r:priv_app:s0",
	"u:r:isolated_app:s0", "u:r:su:s0",
};
static void selinux_ctx_reject(const char *in)
{
	fprintf(stderr,
		"su: security context `%s' names a domain that does not exist on this device; refusing\n",
		in);
	exit(1);
}
static int selinux_ctx_known(const char *ctx)
{
	size_t i;
	if (ctx == NULL)
		return 0;
	char base[128];
	const char *mls = strstr(ctx, ":s0:c");
	if (mls != NULL) {
		size_t n = (size_t)(mls - ctx) + 3;
		if (n >= sizeof(base))
			return 0;
		memcpy(base, ctx, n);
		base[n] = '\0';
		ctx = base;
	}
	for (i = 0; i < sizeof(AID_SELINUX_CTX)/sizeof(AID_SELINUX_CTX[0]); i++)
		if (strcmp(AID_SELINUX_CTX[i].ctx, ctx) == 0)
			return 1;
	for (i = 0; i < sizeof(SELINUX_KNOWN_EXTRA_CTX)/sizeof(SELINUX_KNOWN_EXTRA_CTX[0]); i++)
		if (strcmp(SELINUX_KNOWN_EXTRA_CTX[i], ctx) == 0)
			return 1;
	return 0;
}
/* Accept a full u:r:<domain>:s0 string or a bare known name (e.g. "shell",
 * "radio"), normalize it, and hard-fail if the domain is not one this device
 * could actually carry.  Rejects any non-sepolicy character (no "/", spaces,
 * newlines) so the ctx file can never be corrupted via injection. */
static const char *selinux_ctx_norm_validate(const char *in)
{
	static char norm[128];
	const char *ctx = in;
	size_t i;
	if (in == NULL || in[0] == '\0') {
		fprintf(stderr, "su: empty security context\n");
		exit(1);
	}
	if (strncmp(in, "u:r:", 4) != 0) {
		for (i = 0; i < sizeof(AID_SELINUX_CTX)/sizeof(AID_SELINUX_CTX[0]); i++)
			if (strcmp(AID_SELINUX_CTX[i].user, in) == 0) {
				ctx = AID_SELINUX_CTX[i].ctx;
				break;
			}
		if (strncmp(ctx, "u:r:", 4) != 0) {
			/* a bare domain name: only usable if it maps to a known ctx */
			static char bare[128];
			if (snprintf(bare, sizeof(bare), "u:r:%s:s0", in) >= (int)sizeof(bare))
				selinux_ctx_reject(in);
			if (!selinux_ctx_known(bare))
				selinux_ctx_reject(in);
			ctx = bare;
		}
	}
	for (i = 0; ctx[i] != '\0'; i++) {
		char c = ctx[i];
		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
		      || (c >= '0' && c <= '9') || c == '_' || c == ':' || c == ','))
			selinux_ctx_reject(in);
		if (i >= sizeof(norm) - 2)
			selinux_ctx_reject(in);
	}
	if (!selinux_ctx_known(ctx))
		selinux_ctx_reject(in);
	snprintf(norm, sizeof(norm), "%s", ctx);
	return norm;
}
static bool valid_uint_str(const char *s, long *out);
static int rishq_query(const char *cmd, char **out, size_t *outlen);
static const char *aid_merged_table(void);
static const char *const g_usermap_name = ".usermap";
/* Look up "name:uid" lines in a merged table; return true when row found. */
static bool aid_table_has_uid(const char *table, long uid)
{
	if (table == NULL)
		return false;
	char *copy = strdup(table);
	if (copy == NULL)
		return false;
	bool res = false;
	char *save = NULL;
	for (char *line = strtok_r(copy, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		char *colon = strchr(line, ':');
		long v;
		if (colon && valid_uint_str(colon + 1, &v) && v == uid) {
			res = true;
			break;
		}
	}
	free(copy);
	return res;
}
/* Append the oem_# auto-named ranges AOSP reserves (2900-2999 vendor,
 * 5000-5999 second oem range): every value in these ranges exists as a
 * resolvable user/group name on real devices. */
static void aid_append_oem_range(char *merged, size_t cap, size_t *n, int lo, int hi)
{
	for (int u = lo; u <= hi; u++) {
		size_t left = cap - *n;
		if (left < 24)
			break;
		int w = snprintf(merged + *n, left, "oem_%d:%d\n", u, u);
		if (w < 0)
			break;
		*n += (size_t)w;
	}
}
/* Full, searchable identity table = static AID_USERS + the OEM/gap names
 * + the auto-named oem ranges + the real user list fetched from the device
 * itself via rishq (kept in workdir/.usermap).  This is what "su <user>"
 * resolves names against, so every user the vendor actually exposes
 * (incl. the 3000-series) has a uid<->name mapping instead of silently
 * collapsing into root. */
static const char *aid_merged_table(void)
{
	static char merged[65536];
	static bool built = false;
	if (!built) {
		built = true;
		size_t n = 0;
		const char *tables[2] = { AID_USERS, AID_GAPS };
		for (int ti = 0; ti < 2 && n < sizeof(merged) - 2; ti++) {
			char *copy = strdup(tables[ti]);
			if (copy == NULL)
				continue;
			char *save = NULL;
			for (char *line = strtok_r(copy, "\n", &save); line && n < sizeof(merged) - 2;
			     line = strtok_r(NULL, "\n", &save)) {
				size_t l = strlen(line);
				if (n + l + 2 >= sizeof(merged))
					break;
				memcpy(merged + n, line, l);
				n += l;
				merged[n++] = '\n';
			}
			free(copy);
		}
		aid_append_oem_range(merged, sizeof(merged), &n, 2900, 2999);
		aid_append_oem_range(merged, sizeof(merged), &n, 5000, 5999);
		/* Merge the device's real users (uid unknown to the static
		 * table): e.g. vendor-only AIDs running on this box. */
		char path[PATH_MAX];
		snprintf(path, sizeof(path), "%s/%s", g_work_dir, g_usermap_name);
		FILE *f = fopen(path, "r");
		if (f) {
			char line[256];
			while (f && n < sizeof(merged) - 2 && fgets(line, sizeof(line), f)) {
				char *sp = strchr(line, ' ');
				if (!sp)
					continue;
				*sp = '\0';
				char *end;
				long u = strtol(sp + 1, &end, 10);
				if (end == sp + 1 || u < 1000 || u >= 200000)
					continue;
				size_t nl = strlen(line);
				while (nl > 0 && (line[nl - 1] == '\n' || line[nl - 1] == '\r'))
					line[--nl] = '\0';
				if (nl == 0 || strchr(line, ':') || strchr(line, ' '))
					continue;
				if (aid_table_has_uid(merged, u))
					continue;
				if (n + nl + 16 >= sizeof(merged))
					continue;
				memcpy(merged + n, line, nl);
				n += nl;
				merged[n++] = ':';
				size_t digs = snprintf(merged + n, sizeof(merged) - n, "%ld\n", u);
				n += digs;
			}
			fclose(f);
		}
		merged[n] = '\0';
	}
	return merged;
}
/* Fetch the device's real uid<->name list through rishq (the shell-level
 * bridge), persist it to workdir/.usermap, and refresh it once an hour so
 * vendor-specific users discovered by later runs keep the tables warm
 * without hammering shizuku on every invocation. */
static void aid_real_patch(void)
{
	if (g_work_dir[0] == '\0')
		return;
	char path[PATH_MAX], tmp[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", g_work_dir, g_usermap_name);
	snprintf(tmp, sizeof(tmp), "%s/%s.tmp", g_work_dir, g_usermap_name);
	struct stat st;
	if (stat(path, &st) == 0 && st.st_size > 0
	    && time(NULL) - st.st_mtime < 3600)
		return;
	char *out = NULL;
	size_t outlen = 0;
	if (rishq_query("ps -A -o USER= -o UID= 2>/dev/null", &out, &outlen) != 0) {
		free(out);
		return;
	}
	FILE *f = fopen(tmp, "w");
	if (!f) {
		free(out);
		return;
	}
	int written = 0;
	if (out != NULL) {
		char *save = NULL;
		for (char *line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
			char *sp = strchr(line, ' ');
			if (!sp)
				continue;
			*sp = '\0';
			char *end;
			errno = 0;
			long u = strtol(sp + 1, &end, 10);
			if (end == sp + 1 || errno)
				continue;
			char *ep = end;
			while (*ep == ' ')
				ep++;
			if (*ep != '\0' && *ep != '\n' && *ep != '\r')
				continue;
			if (line[0] == '\0')
				continue;
			fprintf(f, "%s %ld\n", line, u);
			written++;
		}
	}
	if (written == 0)
		fprintf(f, "root 0\n");
	fclose(f);
	rename(tmp, path);
	free(out);

	/* Harvest real per-uid supplementary groups from live processes
	 * (/proc/<pid>/status "Groups:"), so each su <user> keeps the group
	 * memberships that uid actually carries on this device (which can
	 * differ from the canonical single-gid assumption). */
	char gpath[PATH_MAX], gtmp[PATH_MAX];
	snprintf(gpath, sizeof(gpath), "%s/.ugroups", g_work_dir);
	snprintf(gtmp, sizeof(gtmp), "%s/.ugroups.tmp", g_work_dir);
	out = NULL;
	outlen = 0;
	if (rishq_query(
	    "for d in /proc/[0-9]*; do s=\"$d/status\"; [ -r \"$s\" ] || continue; "
	    "u=$(sed -n 's/^Uid:[[:space:]]*\\([0-9][0-9]*\\).*/\\1/p' \"$s\"); "
	    "[ -n \"$u\" ] || continue; "
	    "g=$(sed -n 's/^Groups:[[:space:]]*//p' \"$s\"); "
	    "[ -n \"$g\" ] || g=0; "
	    "echo \"$u:$g\"; done 2>/dev/null | sort -u", &out, &outlen) != 0) {
		free(out);
		return;
	}
	f = fopen(gtmp, "w");
	if (f == NULL) {
		free(out);
		return;
	}
	if (out != NULL) {
		char *save = NULL;
		for (char *line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
			char *colon = strchr(line, ':');
			if (colon == NULL)
				continue;
			char *end;
			long u = strtol(line, &end, 10);
			if (end != colon || u < 0 || u > 0x7fffffffL)
				continue;
			char *g = colon + 1;
			while (*g == ' ' || *g == '\t')
				g++;
			/* Keep the first occurrence per uid. */
			if (strncmp(line, "999999999:", 10) == 0)
				continue;
			fprintf(f, "%ld:%s\n", u, g);
		}
	}
	fclose(f);
	rename(gtmp, gpath);
	free(out);
}
static const char *aid_name_for_uid(long uid, char *name, size_t namesz)
{
	char *table = strdup(aid_merged_table());
	if (table == NULL)
		return NULL;
	char *save = NULL;
	const char *res = NULL;
	for (char *line = strtok_r(table, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		char *colon = strchr(line, ':');
		if (colon == NULL)
			continue;
		long v;
		if (valid_uint_str(colon + 1, &v) && v == uid) {
			size_t n = (size_t)(colon - line);
			if (n >= namesz)
				n = namesz - 1;
			memcpy(name, line, n);
			name[n] = '\0';
			res = name;
			break;
		}
	}
	free(table);
	return res;
}
static int run_cmd_capture_argv(const char *path, const char *arg1, const char *arg2, char **out, size_t *outlen);
static const char *selinux_ctx_for_user(const char *user)
{
	/* resolve name like uid/gid resolution does: numeric -> AID_USERS name */
	const char *name = user;
	char namebuf[64];
	if (name != NULL && name[0] != '\0' && isdigit((unsigned char)name[0])) {
		long u;
		if (valid_uint_str(name, &u) && aid_name_for_uid(u, namebuf, sizeof(namebuf)))
			name = namebuf;
	}
	if (name == NULL || name[0] == '\0' || strcmp(name, "root") == 0)
		return "u:r:kernel:s0";
	if (strcmp(name, "shell") == 0)
		return "u:r:shell:s0";

	/* real-time context list: scan ps -Z -A for the target user */
	char exact_ctx[96];
	snprintf(exact_ctx, sizeof(exact_ctx), "u:r:%s:s0", name);
	static char g_ctx_buf[128];
	g_ctx_buf[0] = '\0';
	char *out = NULL;
	size_t outlen = 0;
	if (run_cmd_capture_argv("/system/bin/ps", "-Z", "-A", &out, &outlen) == 0 && out != NULL) {
		char *copy = strdup(out);
		free(out);
		if (copy != NULL) {
			const char *pref = NULL, *first = NULL;
			char *lsave = NULL;
			for (char *line = strtok_r(copy, "\n", &lsave); line; line = strtok_r(NULL, "\n", &lsave)) {
				if (strncmp(line, "u:r:", 4) != 0)
					continue;
				char *psave = NULL;
				char *label = strtok_r(line, " ", &psave);
				char *uname = strtok_r(NULL, " ", &psave);
				if (label == NULL || uname == NULL)
					continue;
				if (strcmp(uname, name) != 0)
					continue;
				if (strcmp(label, exact_ctx) == 0) {
					snprintf(g_ctx_buf, sizeof(g_ctx_buf), "%s", label);
					break;
				}
				if (strncmp(label + 4, name, strlen(name)) == 0 && pref == NULL)
					pref = label;
				if (first == NULL)
					first = label;
			}
			if (g_ctx_buf[0] == '\0') {
				const char *chosen = pref != NULL ? pref : first;
				if (chosen != NULL)
					snprintf(g_ctx_buf, sizeof(g_ctx_buf), "%s", chosen);
			}
			free(copy);
			if (g_ctx_buf[0] != '\0')
				return g_ctx_buf;
		}
	}
	/* fallback: static table */
	for (size_t i = 0; i < sizeof(AID_SELINUX_CTX)/sizeof(AID_SELINUX_CTX[0]); i++)
		if (strcmp(AID_SELINUX_CTX[i].user, name) == 0)
			return AID_SELINUX_CTX[i].ctx;
	return "u:r:shell:s0";
}
static bool valid_uint_str(const char *s, long *out)
{
	size_t len = strlen(s);
	if (len < 1 || len > 10)
		return false;
	for (size_t i = 0; i < len; i++) {
		if (!isdigit((unsigned char)s[i]))
			return false;
	}
	errno = 0;
	char *end;
	long v = strtol(s, &end, 10);
	if (errno || *end != '\0' || v < 0 || v > INT_MAX_VALUE)
		return false;
	if (out)
		*out = v;
	return true;
}
static void xstrcat_grow(char **buf, size_t *cap, size_t *len, const char *s, size_t n)
{
	if (*buf == NULL) {
		*cap = 1024;
		*buf = malloc(*cap);
		if (*buf == NULL) {
			perror("malloc");
			exit(1);
		}
	}
	while (*len + n + 1 > *cap) {
		*cap *= 2;
		*buf = realloc(*buf, *cap);
		if (*buf == NULL) {
			perror("realloc");
			exit(1);
		}
	}
	memcpy(*buf + *len, s, n);
	*len += n;
	(*buf)[*len] = '\0';
}
static void cmd_set_value(const char *val)
{
	size_t n = strlen(val);
	xstrcat_grow(&g_cmd, &g_cmd_cap, &g_cmd_len, val, n);
}
static void cmd_append_arg(const char *arg)
{
	size_t n = strlen(arg);
	xstrcat_grow(&g_cmd, &g_cmd_cap, &g_cmd_len, " ", 1);
	xstrcat_grow(&g_cmd, &g_cmd_cap, &g_cmd_len, arg, n);
}
static bool is_enoexec(const char *p)
{
	int fd = open(p, O_RDONLY);
	if (fd < 0)
		return true;
	unsigned char hdr[4];
	ssize_t n = read(fd, hdr, sizeof(hdr));
	close(fd);
	if (n <= 0)
		return true;
	if (n >= 4 && hdr[0] == 0x7f && hdr[1] == 'E' && hdr[2] == 'L' && hdr[3] == 'F')
		return false;
	if (hdr[0] == '#' && hdr[1] == '!') {
		char line[512];
		FILE *f = fopen(p, "r");
		if (!f)
			return true;
		if (fgets(line, sizeof(line), f) == NULL) {
			fclose(f);
			return true;
		}
		fclose(f);
		if (line[0] != '#' || line[1] != '!')
			return true;
		char *ip = line + 2;
		while (*ip == ' ' || *ip == '\t')
			ip++;
		char *end = ip;
		while (*end && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r')
			end++;
		if (end == ip)
			return true;
		*end = '\0';
		struct stat st;
		if (stat(ip, &st) == 0)
			return false;
		return true;
	}
	return true;
}
static int shell_validate(const char *s)
{
	if (s == NULL || s[0] == '\0') {
		fprintf(stderr, "Cannot execute : No such file or directory\n");
		return 1;
	}
	if (strchr(s, '/') != NULL) {
		struct stat st;
		if (stat(s, &st) != 0) {
			fprintf(stderr, "Cannot execute %s: No such file or directory\n", s);
			return 1;
		}
		if (S_ISDIR(st.st_mode)) {
			fprintf(stderr, "Cannot execute %s: Permission denied\n", s);
			return 1;
		}
		if (access(s, X_OK) != 0) {
			fprintf(stderr, "Cannot execute %s: Permission denied\n", s);
			return 1;
		}
		if (is_enoexec(s))
			return 2;
		return 0;
	}
	bool found = false;
	const char *err = "No such file or directory";
	char *path = strdup(ANDROID_PATH);
	char *save = NULL;
	for (char *d = strtok_r(path, ":", &save); d; d = strtok_r(NULL, ":", &save)) {
		char full[PATH_MAX];
		snprintf(full, sizeof(full), "%s/%s", d, s);
		struct stat st;
		if (stat(full, &st) == 0) {
			found = true;
			if (!S_ISDIR(st.st_mode) && access(full, X_OK) == 0) {
				free(path);
				if (is_enoexec(full))
					return 2;
				return 0;
			}
		}
	}
	free(path);
	if (found)
		err = "Permission denied";
	fprintf(stderr, "Cannot execute %s: %s\n", s, err);
	return 1;
}
static int run_cmd_capture_argv(const char *path, const char *arg1, const char *arg2, char **out, size_t *outlen);
static int run_cmd_capture(const char *path, const char *arg, char **out, size_t *outlen)
{
	return run_cmd_capture_argv(path, arg, NULL, out, outlen);
}
static int run_cmd_capture_argv(const char *path, const char *arg1, const char *arg2, char **out, size_t *outlen)
{
	int pipefd[2];
	if (pipe(pipefd) != 0)
		return -1;
	pid_t pid = fork();
	if (pid < 0) {
		close(pipefd[0]);
		close(pipefd[1]);
		return -1;
	}
	if (pid == 0) {
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);
		if (arg2)
			execl(path, path, arg1, arg2, (char *)NULL);
		else
			execl(path, path, arg1, (char *)NULL);
		_exit(127);
	}
	close(pipefd[1]);
	size_t cap = 8192, len = 0;
	char *buf = malloc(cap);
	if (!buf) {
		close(pipefd[0]);
		return -1;
	}
	for (;;) {
		if (len + 4096 + 1 > cap) {
			cap *= 2;
			char *nb = realloc(buf, cap);
			if (!nb) {
				free(buf);
				close(pipefd[0]);
				return -1;
			}
			buf = nb;
		}
		ssize_t n = read(pipefd[0], buf + len, cap - len - 1);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (n == 0)
			break;
		len += (size_t)n;
	}
	close(pipefd[0]);
	buf[len] = '\0';
	int status = 0;
	waitpid(pid, &status, 0);
	*out = buf;
	*outlen = len;
	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		return -1;
	return 0;
}
static int rishq_query(const char *cmd, char **out, size_t *outlen)
{
	char rishq_path[PATH_MAX];
	snprintf(rishq_path, sizeof(rishq_path), "%s/%s", g_self_dir, RISHQ_NAME);
	struct stat st;
	if (stat(rishq_path, &st) != 0)
		return -1;
	char bash_path[PATH_MAX];
	snprintf(bash_path, sizeof(bash_path), "%s/bash", g_work_dir);
	if (stat(bash_path, &st) == 0)
		return run_cmd_capture_argv(bash_path, rishq_path, cmd, out, outlen);
	return run_cmd_capture(rishq_path, cmd, out, outlen);
}
static bool real_snapshot_valid(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", g_work_dir, REAL_FILE);
	struct stat st;
	if (stat(path, &st) != 0)
		return false;
	return (time(NULL) - st.st_mtime) < 60;
}
static bool real_snapshot(void)
{
	if (real_snapshot_valid())
		return true;
	char *out = NULL;
	size_t out_len = 0;
	if (rishq_query("pm list users; echo __SEP__; pm list packages -U", &out, &out_len) != 0) {
		free(out);
		return false;
	}
	char *sep = strstr(out, "__SEP__");
	if (sep == NULL) {
		free(out);
		return false;
	}
	*sep = '\0';
	char *pkgs_part = sep + 7;
	if (*pkgs_part == '\n')
		pkgs_part++;
	char *users_csv = NULL;
	size_t users_csv_cap = 0, users_csv_len = 0;
	for (char *c = strstr(out, "UserInfo{"); c; c = strstr(c + 1, "UserInfo{")) {
		c += 9; 
		char *end = c;
		while (*end && isdigit((unsigned char)*end))
			end++;
		if (end == c)
			continue;
		if (users_csv_len)
			xstrcat_grow(&users_csv, &users_csv_cap, &users_csv_len, ",", 1);
		xstrcat_grow(&users_csv, &users_csv_cap, &users_csv_len, c, (size_t)(end - c));
	}
	if (users_csv == NULL || users_csv_len == 0) {
		free(out);
		free(users_csv);
		return false;
	}
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", g_work_dir, REAL_FILE);
	char tmp_path[PATH_MAX];
	snprintf(tmp_path, sizeof(tmp_path), "%s/%s.tmp", g_work_dir, REAL_FILE);
	FILE *f = fopen(tmp_path, "w");
	if (!f) {
		free(users_csv);
		return false;
	}
	fprintf(f, "users:%s\n", users_csv);
	free(users_csv);
	char *pline = NULL, *ptok = NULL;
	for (char *line = strtok_r(pkgs_part, "\n", &pline); line; line = strtok_r(NULL, "\n", &pline)) {
		if (strncmp(line, "package:", 8) != 0)
			continue;
		char *uidmark = strstr(line, " uid:");
		if (!uidmark)
			continue;
		uidmark += 5;
		for (char *tok = strtok_r(uidmark, ",", &ptok); tok; tok = strtok_r(NULL, ",", &ptok)) {
			bool all_digits = tok[0] != '\0';
			for (char *p = tok; *p; p++)
				if (!isdigit((unsigned char)*p)) {
					all_digits = false;
					break;
				}
			if (all_digits)
				fprintf(f, "%s\n", tok);
		}
	}
	free(out);
	fclose(f);
	rename(tmp_path, path);
	return true;
}
static bool real_u_exists(long user_no, long check_uid)
{
	if (!real_snapshot())
		return true;
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/%s", g_work_dir, REAL_FILE);
	FILE *f = fopen(path, "r");
	if (!f)
		return true;
	char line[4096];
	if (fgets(line, sizeof(line), f) == NULL) {
		fclose(f);
		return true;
	}
	fclose(f);
	char *p = line;
	if (strncmp(p, "users:", 6) != 0)
		return true;
	p += 6;
	char userbuf[32];
	snprintf(userbuf, sizeof(userbuf), "%ld", user_no);
	bool found_user = false;
	char *save = NULL;
	for (char *tok = strtok_r(p, ",\n", &save); tok; tok = strtok_r(NULL, ",\n", &save))
		if (strcmp(tok, userbuf) == 0)
			found_user = true;
	if (!found_user)
		return false;
	if (check_uid < 0)
		return true;
	char uidbuf[32];
	snprintf(uidbuf, sizeof(uidbuf), "%ld", check_uid);
	f = fopen(path, "r");
	if (!f)
		return true;
	if (fgets(line, sizeof(line), f) == NULL) {
		fclose(f);
		return true;
	}
	bool found_uid = false;
	while (fgets(line, sizeof(line), f)) {
		line[strcspn(line, "\r\n")] = '\0';
		if (strcmp(line, uidbuf) == 0)
			found_uid = true;
	}
	fclose(f);
	return found_uid;
}
static bool parse_u_user(const char *name, long *out_uid, long *out_check)
{
	const char *p = name;
	if (p[0] != 'u')
		return false;
	p++;
	char *end;
	unsigned long u = strtoul(p, &end, 10);
	if (end == p)
		return false;
	if (*end != '_')
		return false;
	char type = end[1];
	if (type != 'a' && type != 'i')
		return false;
	p = end + 2;
	errno = 0;
	unsigned long a = strtoul(p, &end, 10);
	if (end == p || errno)
		return false;
	bool cache = false;
	if (*end == '_' && strcmp(end, "_cache") == 0)
		cache = true;
	else if (*end != '\0')
		return false;
	if (type == 'a') {
		if (a > 9999)
			return false;
		long base_appid = 10000 + (long)a;
		long base = (long)(u * 100000) + base_appid;
		*out_uid = base;
		*out_check = cache ? base + 1 : base;
		return true;
	} else {
		if (a > 8999)
			return false;
		long base = (long)(u * 100000) + 90000 + (long)a;
		*out_uid = base;
		*out_check = -1;
		return true;
	}
}
static bool app_uid(const char *name, long *out_uid)
{
	long user_no = 0;
	char pkg[256];
	strncpy(pkg, name, sizeof(pkg) - 1);
	pkg[sizeof(pkg) - 1] = '\0';
	char *at = strstr(pkg, "@user");
	if (at) {
		*at = '\0';
		char *end;
		long un = strtol(at + 5, &end, 10);
		if (end != at + 5)
			user_no = un;
	}
	char query[512];
	snprintf(query, sizeof(query), "pm list packages -U");
	char *out = NULL;
	size_t outlen = 0;
	if (rishq_query(query, &out, &outlen) != 0) {
		free(out);
		return false;
	}
	char *save = NULL;
	bool ok = false;
	for (char *line = strtok_r(out, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		if (strncmp(line, "package:", 8) != 0)
			continue;
		char *sp = strchr(line + 8, ' ');
		if (!sp)
			continue;
		*sp = '\0';
		if (strcmp(line + 8, pkg) != 0)
			continue;
		char *uidmark = strstr(sp + 1, "uid:");
		if (!uidmark)
			continue;
		uidmark += 4;
		char *utok = NULL;
		for (char *tok = strtok_r(uidmark, ",", &utok); tok; tok = strtok_r(NULL, ",", &utok)) {
			errno = 0;
			char *te;
			unsigned long u = strtoul(tok, &te, 10);
			if (te == tok || errno || *te != '\0')
				continue;
			if ((long)(u / 100000) == user_no) {
				*out_uid = (long)u;
				ok = true;
				break;
			}
		}
		if (ok)
			break;
	}
	free(out);
	return ok;
}
static void ensure_bash(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/bash", g_work_dir);
	struct stat st;
	if (stat(path, &st) == 0 && st.st_size == (off_t)EMBEDDED_BASH_SIZE)
		return;
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s/bash.tmp", g_work_dir);
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		return;
	const unsigned char *p = _binary_bash_bin_start;
	size_t left = EMBEDDED_BASH_SIZE;
	while (left > 0) {
		ssize_t n = write(fd, p, left);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		p += n;
		left -= (size_t)n;
	}
	close(fd);
	rename(tmp, path);
	chmod(path, 0755);
}
static void copy_file(const char *src, const char *dst)
{
	int in = open(src, O_RDONLY);
	if (in < 0)
		return;
	int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (out < 0) {
		close(in);
		return;
	}
	char buf[65536];
	for (;;) {
		ssize_t n = read(in, buf, sizeof(buf));
		if (n <= 0)
			break;
		ssize_t off = 0;
		while (off < n) {
			ssize_t w = write(out, buf + off, (size_t)(n - off));
			if (w < 0) {
				if (errno == EINTR)
					continue;
				break;
			}
			off += w;
		}
	}
	close(out);
	close(in);
}
static void write_embedded(const char *path, const unsigned char *start, const unsigned char *end)
{
	int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		return;
	const unsigned char *p = start;
	size_t left = (size_t)(end - start);
	while (left > 0) {
		ssize_t n = write(fd, p, left);
		if (n < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		p += n;
		left -= (size_t)n;
	}
	close(fd);
}
static void ensure_libs(void)
{
	char libdir[PATH_MAX];
	snprintf(libdir, sizeof(libdir), "%s/lib", g_work_dir);
	mkdir(libdir, 0755);
	char ncu[PATH_MAX], ncu65[PATH_MAX], ncu_sym[PATH_MAX], iconv_src[PATH_MAX];
	snprintf(ncu, sizeof(ncu), "%s/libncursesw.so.6", libdir);
	snprintf(ncu65, sizeof(ncu65), "%s/libncursesw.so.6.5", libdir);
	snprintf(ncu_sym, sizeof(ncu_sym), "%s/libncursesw.so", libdir);
	snprintf(iconv_src, sizeof(iconv_src), "%s/libiconv.so", libdir);
	struct stat st_ncu;
	if (stat(ncu65, &st_ncu) != 0) {
		const char *srcs[2];
		char self_lib[PATH_MAX];
		snprintf(self_lib, sizeof(self_lib), "%s/lib", g_self_dir);
		srcs[0] = self_lib;
		srcs[1] = "/data/data/com.termux/files/usr/lib";
		for (int i = 0; i < 2; i++) {
			char s65[PATH_MAX];
			snprintf(s65, sizeof(s65), "%s/libncursesw.so.6.5", srcs[i]);
			struct stat st;
			if (stat(s65, &st) != 0)
				continue;
			copy_file(s65, ncu65);
			char sico[PATH_MAX];
			snprintf(sico, sizeof(sico), "%s/libiconv.so", srcs[i]);
			if (stat(sico, &st) == 0)
				copy_file(sico, iconv_src);
			break;
		}
		if (stat(ncu65, &st_ncu) != 0)
			write_embedded(ncu65, _binary_libncursesw_so_6_5_start, _binary_libncursesw_so_6_5_end);
	} else if (stat(iconv_src, &st_ncu) != 0) {
		const char *srcs[2];
		char self_lib[PATH_MAX];
		snprintf(self_lib, sizeof(self_lib), "%s/lib", g_self_dir);
		srcs[0] = self_lib;
		srcs[1] = "/data/data/com.termux/files/usr/lib";
		for (int i = 0; i < 2; i++) {
			char sico[PATH_MAX];
			snprintf(sico, sizeof(sico), "%s/libiconv.so", srcs[i]);
			struct stat st;
			if (stat(sico, &st) == 0) {
				copy_file(sico, iconv_src);
				break;
			}
		}
		if (stat(iconv_src, &st_ncu) != 0)
			write_embedded(iconv_src, _binary_libiconv_so_start, _binary_libiconv_so_end);
	}
	if (stat(ncu65, &st_ncu) == 0) {
		unlink(ncu);
		unlink(ncu_sym);
		symlink("libncursesw.so.6.5", ncu);
		symlink("libncursesw.so.6", ncu_sym);
	}
}
static void aid_files_setup(void)
{
	char pw[PATH_MAX], gr[PATH_MAX];
	snprintf(pw, sizeof(pw), "%s/etc_passwd", g_work_dir);
	snprintf(gr, sizeof(gr), "%s/etc_group", g_work_dir);
	struct stat st;
	if (stat(pw, &st) == 0 && st.st_size > 0 &&
	    stat(gr, &st) == 0 && st.st_size > 0)
		return;
	FILE *fp = fopen(pw, "w");
	FILE *fg = fopen(gr, "w");
	if (!fp || !fg) { if (fp) fclose(fp); if (fg) fclose(fg); return; }
	fprintf(fp, "root:x:0:0:root:/root:/system/bin/sh\n");
	fprintf(fp, "shell:x:2000:2000:shell:/data/user/0:/system/bin/sh\n");
	fprintf(fp, "system:x:1000:1000:system:/data/user/0:/system/bin/sh\n");
	fprintf(fg, "root:x:0:\n");
	fprintf(fg, "shell:x:2000:\n");
	fprintf(fg, "system:x:1000:\n");
	fprintf(fg, "input:x:1004:\n");
	fprintf(fg, "log:x:1007:\n");
	fprintf(fg, "sdcard_rw:x:1015:\n");
	fprintf(fg, "ext_data_rw:x:1078:\n");
	fprintf(fg, "ext_obb_rw:x:1079:\n");
	fclose(fp);
	fclose(fg);
}
static void ensure_current_uid_in_files(void)
{
	if (g_uid <= 0 || g_gid <= 0)
		return;
	char gr[PATH_MAX];
	snprintf(gr, sizeof(gr), "%s/etc_group", g_work_dir);
	char line[4096];
	bool found = false;
	FILE *f = fopen(gr, "r");
	if (f) {
		while (fgets(line, sizeof(line), f)) {
			char buf[64];
			snprintf(buf, sizeof(buf), "gid_%ld:x:", g_gid);
			if (strncmp(line, buf, strlen(buf)) == 0) {
				found = true;
				break;
			}
		}
		fclose(f);
	}
	if (!found) {
		FILE *fg = fopen(gr, "a");
		if (fg) {
			fprintf(fg, "gid_%ld:x:%ld:\n", g_gid, g_gid);
			fclose(fg);
		}
		char pw[PATH_MAX];
		snprintf(pw, sizeof(pw), "%s/etc_passwd", g_work_dir);
		FILE *fp = fopen(pw, "a");
		if (fp) {
			fprintf(fp, "user_%ld:x:%ld:%ld:User %ld:/data/user/0:/system/bin/sh\n",
				g_uid, g_uid, g_gid, g_uid);
			fclose(fp);
		}
	}
}
static void linker_config_setup(void)
{
	char dir[PATH_MAX], src[PATH_MAX], dst[PATH_MAX];
	snprintf(dir, sizeof(dir), "%s/linkerconfig", g_work_dir);
	snprintf(src, sizeof(src), "%s", "/linkerconfig/ld.config.txt");
	snprintf(dst, sizeof(dst), "%s/ld.config.txt", dir);
	mkdir(dir, 0755);
	struct stat ss, sd;
	if (stat(src, &ss) != 0)
		return;
	if (stat(dst, &sd) == 0 && sd.st_size == ss.st_size)
		return;
	copy_file(src, dst);
}
static void setenv_str(const char *k, const char *v)
{
	setenv(k, v, 1);
}
static void setup_ld_library_path(void)
{
	char libdir[PATH_MAX];
	snprintf(libdir, sizeof(libdir), "%s/lib", g_work_dir);
	const char *cur = getenv("LD_LIBRARY_PATH");
	char ld[4096];
	if (cur && cur[0])
		snprintf(ld, sizeof(ld), "%s:%s", libdir, cur);
	else
		snprintf(ld, sizeof(ld), "%s", libdir);
	setenv_str("LD_LIBRARY_PATH", ld);
}
/* Policy table, persisted next to the other harvest caches so a decision
 * survives across sessions the way Magisk's SQLite policies table does. */
#define POLICY_FILE ".policy"
#define POLICY_MAX 256

static const char *policy_file_path(void)
{
	static char p[PATH_MAX];
	snprintf(p, sizeof(p), "%s/%s", g_work_dir, POLICY_FILE);
	return p;
}

/* "uid policy" per line; policy is one of allow/deny/query/restrict. */
/* "uid policy" per line; policy is one of allow/deny/query/restrict.
 * Two global lines are also accepted: "root-access MODE" and
 * "multiuser MODE", written by the --root-access/--multiuser switches. */
static void policy_load_globals(void)
{
	FILE *f = fopen(policy_file_path(), "r");
	if (f == NULL)
		return;
	char line[128];
	while (fgets(line, sizeof(line), f) != NULL) {
		char what[32], val[32];
		if (sscanf(line, "root-access %31s", val) == 1) {
			if (strcmp(val, "disabled") == 0)        g_root_access = ROOT_ACCESS_DISABLED;
			else if (strcmp(val, "adb-only") == 0)   g_root_access = ROOT_ACCESS_ADB_ONLY;
			else if (strcmp(val, "apps-only") == 0)  g_root_access = ROOT_ACCESS_APPS_ONLY;
			else                                      g_root_access = ROOT_ACCESS_PROMPT;
		} else if (sscanf(line, "multiuser %31s", val) == 1) {
			if (strcmp(val, "owner-only") == 0)     g_multiuser = MULTIUSER_OWNER_ONLY;
			else if (strcmp(val, "global") == 0)     g_multiuser = MULTIUSER_GLOBAL;
			else                                     g_multiuser = MULTIUSER_OWNER_MANAGED;
		}
		(void)what;
	}
	fclose(f);
}

static enum su_policy policy_lookup(long uid, bool *found)
{
	*found = false;
	policy_load_globals();
	FILE *f = fopen(policy_file_path(), "r");
	if (f == NULL)
		return SU_POLICY_QUERY;
	char line[128];
	enum su_policy result = SU_POLICY_QUERY;
	while (fgets(line, sizeof(line), f) != NULL) {
		/* skip the global setting lines */
		if (strncmp(line, "root-access", 11) == 0
		    || strncmp(line, "multiuser", 10) == 0)
			continue;
		long u;
		char what[32];
		if (sscanf(line, "%ld %31s", &u, what) != 2)
			continue;
		if (u != uid)
			continue;
		*found = true;
		if (strcmp(what, "allow") == 0)         result = SU_POLICY_ALLOW;
		else if (strcmp(what, "deny") == 0)     result = SU_POLICY_DENY;
		else if (strcmp(what, "restrict") == 0) result = SU_POLICY_RESTRICT;
		else                                   result = SU_POLICY_QUERY;
		break;
	}
	fclose(f);
	return result;
}

static void policy_store(long uid, enum su_policy p)
{
	const char *what = "query";
	switch (p) {
	case SU_POLICY_ALLOW:    what = "allow";    break;
	case SU_POLICY_DENY:     what = "deny";     break;
	case SU_POLICY_RESTRICT: what = "restrict"; break;
	case SU_POLICY_QUERY:    what = "query";    break;
	}
	/* rewrite the single line for this uid, then append if absent */
	FILE *f = fopen(policy_file_path(), "r");
	if (f) {
		char tmp[PATH_MAX];
		snprintf(tmp, sizeof(tmp), "%s.tmp", policy_file_path());
		FILE *o = fopen(tmp, "w");
		if (o) {
			char line[128];
			int written = 0;
			while (fgets(line, sizeof(line), f) != NULL) {
				/* keep the global setting lines */
				if (strncmp(line, "root-access", 11) == 0
				    || strncmp(line, "multiuser", 10) == 0) {
					fputs(line, o);
					continue;
				}
				long u;
				if (sscanf(line, "%ld", &u) == 1 && u == uid)
					continue;	/* replaced below */
				fputs(line, o);
			}
			fprintf(o, "%ld %s\n", uid, what);
			written = 1;
			fclose(o);
			if (written) {
				rename(tmp, policy_file_path());
				fclose(f);
				return;
			}
			unlink(tmp);
		}
		fclose(f);
	}
	FILE *a = fopen(policy_file_path(), "a");
	if (a) {
		fprintf(a, "%ld %s\n", uid, what);
		fclose(a);
	}
}

/* Android user id = uid / 100000. */
static long to_user_id(long uid) { return uid / 100000; }
/* App id = uid % 100000. */
static long to_app_id(long uid) { return uid % 100000; }

#define AID_ROOT 0L
#define AID_SHELL 2000L

/*
 * Decide whether this caller may have root, following Magisk's
 * build_su_info() order: root is always allowed, the manager is allowed
 * silently, the global RootAccess mode is enforced, multiuser OwnerOnly
 * confines root to the device owner, and only then does the per-uid policy
 * table decide.  A caller with no stored decision lands on QUERY, which
 * Magisk resolves by asking its manager app; with no manager to ask it
 * denies, and so do we.
 */
static void apply_root_policy(void)
{
	if (g_caller_uid < 0)
		return;
	if (g_caller_uid == AID_ROOT) {
		/* uid 0 asking for root is root already. */
		return;
	}
	if (g_root_access == ROOT_ACCESS_DISABLED) {
		fprintf(stderr, "su: root access is disabled\n");
		exit(1);
	}
	if (g_root_access == ROOT_ACCESS_ADB_ONLY && g_caller_uid != AID_SHELL) {
		fprintf(stderr, "su: root access limited to ADB only\n");
		exit(1);
	}
	if (g_root_access == ROOT_ACCESS_APPS_ONLY && g_caller_uid == AID_SHELL) {
		fprintf(stderr, "su: root access is disabled for ADB\n");
		exit(1);
	}
	if (g_multiuser == MULTIUSER_OWNER_ONLY && to_user_id(g_caller_uid) != 0) {
		fprintf(stderr, "su: root access is limited to the device owner "
			"(user %ld)\n", to_user_id(g_caller_uid));
		exit(1);
	}

	bool found = false;
	enum su_policy pol = policy_lookup(g_caller_uid, &found);
	if (pol == SU_POLICY_DENY) {
		fprintf(stderr, "su: request rejected for uid %ld\n", g_caller_uid);
		exit(1);
	}
	if (pol == SU_POLICY_RESTRICT) {
		/* Magisk forces drop_cap for a restricted policy. */
		g_drop_cap = true;
	}
	if (pol == SU_POLICY_QUERY) {
		/* No decision stored.  Magisk would prompt its manager here;
		 * without one there is nobody to ask, and Magisk's own code
		 * path treats a missing manager as deny.  Termux is granted
		 * root explicitly below so the package is usable out of the
		 * box on a device that never ran a manager. */
		if (to_app_id(g_caller_uid) == to_app_id((long)getuid())) {
			pol = SU_POLICY_ALLOW;
		} else {
			fprintf(stderr, "su: uid %ld has no root policy; deny\n",
				g_caller_uid);
			exit(1);
		}
	}
	(void)to_app_id;
}

static void gen_fake_status(void)
{
	/* Fuse: transplant of thj_ptrace's build_fake_status().  Snapshot the
	 * supervisor's own /proc/self/status and rewrite the root-visible
	 * fields (Uid/Gid/Groups/Context/Cap*), so reads of /proc/self/status
	 * (grep, bash $'', python) look like a real root session. */
	char line[1024];
	char *buf = malloc(16384);
	if (!buf)
		return;
	size_t cap = 16384, n = 0;
	buf[0] = '\0';
	FILE *f = fopen("/proc/self/status", "r");
	if (!f) { free(buf); return; }
	const char *ctx = (g_selinux_ctx && g_selinux_ctx[0]) ? g_selinux_ctx : "u:r:shell:s0";
	while (fgets(line, sizeof(line), f)) {
		const char *repl = NULL;
		char uidbuf[64], gidbuf[64], grpbuf[512];
		if      (strncmp(line, "Name:", 5) == 0) {
			char nb[64];
			snprintf(nb, sizeof(nb), "Name:\t%s\n", g_fake_name);
			repl = nb;
		} else if (strncmp(line, "PPid:", 5) == 0) {
			/* A real root shell has no survivable parent chain to
			 * follow: seed the session as owned by pid 0. */
			repl = "PPid:\t0\n";
		} else if (strncmp(line, "Uid:", 4) == 0) {
			snprintf(uidbuf, sizeof(uidbuf), "Uid:\t%ld\t%ld\t%ld\t%ld\n",
				 g_uid, g_uid, g_uid, g_uid);
			repl = uidbuf;
		} else if (strncmp(line, "Gid:", 4) == 0) {
			snprintf(gidbuf, sizeof(gidbuf), "Gid:\t%ld\t%ld\t%ld\t%ld\n",
				 g_gid, g_gid, g_gid, g_gid);
			repl = gidbuf;
		} else if (strncmp(line, "Groups:", 7) == 0) {
			int glen = snprintf(grpbuf, sizeof(grpbuf), "Groups:\t%ld", g_gid);
			for (size_t k = 0; k < g_suppg_cnt && glen < (int)sizeof(grpbuf) - 8; k++) {
				long sv;
				if (valid_uint_str(g_suppg[k], &sv) && sv != g_gid)
					glen += snprintf(grpbuf + glen, sizeof(grpbuf) - (size_t)glen, " %ld", sv);
			}
			snprintf(grpbuf + glen, sizeof(grpbuf) - (size_t)glen, "\n");
			repl = grpbuf;
		} else if (strncmp(line, "Context:", 8) == 0) {
			char tmp[512];
			snprintf(tmp, sizeof(tmp), "Context:\t%s\n", ctx);
			repl = tmp;
		} else if (strncmp(line, "CapInh:", 7) == 0)
			repl = "CapInh:\t0000000000000000\n";
		else if (strncmp(line, "CapPrm:", 7) == 0)
			repl = g_uid == 0 && !g_drop_cap
				? "CapPrm:\t000001ffffffffff\n"
				: "CapPrm:\t0000000000000000\n";
		else if (strncmp(line, "CapEff:", 7) == 0)
			repl = g_uid == 0 && !g_drop_cap
				? "CapEff:\t000001ffffffffff\n"
				: "CapEff:\t0000000000000000\n";
		else if (strncmp(line, "CapBnd:", 7) == 0)
			repl = g_uid == 0 && !g_drop_cap
				? "CapBnd:\t000001ffffffffff\n"
				: "CapBnd:\t0000000000000000\n";
		else if (strncmp(line, "CapAmb:", 7) == 0)
			repl = g_uid == 0 && !g_drop_cap
				? "CapAmb:\t000001ffffffffff\n"
				: "CapAmb:\t0000000000000000\n";
		const char *use = repl ? repl : line;
		size_t l = strlen(use);
		if (n + l < cap) {
			memcpy(buf + n, use, l);
			n += l;
		}
	}
	fclose(f);
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/status", g_work_dir);
	unlink(path);
	FILE *sf = fopen(path, "w");
	if (sf) {
		fwrite(buf, 1, n, sf);
		fclose(sf);
	}
	free(buf);
}

/* Fake /proc/<pid>/cmdline for the session: a real su runs
 * "/system/bin/sh -c <command>", so the procfs entry must show exactly
 * that (NUL separated, trailing NUL) instead of the Termux bash path. */
static void gen_fake_cmdline(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/cmdline", g_work_dir);
	unlink(path);
	FILE *f = fopen(path, "w");
	if (f == NULL)
		return;
	static const char sh[] = "/system/bin/sh";
	fwrite(sh, 1, sizeof(sh) - 1, f);
	fputc('\0', f);
	if (g_cmd != NULL && g_cmd[0] != '\0') {
		fwrite("-c", 1, 2, f);
		fputc('\0', f);
		fwrite(g_cmd, 1, strlen(g_cmd), f);
		fputc('\0', f);
	}
	fclose(f);
}

static void rm_rf(const char *path)
{
	char buf[PATH_MAX];
	struct stat st;
	if (lstat(path, &st) != 0)
		return;
	if (S_ISDIR(st.st_mode)) {
		DIR *d = opendir(path);
		if (!d)
			return;
		struct dirent *e;
		while ((e = readdir(d)) != NULL) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
				continue;
			snprintf(buf, sizeof(buf), "%s/%s", path, e->d_name);
			rm_rf(buf);
		}
		closedir(d);
		rmdir(path);
	} else {
		unlink(path);
	}
}

/*
 * Harvest the device's real property set and freeze it into
 * <workdir>/.props.  The session's getprop/setprop/resetprop wrappers
 * then operate on this file, so ro.* can be "changed" (resetprop) and
 * every getprop answer stays self-consistent -- including the values
 * that /system/build.prop mirrors.  An empty .props would make the
 * wrappers pure pass-through relays of the device, which is correct but
 * offers no place for a fake model/serial/... to stick.
 */
static void gen_fake_props(void)
{
	char path[PATH_MAX];
	char modpath[PATH_MAX];
	snprintf(path, sizeof(path), "%s/.props", g_work_dir);
	snprintf(modpath, sizeof(modpath), "%s/.propsmod", g_work_dir);

	/* Values the user set through the session's setprop/resetprop live
	 * in the previous .props and their keys are listed in .propsmod.
	 * gen_fake_props() rebuilds .props from a fresh device harvest on
	 * every session, so without this snapshot those edits would be
	 * wiped; copy them back over the harvested ones. */
	char frozen[4096][1024];
	size_t nfrozen = 0;
	{
		FILE *mf = fopen(modpath, "r");
		if (mf) {
			FILE *pf = fopen(path, "r");
			if (pf) {
				char line[1024];
				while (nfrozen < sizeof(frozen)/sizeof(frozen[0])
				       && fgets(line, sizeof(line), pf) != NULL) {
					char *eq = strchr(line, '=');
					if (eq == NULL)
						continue;
					char key[1024], val[1024];
					size_t klen = (size_t)(eq - line);
					if (klen >= sizeof(key))
						klen = sizeof(key) - 1;
					memcpy(key, line, klen);
					key[klen] = '\0';
					strncpy(val, eq + 1, sizeof(val) - 1);
					val[sizeof(val) - 1] = '\0';
					val[strcspn(val, "\n")] = '\0';
					rewind(mf);
					char mline[1024];
					while (fgets(mline, sizeof(mline), mf) != NULL) {
						mline[strcspn(mline, "\n")] = '\0';
						if (strcmp(mline, key) == 0) {
							snprintf(frozen[nfrozen],
								 sizeof(frozen[0]),
								 "%s=%s", key, val);
							frozen[nfrozen][sizeof(frozen[0]) - 1] = '\0';
							nfrozen++;
							break;
						}
					}
				}
				fclose(pf);
			}
			fclose(mf);
		}
	}

	unlink(path);

	char *out = NULL;
	size_t outlen = 0;
	if (run_cmd_capture("/system/bin/getprop", NULL, &out, &outlen) != 0) {
		free(out);
		return;
	}

	FILE *f = fopen(path, "w");
	if (f == NULL) {
		free(out);
		return;
	}
	size_t pos = 0;
	while (pos < outlen) {
		char *nl = memchr(out + pos, '\n', outlen - pos);
		size_t len = nl ? (size_t)(nl - (out + pos)) : outlen - pos;
		if (len > 0) {
			char line[1024];
			if (len >= sizeof(line))
				len = sizeof(line) - 1;
			memcpy(line, out + pos, len);
			line[len] = '\0';
			if (line[0] == '[') {
				size_t k;
				for (k = 1; k < len; k++)
					if (line[k] == ']' && k + 4 <= len
					    && strncmp(line + k, "]: [", 4) == 0)
						break;
				if (k < len) {
					line[k] = '\0';
					char *v = line + k + 4;
					size_t vlen = strlen(v);
					if (vlen > 0 && v[vlen - 1] == ']')
						v[vlen - 1] = '\0';
					char key[1024];
					snprintf(key, sizeof(key), "%s", line + 1);
					/* skip if this key was user-edited; its
					 * frozen value is appended after the
					 * harvest below */
					bool edited = false;
					for (size_t i = 0; i < nfrozen; i++) {
						if (strncmp(frozen[i], key,
							    strlen(key)) == 0
						    && frozen[i][strlen(key)] == '=') {
							edited = true;
							break;
						}
					}
					if (!edited)
						fprintf(f, "%s=%s\n", key, v);
				}
			}
		}
		pos += len + 1;
	}
	for (size_t i = 0; i < nfrozen; i++)
		fprintf(f, "%s\n", frozen[i]);
	fclose(f);
	free(out);
}

/*
 * Mirror /system/build.prop: the real one on this device is 0600
 * root-owned and unreadable to the app domain, yet a root-like session
 * must be able to read it.  Produce a plausible file from the SAME
 * harvested property set so its values match getprop exactly, and bind
 * it over /system/build.prop.
 */
static void gen_build_prop(void)
{
	char src[PATH_MAX], dst[PATH_MAX];
	snprintf(src, sizeof(src), "%s/.props", g_work_dir);
	snprintf(dst, sizeof(dst), "%s/build.prop", g_work_dir);

	FILE *in = fopen(src, "r");
	FILE *f = fopen(dst, "w");
	if (in == NULL || f == NULL) {
		if (in) fclose(in);
		if (f) fclose(f);
		return;
	}
	fprintf(f, "# begin build properties\n");
	fprintf(f, "# autogenerated by aFakeSU\n");
	char line[1024];
	while (fgets(line, sizeof(line), in) != NULL) {
		if (strncmp(line, "ro.", 3) == 0
		    || strncmp(line, "build.", 6) == 0
		    || strncmp(line, "persist.", 8) == 0) {
			fputs(line, f);
		}
	}
	fprintf(f, "# end build properties\n");
	fclose(in);
	fclose(f);
	chmod(dst, 0644);
}

static void cleanup_workdir(void)
{
	/* A real su leaves nothing behind: drop every runtime artifact the
	 * session produced, keeping only the harvest caches (.usermap,
	 * .ugroups, .xattrs) that a following session may reuse.  The
	 * dot-prefixed cache files stay invisible to a plain `ls` anyway. */
	if (g_work_dir[0] == '\0')
		return;
	static const char *keep[] = { ".usermap", ".ugroups", ".xattrs", ".policy",
				      ".props", ".propsmod", NULL };
	DIR *d = opendir(g_work_dir);
	if (!d)
		return;
	struct dirent *e;
	char buf[PATH_MAX];
	while ((e = readdir(d)) != NULL) {
		if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
			continue;
		bool held = false;
		for (int i = 0; keep[i]; i++)
			if (strcmp(e->d_name, keep[i]) == 0)
				held = true;
		if (held)
			continue;
		snprintf(buf, sizeof(buf), "%s/%s", g_work_dir, e->d_name);
		rm_rf(buf);
	}
	closedir(d);
}

static void build_and_run_proot(void)
{
	bool enoexec_case = false;
	const char *shell = g_shell;
	const char *shell_argv0 = g_shell;
	/* Default session shell is the device's real /system/bin/sh, so the
	 * process that actually runs matches the faked /proc/<pid>/exe and
	 * comm ("sh").  The embedded bash is only an auxiliary interpreter
	 * for the rishq uid-table probe (see ensure_bash), never the session
	 * shell. */
	int rc = shell_validate(shell);
	if (rc == 1)
		exit(0);
	if (rc == 2) {
		enoexec_case = true;
		shell = "/bin/sh";
	}
	char *binds[8];
	int bind_cnt = 0;
	/* Freeze the device property set for the wrappers and mirror a
	 * readable /system/build.prop off it, BEFORE the binds below
	 * reference the generated build.prop. */
	gen_fake_props();
	gen_build_prop();
	{
		char ld[PATH_MAX];
		snprintf(ld, sizeof(ld), "%s/linkerconfig/ld.config.txt", g_work_dir);
		struct stat st;
		if (stat(ld, &st) == 0 && st.st_size > 0) {
			char *b = malloc(strlen(g_work_dir) + 64);
			sprintf(b, "-b %s/linkerconfig:/linkerconfig", g_work_dir);
			binds[bind_cnt++] = b;
		}
		char pw[PATH_MAX], grp[PATH_MAX];
		snprintf(pw, sizeof(pw), "%s/etc_passwd", g_work_dir);
		snprintf(grp, sizeof(grp), "%s/etc_group", g_work_dir);
		if (stat(pw, &st) == 0 && st.st_size > 0 &&
		    stat(grp, &st) == 0 && st.st_size > 0) {
			char *b = malloc(strlen(g_work_dir) * 2 + 64);
			sprintf(b, "-b %s/etc_passwd:/etc/passwd -b %s/etc_group:/etc/group", g_work_dir, g_work_dir);
			binds[bind_cnt++] = b;
		}
		/* The harvested /system/build.prop overlay: the original is
		 * unreadable to the app domain, and its values must mirror
		 * what getprop returns. */
		char bp[PATH_MAX];
		snprintf(bp, sizeof(bp), "%s/build.prop", g_work_dir);
		if (stat(bp, &st) == 0 && st.st_size > 0) {
			char *b = malloc(strlen(g_work_dir) + 64);
			sprintf(b, "-b %s/build.prop:/system/build.prop", g_work_dir);
			binds[bind_cnt++] = b;
		}
	}
	if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
		/* Mirror the wrapper dir at a non-Termux guest path so $PATH
		 * and `which` never reveal /data/data/com.termux/... . */
		char *gb = malloc(strlen(g_work_dir) + 64);
		if (gb != NULL) {
			sprintf(gb, "-b %s/bin:%s", g_work_dir, GUEST_BIN_DIR);
			binds[bind_cnt++] = gb;
		}
		/* The ctx file is consumed via the AFAKESU /proc redirect
		 * (readlink_proc attr-path self-binding), NOT a proot bind:
		 * a bind would let stat() show the plain file (size 14,
		 * mode 1777) instead of a procfs-shaped entry. */
		char ctx_path[PATH_MAX];
		snprintf(ctx_path, sizeof(ctx_path), "%s/ctx", g_work_dir);
		unlink(ctx_path);
		FILE *cf = fopen(ctx_path, "w");
		if (cf) {
			fprintf(cf, "%s", g_selinux_ctx);
			fclose(cf);
		}
	}
	{
		/* Fuse: the workdir fake-status file is redirected by proc.c's
		 * readlink_proc (component "status"), not by a proot bind: binds
		 * can't match /proc paths because self/thread-self canonicalize
		 * to /proc/<pid>, which is unknown at bind time. */
		char st[PATH_MAX];
		snprintf(st, sizeof(st), "%s/status", g_work_dir);
		/* Always regenerate: the status file is per-session and must
		 * reflect the current persona (uid/gid/groups/ctx/caps). */
		gen_fake_status();
		gen_fake_cmdline();

		/* Symlink used by enter.c to answer readlink("/proc/1/exe"). */
		char exe1[PATH_MAX];
		snprintf(exe1, sizeof(exe1), "%s/.exe1", g_work_dir);
		unlink(exe1);
		symlink("/system/bin/init", exe1);

		/* Fake kernel cmdline, substituted for the unreadable /proc/cmdline. */
		char cmd[PATH_MAX];
		snprintf(cmd, sizeof(cmd), "%s/.cmdline", g_work_dir);
		struct stat cst;
		if (stat(cmd, &cst) != 0 || cst.st_size <= 0) {
			FILE *mf = fopen(cmd, "w");
			if (mf) {
				fprintf(mf, "androidboot.hardware=qcom androidboot.bootdevice=soc/1d84000.ufshc "
					"androidboot.selinux=enforcing androidboot.verifiedbootstate=green "
					"androidboot.veritymode=enforcing console=ttyMSM0,115200n8 "
					"androidboot.serialno=XHA0123456 buildvariant=user\n");
				fclose(mf);
				chmod(cmd, 0444);
			}
		}

		/* SELinux global enforce state, substituted for the gated
		 * read of /sys/fs/selinux/enforce. */
		char enf[PATH_MAX];
		snprintf(enf, sizeof(enf), "%s/.enforce", g_work_dir);
		struct stat est;
		if (stat(enf, &est) != 0 || est.st_size <= 0) {
			FILE *ef = fopen(enf, "w");
			if (ef) {
				fprintf(ef, "1\n");
				fclose(ef);
				chmod(enf, 0444);
			}
		}

		/* Writtable-overlay root, plus the per-partition rw markers
		 * that model "mount -o remount,rw <part>".  A rooted phone
		 * ships system/vendor/product/odm already writable (Magisk),
		 * so seed them the same way. */
		char ovdir[PATH_MAX];
		snprintf(ovdir, sizeof(ovdir), "%s/ov", g_work_dir);
		mkdir(ovdir, 0755);

		char mntd[PATH_MAX];
		snprintf(mntd, sizeof(mntd), "%s/.mnt", g_work_dir);
		mkdir(mntd, 0755);
		static const char *def_roots[] = { "system", "vendor", "product", "odm", "root" };
		size_t di;
		for (di = 0; di < sizeof(def_roots) / sizeof(def_roots[0]); di++) {
			char mr[PATH_MAX];
			snprintf(mr, sizeof(mr), "%s/.mnt/%s", g_work_dir, def_roots[di]);
			if (access(mr, F_OK) != 0) {
				FILE *f = fopen(mr, "w");
				if (f) {
					fputs("1\n", f);
					fclose(f);
				}
			}
		}

		/* chcon/restorecon label store consumed by enter.c. */
		char xad[PATH_MAX];
		snprintf(xad, sizeof(xad), "%s/.xattrs", g_work_dir);
		mkdir(xad, 0755);
	}
	char idbuf[64];
	const char *id_arg[2];
	int id_arg_cnt = 0;
	if (g_uid == 0 && g_gid == 0) {
		id_arg[0] = "-0";
		id_arg_cnt = 1;
	} else {
		snprintf(idbuf, sizeof(idbuf), "-i %ld:%ld", g_uid, g_gid);
		id_arg[0] = idbuf;
		id_arg_cnt = 1;
	}
	const char *login_flag = NULL;
	if (!enoexec_case && g_login)
		login_flag = "-l";
	if (!g_preserve) {
		/* Clean, stock-looking PATH: the wrapper dir (mirrored at
		 * GUEST_BIN_DIR) shadows getprop/mount/chcon; the Termux
		 * prefix is deliberately kept out of the session PATH. */
		char path_env[PATH_MAX];
		snprintf(path_env, sizeof(path_env), "%s", GUEST_PATH);
		setenv_str("PATH", path_env);
		setenv_str("TMPDIR", "/data/local/tmp");
		setenv_str("TMP", "/data/local/tmp");
		if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
			char ctx_path[PATH_MAX];
			snprintf(ctx_path, sizeof(ctx_path), "%s/bin", g_work_dir);
			mkdir(ctx_path, 0755);
			/* dmesg: the kernel log is gated by CAP_SYSLOG; a fake kernel
			 * domain answers it like real root would, with plausible output. */
			char dm_path[PATH_MAX];
			snprintf(dm_path, sizeof(dm_path), "%s/dmesg", ctx_path);
			FILE *df = fopen(dm_path, "w");
			if (df) {
				fprintf(df,
					"#!/system/bin/sh\n"
					"for line in \\\n"
					"'[    0.000000] Booting Linux on physical CPU 0x0000000000' \\\n"
					"'[    0.000000] Linux version 5.15.94-android13-8-g0000000 (build@unknown) #1 SMP PREEMPT' \\\n"
					"'[    0.000000] Kernel command line: androidboot.hardware=qcom androidboot.selinux=enforcing androidboot.verifiedbootstate=green' \\\n"
					"'[    0.020062] cgroup: cgroup2 opened' \\\n"
					"'[    1.721900] init: starting service 'zygote'...' \\\n"
					"'[    5.412501] mmc0: new card done' \\\n"
					"'[    9.103812] binder: 512:512 ioctl 4 4c00 fe01 returned 0' \\\n"
					"'[   12.736114] init: Starting service 'audioserver'...' \\\n"
					"'[   16.002313] init: Starting service 'surfaceflinger'...'\n"
					"do echo \"$line\"; done\n"
					"exit 0\n");
				fclose(df);
				chmod(dm_path, 0755);
			}
			/* getenforce: the read of /sys/fs/selinux/enforce is gated
			 * by SELinux itself; a policy-loaded kernel answers. */
			char ge_path[PATH_MAX];
			snprintf(ge_path, sizeof(ge_path), "%s/getenforce", ctx_path);
			FILE *gf = fopen(ge_path, "w");
			if (gf) {
				fprintf(gf, "#!/system/bin/sh\necho Enforcing\nexit 0\n");
				fclose(gf);
				chmod(ge_path, 0755);
			}
			char path_env[PATH_MAX];
			snprintf(path_env, sizeof(path_env), "%s", GUEST_PATH);
			setenv_str("PATH", path_env);
			emit_prop_wrappers(ctx_path);
			emit_mount_wrapper(ctx_path);
			emit_chcon_wrappers(ctx_path);
		}
		char home[64];
		if (g_uid == 0)
			snprintf(home, sizeof(home), "/");
		else
			snprintf(home, sizeof(home), "/data/user/%ld", g_uid / 100000);
		setenv_str("HOME", home);
		char u0[96];
		char unamebuf[96];
		if (aid_name_for_uid(g_uid, unamebuf, sizeof(unamebuf)) != NULL
		    && strchr(unamebuf, ' ') == NULL && strcmp(unamebuf, "1000") != 0)
			snprintf(u0, sizeof(u0), "%s", unamebuf);
		else if (g_uid >= 10000 && g_uid % 100000 >= 10000)
			snprintf(u0, sizeof(u0), "u%ld_a%ld", g_uid / 100000, g_uid % 100000 - 10000);
		else
			snprintf(u0, sizeof(u0), "u0_a%ld", g_uid);
		setenv_str("LOGNAME", u0);
		setenv_str("USER", u0);
		setenv_str("SHELL", shell == g_shell ? g_shell : shell_argv0);
		setenv_str("ANDROID_BOOTLOGO", "1");
		setenv_str("ANDROID_ROOT", "/system");
		setenv_str("ANDROID_DATA", "/data");
		setenv_str("ANDROID_ASSETS", "/system/app");
		setenv_str("EXTERNAL_STORAGE", "/sdcard");
		setenv_str("ASEC_MOUNTPOINT", "/mnt/asec");
		setenv_str("LOOP_MOUNTPOINT", "/mnt/obb");
	}
	setenv_str("PROOT_TMP_DIR", g_work_dir);
	setup_ld_library_path();
	enum { MAX_ARGV = 512 };
	const char *argv[MAX_ARGV];
	int argc = 0;
	argv[argc++] = g_self_exe;
	char binpath[PATH_MAX];
	int has_bin = 0;
	if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
		snprintf(binpath, sizeof(binpath), "%s/bin", g_work_dir);
		has_bin = 1;
	}
	for (int i = 0; i < bind_cnt; i++) {
		char *b = binds[i];
		char *p = b + 2; 
		while (*p) {
			argv[argc++] = "-b";
			char *space = strchr(p, ' ');
			if (space) {
				*space = '\0';
				argv[argc++] = p;
				p = space + 1;
			} else {
				argv[argc++] = p;
				break;
			}
		}
	}
	for (int i = 0; i < id_arg_cnt; i++)
		argv[argc++] = id_arg[i];
	if (force_pty) {
		/* -i: the real su hands the session a pseudo-terminal even when
		 * -c is used.  proot cannot allocate one itself, so route the
		 * shell through util-linux `script`, which already does.  Look
		 * for it in the places a Termux install actually has it rather
		 * than assuming a path: /system/bin/script does not exist on
		 * every device (this one has no toybox script applet).  If no
		 * `script` is available we cannot honour -i, and saying so beats
		 * pretending the session got a tty. */
		static const char *pty_candidates[] = {
			"/data/data/com.termux/files/usr/bin/script",
			"/system/bin/script",
			"/bin/script",
			NULL
		};
		const char *script_bin = NULL;
		for (int i = 0; pty_candidates[i] != NULL; i++) {
			if (access(pty_candidates[i], X_OK) == 0) {
				script_bin = pty_candidates[i];
				break;
			}
		}
		if (script_bin == NULL) {
			fprintf(stderr, "su: -i needs a `script' utility to allocate a "
				"pty and none was found; install util-linux "
				"(pkg install util-linux)\n");
		} else {
			argv[argc++] = "/system/bin/sh";
			argv[argc++] = "-c";
			char ptyline[PATH_MAX * 2];
			snprintf(ptyline, sizeof(ptyline),
				"exec %s -qfc %s /dev/null",
				script_bin, shell);
			argv[argc++] = ptyline;
			argv[argc] = NULL;
			proot_quiet = true;
			exit(proot_main(argc, (char *const *)argv));
		}
	}
	argv[argc++] = shell;
	if (enoexec_case)
		argv[argc++] = shell_argv0;
	if (login_flag)
		argv[argc++] = login_flag;
	if (g_cmd == NULL) {
		const char *env_cmd = getenv("FAKESU_CMD");
		if (env_cmd && env_cmd[0]) {
			g_cmd = strdup(env_cmd);
			if (g_cmd) {
				g_cmd_len = strlen(g_cmd);
				g_cmd_cap = g_cmd_len + 1;
			}
		}
	}
	if (g_cmd != NULL) {
		argv[argc++] = "-c";
		argv[argc++] = g_cmd;
	} else if (g_pos_cnt > 1) {
		for (size_t i = 1; i < g_pos_cnt; i++)
			argv[argc++] = g_pos[i];
	}
	argv[argc] = NULL;
	proot_quiet = true;
	exit(proot_main(argc, (char *const *)argv));
}
static void parse_user_and_groups(const char *user)
{
	long uid = 0, gid = 0;
	if (user == NULL || user[0] == '\0' || strcmp(user, "root") == 0) {
		uid = 0;
		gid = 0;
	} else if (isdigit((unsigned char)user[0])) {
		const char *tmp = user;
		const char *comma = strchr(tmp, ',');
		const char *colon = strchr(tmp, ':');
		const char *sep = NULL;
		if (comma && colon)
			sep = comma < colon ? comma : colon;
		else
			sep = comma ? comma : colon;
		char uid_str[32], gid_str[32];
		if (sep) {
			size_t n = (size_t)(sep - tmp);
			if (n >= sizeof(uid_str))
				n = sizeof(uid_str) - 1;
			memcpy(uid_str, tmp, n);
			uid_str[n] = '\0';
			const char *rest = sep + 1;
			if (rest[0] == '\0') {
				strcpy(gid_str, uid_str);
			} else {
				const char *comma2 = strchr(rest, ',');
				const char *colon2 = strchr(rest, ':');
				const char *sep2 = NULL;
				if (comma2 && colon2)
					sep2 = comma2 < colon2 ? comma2 : colon2;
				else
					sep2 = comma2 ? comma2 : colon2;
				if (sep2) {
					n = (size_t)(sep2 - rest);
					if (n >= sizeof(gid_str))
						n = sizeof(gid_str) - 1;
					memcpy(gid_str, rest, n);
					gid_str[n] = '\0';
				} else {
					strncpy(gid_str, rest, sizeof(gid_str) - 1);
					gid_str[sizeof(gid_str) - 1] = '\0';
				}
			}
		} else {
			strncpy(uid_str, tmp, sizeof(uid_str) - 1);
			uid_str[sizeof(uid_str) - 1] = '\0';
			strcpy(gid_str, uid_str);
		}
		long u, g;
		if (!valid_uint_str(uid_str, &u) || !valid_uint_str(gid_str, &g)) {
			uid = 0;
			gid = 0;
		} else {
			uid = u;
			gid = g;
		}
	} else {
		bool matched = false;
		char *table = strdup(aid_merged_table());
		char *save = NULL;
		size_t ulen = strlen(user);
		for (char *line = strtok_r(table, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
			if (strncmp(line, user, ulen) == 0 && line[ulen] == ':') {
				long v;
				if (valid_uint_str(line + ulen + 1, &v)) {
					uid = v;
					gid = v;
					matched = true;
				}
				break;
			}
		}
		free(table);
		if (!matched) {
			long u, check;
			if (parse_u_user(user, &u, &check)) {
				uid = u;
				gid = u;
				if (!real_u_exists(uid / 100000, check)) {
					fprintf(stderr, "su: uid '%s' not found, falling back\n", user);
				}
			} else {
				long au;
				if (app_uid(user, &au)) {
					uid = au;
					gid = au;
				} else {
					uid = 0;
					gid = 0;
				}
			}
		}
	}
	g_uid = uid;
	g_gid = gid;
}
static void parse_options(int argc, char **argv)
{
	int i = 1;
	bool break_outer = false;
	while (i < argc && !break_outer) {
		const char *a = argv[i];
		i++;
		if (strcmp(a, "--") == 0) {
			while (i < argc)
				g_pos[g_pos_cnt++] = argv[i++];
			break;
		}
		if (strncmp(a, "--", 2) == 0 && a[2] != '\0') {
			const char *eq = strchr(a, '=');
			if (eq) {
				size_t nlen = (size_t)(eq - a) - 2;
				char name[64];
				if (nlen >= sizeof(name))
					nlen = sizeof(name) - 1;
				memcpy(name, a + 2, nlen);
				name[nlen] = '\0';
				const char *val = eq + 1;
				if (strcmp(name, "command") == 0) {
					cmd_set_value(val);
					while (i < argc)
						cmd_append_arg(argv[i++]);
					break_outer = true;
				} else if (strcmp(name, "group") == 0)
					g_grp = val;
				else if (strcmp(name, "supp-group") == 0)
					g_suppg[g_suppg_cnt++] = val;
				else if (strcmp(name, "context") == 0)
					g_selinux_ctx = selinux_ctx_norm_validate(val);
				else if (strcmp(name, "target") == 0)
					g_tgt = val;
				else if (strcmp(name, "shell") == 0)
					g_shell = val;
				else if (strcmp(name, "help") == 0)
					usage_err("su: option `--help' doesn't allow an argument");
				else if (strcmp(name, "login") == 0)
					usage_err("su: option `--login' doesn't allow an argument");
				else if (strcmp(name, "preserve-environment") == 0)
					usage_err("su: option `--preserve-environment' doesn't allow an argument");
				else if (strcmp(name, "version") == 0)
					usage_err("su: option `--version' doesn't allow an argument");
				else if (strcmp(name, "mount-master") == 0)
					usage_err("su: option `--mount-master' doesn't allow an argument");
				else
					usage_err("su: unrecognized option `--%s'", name);
			} else {
				const char *name = a + 2;
				if (strcmp(name, "command") == 0) {
					if (i >= argc)
						usage_err("su: option `--command' requires an argument");
					cmd_set_value(argv[i++]);
					while (i < argc)
						cmd_append_arg(argv[i++]);
					break_outer = true;
				} else if (strcmp(name, "group") == 0) {
					if (i >= argc)
						usage_err("su: option `--group' requires an argument");
					g_grp = argv[i++];
				} else if (strcmp(name, "supp-group") == 0) {
					if (i >= argc)
						usage_err("su: option `--supp-group' requires an argument");
					g_suppg[g_suppg_cnt++] = argv[i++];
				} else if (strcmp(name, "context") == 0) {
					if (i >= argc)
						usage_err("su: option `--context' requires an argument");
					g_selinux_ctx = selinux_ctx_norm_validate(argv[i++]);
				} else if (strcmp(name, "target") == 0) {
					if (i >= argc)
						usage_err("su: option `--target' requires an argument");
					g_tgt = argv[i++];
				} else if (strcmp(name, "shell") == 0) {
					if (i >= argc)
						usage_err("su: option `--shell' requires an argument");
					g_shell = argv[i++];
				} else if (strcmp(name, "help") == 0) {
					print_help(stdout);
					exit(0);
				} else if (strcmp(name, "login") == 0)
					g_login = true;
				else if (strcmp(name, "interactive") == 0)
					g_interactive = true;
				else if (strcmp(name, "drop-cap") == 0)
					g_drop_cap = true;
				else if (strcmp(name, "preserve-environment") == 0)
					g_preserve = true;
				else if (strcmp(name, "version") == 0) {
					printf("%s\n", VERSION_STR);
					exit(0);
				} else if (strcmp(name, "mount-master") == 0)
					g_mount = true;
				else
					usage_err("su: unrecognized option `--%s'", name);
			}
			continue;
		}
		if (a[0] == '-' && a[1] == '-' && a[2] == '\0')
			continue; 
		if (a[0] == '-' && a[1] != '\0') {
			const char *cluster = a + 1;
			/* Magisk keeps two legacy spellings working: -cn and -z are
			 * both rewritten to -Z before getopt_long ever sees them. */
			if (strcmp(cluster, "cn") == 0 || strcmp(cluster, "z") == 0) {
				if (i >= argc)
					usage_err("su: option requires an argument -- Z");
				g_selinux_ctx = selinux_ctx_norm_validate(argv[i++]);
				continue;
			}
			if (strcmp(cluster, "mm") == 0) {
				g_mount = true;
				continue;
			}
			size_t clen = strlen(cluster);
			for (size_t j = 0; j < clen; j++) {
				char ch = cluster[j];
				switch (ch) {
				case 'c':
				case 'g':
				case 'G':
				case 'Z':
				case 'z':
				case 't':
				case 's': {
					const char *arg;
					if (j + 1 < clen) {
						arg = cluster + j + 1;
						j = clen;
					} else {
						if (i >= argc) {
							char shown = (ch == 'z') ? 'Z' : ch;
							usage_err("su: option requires an argument -- %c", shown);
						}
						arg = argv[i++];
						j = clen;
					}
					switch (ch) {
					case 'c':
						cmd_set_value(arg);
						while (i < argc)
							cmd_append_arg(argv[i++]);
						break_outer = true;
						break;
					case 'g':
						g_grp = arg;
						break;
					case 'G':
						g_suppg[g_suppg_cnt++] = arg;
						break;
					case 'Z':
					case 'z':
						g_selinux_ctx = selinux_ctx_norm_validate(arg);
						break;
					case 't':
						g_tgt = arg;
						break;
					case 's':
						g_shell = arg;
						break;
					}
					break;
				}
				case 'h':
					print_help(stdout);
					exit(0);
				case 'l':
					g_login = true;
					break;
				case 'i':
					g_interactive = true;
					break;
				case 'd':
					g_drop_cap = true;
					break;
				case 'm':
				case 'p':
					g_preserve = true;
					break;
				case 'v':
					printf("%s\n", VERSION_STR);
					exit(0);
				case 'V':
					printf("%s\n", VERSION_CODE);
					exit(0);
				case 'M':
					g_mount = true;
					break;
				default:
					usage_err("su: invalid option -- %c", ch);
				}
			}
			continue;
		}
		if (a[0] == '-' && a[1] == '\0') {
			g_login = true;
			continue;
		}
		g_pos[g_pos_cnt++] = a;
	}
}
int main(int argc, char **argv)
{
	atexit(cleanup_workdir);
	if (argc > 0 && argv[0] != NULL) {
		size_t alen = strlen(argv[0]);
		if (alen > 1)
			strncpy(argv[0], "su", alen);
	}
	prctl(PR_SET_NAME, "su");
	ssize_t n = readlink("/proc/self/exe", g_self_exe, sizeof(g_self_exe) - 1);
	if (n < 0)
		n = 0;
	g_self_exe[n] = '\0';
	const char *base = strrchr(g_self_exe, '/');
	base = base ? base + 1 : g_self_exe;
	if (n == 0 || strcmp(base, "linker64") == 0 || strcmp(base, "linker") == 0) {
		if (argc > 0 && strchr(argv[0], '/')) {
			if (realpath(argv[0], g_self_exe) == NULL) {
				snprintf(g_self_exe, sizeof(g_self_exe), "%s", argv[0]);
			}
		} else {
			const char *bn = (argc > 0 && argv[0][0]) ? argv[0] : "su.elf";
			if (getcwd(g_self_exe, sizeof(g_self_exe)) == NULL)
				strcpy(g_self_exe, ".");
			strncat(g_self_exe, "/", sizeof(g_self_exe) - strlen(g_self_exe) - 1);
			strncat(g_self_exe, bn, sizeof(g_self_exe) - strlen(g_self_exe) - 1);
		}
	}
	char *slash = strrchr(g_self_exe, '/');
	if (slash) {
		*slash = '\0';
		strncpy(g_self_dir, g_self_exe, sizeof(g_self_dir) - 1);
		g_self_dir[sizeof(g_self_dir) - 1] = '\0';
	} else {
		strcpy(g_self_dir, ".");
	}
	snprintf(g_work_dir, sizeof(g_work_dir), "%s/%s", g_self_dir, WORK_DIR_NAME);
	mkdir(g_work_dir, 0700);
	chmod(g_work_dir, 0700);
	/* Wipe stale artifacts of a previous (possibly crashed) session before
	 * staging this one; caches (.usermap/.ugroups/.xattrs) survive. */
	cleanup_workdir();
	ensure_bash();
	ensure_libs();
	setup_ld_library_path();
	linker_config_setup();
	aid_files_setup();
	g_pos = malloc(sizeof(char *) * (size_t)(argc + 1));
	g_suppg = malloc(sizeof(char *) * (size_t)(argc + 1));
	if (!g_pos || !g_suppg) {
		perror("malloc");
		return 1;
	}
	aid_real_patch();

	/* Policy administration, handled before option parsing so it never
	 * starts a session:
	 *   su --policy-list
	 *   su --policy-set UID allow|deny|restrict|query
	 *   su --policy-remove UID
	 *   su --root-access prompt|disabled|adb-only|apps-only
	 *   su --multiuser owner-managed|owner-only|global
	 * A fake root that hands root to every caller would be trivially
	 * distinguishable and would not behave like the real thing, so the
	 * per-uid table is part of the normal interface. */
	if (argc >= 2 && strcmp(argv[1], "--policy-list") == 0) {
		FILE *f = fopen(policy_file_path(), "r");
		if (f == NULL) {
			printf("no stored policies\n");
			return 0;
		}
		char line[128];
		while (fgets(line, sizeof(line), f) != NULL)
			fputs(line, stdout);
		fclose(f);
		return 0;
	}
	if (argc >= 4 && strcmp(argv[1], "--policy-set") == 0) {
		long uid;
		if (!valid_uint_str(argv[2], &uid)) {
			fprintf(stderr, "su: invalid uid `%s'\n", argv[2]);
			return 1;
		}
		enum su_policy p;
		if (strcmp(argv[3], "allow") == 0)         p = SU_POLICY_ALLOW;
		else if (strcmp(argv[3], "deny") == 0)     p = SU_POLICY_DENY;
		else if (strcmp(argv[3], "restrict") == 0) p = SU_POLICY_RESTRICT;
		else if (strcmp(argv[3], "query") == 0)    p = SU_POLICY_QUERY;
		else {
			fprintf(stderr, "su: policy must be one of "
				"allow, deny, restrict, query\n");
			return 1;
		}
		policy_store(uid, p);
		printf("uid %ld -> %s\n", uid, argv[3]);
		return 0;
	}
	if (argc >= 3 && strcmp(argv[1], "--policy-remove") == 0) {
		long uid;
		if (!valid_uint_str(argv[2], &uid)) {
			fprintf(stderr, "su: invalid uid `%s'\n", argv[2]);
			return 1;
		}
		policy_store(uid, SU_POLICY_QUERY);
		printf("uid %ld -> query\n", uid);
		return 0;
	}
	if (argc >= 3 && strcmp(argv[1], "--root-access") == 0) {
		const char *m = argv[2];
		enum root_access r;
		if (strcmp(m, "prompt") == 0)        r = ROOT_ACCESS_PROMPT;
		else if (strcmp(m, "disabled") == 0) r = ROOT_ACCESS_DISABLED;
		else if (strcmp(m, "adb-only") == 0) r = ROOT_ACCESS_ADB_ONLY;
		else if (strcmp(m, "apps-only") == 0) r = ROOT_ACCESS_APPS_ONLY;
		else {
			fprintf(stderr, "su: root-access must be one of "
				"prompt, disabled, adb-only, apps-only\n");
			return 1;
		}
		FILE *f = fopen(policy_file_path(), "a");
		if (f) { fprintf(f, "root-access %s\n", m); fclose(f); }
		printf("root-access -> %s\n", m);
		return 0;
	}
	if (argc >= 3 && strcmp(argv[1], "--multiuser") == 0) {
		const char *m = argv[2];
		if (strcmp(m, "owner-managed") != 0 && strcmp(m, "owner-only") != 0
		    && strcmp(m, "global") != 0) {
			fprintf(stderr, "su: multiuser must be one of "
				"owner-managed, owner-only, global\n");
			return 1;
		}
		FILE *f = fopen(policy_file_path(), "a");
		if (f) { fprintf(f, "multiuser %s\n", m); fclose(f); }
		printf("multiuser -> %s\n", m);
		return 0;
	}

	/* The caller is the process that invoked su. */
	g_caller_uid = (long)getuid();

	parse_options(argc, argv);
	long v;
	if (g_grp != NULL && !valid_uint_str(g_grp, &v))
		invalid_gid(g_grp);
	for (size_t k = 0; k < g_suppg_cnt; k++) {
		if (!valid_uint_str(g_suppg[k], &v))
			invalid_gid(g_suppg[k]);
	}
	if (g_tgt != NULL && !valid_uint_str(g_tgt, &v))
		invalid_pid(g_tgt);
	const char *user = (g_pos_cnt > 0) ? g_pos[0] : NULL;
	parse_user_and_groups(user);
	if (g_uid < 0) {
		g_uid = 2000;
		g_gid = 2000;
	}
	if (g_grp != NULL) {
		if (!valid_uint_str(g_grp, &v))
			invalid_gid(g_grp);
		g_gid = v;
	} else if (g_suppg_cnt > 0) {
		if (!valid_uint_str(g_suppg[0], &v))
			invalid_gid(g_suppg[0]);
		g_gid = v;
	}
	/* No explicit -g/-s given: attach the groups this uid really has on the
	 * device (harvested to .ugroups) instead of assuming a single canonical
	 * gid — the real membership may be anything, not just the uid's own. */
	if (g_suppg_cnt == 0) {
		char gp[PATH_MAX];
		snprintf(gp, sizeof(gp), "%s/.ugroups", g_work_dir);
		FILE *gf = fopen(gp, "r");
		if (gf) {
			char line[1024];
			while (fgets(line, sizeof(line), gf) != NULL) {
				char *colon = strchr(line, ':');
				if (colon == NULL)
					continue;
				long u;
				char *end;
				u = strtol(line, &end, 10);
				if (end != colon || u != g_uid)
					continue;
				char *save = NULL;
				for (char *tok = strtok_r(colon + 1, " \t\r\n", &save);
				     tok != NULL && g_suppg_cnt < (size_t) argc;
				     tok = strtok_r(NULL, " \t\r\n", &save)) {
					long gv;
					if (!valid_uint_str(tok, &gv)
					    || gv < 0 || gv > 0x7fffffffL)
						continue;
					/* Primary gid stays the resolved one (AID gid == uid);
					 * the real device membership only fills the
					 * supplementary list. */
					if (gv == g_gid)
						continue;
					g_suppg[g_suppg_cnt++] = strdup(tok);
				}
				break;
			}
			fclose(gf);
		}
	}
	ensure_current_uid_in_files();
	if (g_selinux_ctx == NULL)
		g_selinux_ctx = selinux_ctx_for_user(user);
	{
		char ug[PATH_MAX];
		snprintf(ug, sizeof(ug), "%s/.uidgid", g_work_dir);
		FILE *uf = fopen(ug, "w");
		if (uf) {
			fprintf(uf, "%ld:%ld\n", g_uid, g_gid);
			fclose(uf);
		}
	}
	if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
		char ctx_path[PATH_MAX];
		snprintf(ctx_path, sizeof(ctx_path), "%s/ctx", g_work_dir);
		unlink(ctx_path);
		FILE *cf = fopen(ctx_path, "w");
		if (cf) { fprintf(cf, "%s", g_selinux_ctx); fclose(cf); }
	}
	{
		/* The session presents as a stock su shell: it really runs
		 * the device's /system/bin/sh and /proc/<pid>/exe answers the
		 * same, so comm/Name must be "sh" (never the Termux bash). */
		g_fake_name = "sh";
		snprintf(g_fake_comm, sizeof(g_fake_comm), "sh");
	}
	gen_fake_status();
	gen_fake_cmdline();
	if (g_tgt != NULL) {
		/* -t asks for the mount namespace of another process.  Real su
		 * does that with setns(), which needs privileges we do not have:
		 * fakesu runs unprivileged under proot, so a real namespace switch
		 * is impossible.  Say so instead of printing a success line that
		 * does not happen, and keep running in our own namespace. */
		fprintf(stderr, "su: cannot enter the mount namespace of PID %s: "
			"setns() requires privileges a fake root does not have; "
			"continuing in the current namespace\n", g_tgt);
	}
	if (g_mount)
		fprintf(stderr, "su: entering global mount namespace\n");
	if (g_drop_cap)
		fprintf(stderr, "su: dropping all capabilities\n");
	if (g_interactive)
		force_pty = true;
	/* The caller is whoever invoked this su, not the uid being switched
	 * to.  Enforce its policy before anything is staged for the session. */
	apply_root_policy();
	/* RESTRICT can turn drop_cap on, so the status file has to be built
	 * after the policy ran, not before. */
	gen_fake_status();
	gen_fake_cmdline();
	build_and_run_proot();
	return 0;
}
