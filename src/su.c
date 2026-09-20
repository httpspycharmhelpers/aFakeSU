
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <ctype.h>
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
extern const char _binary_terhijack_bin_start[];
extern const char _binary_terhijack_bin_end[];
extern const unsigned char _binary_libiconv_so_start[];
extern const unsigned char _binary_libiconv_so_end[];
extern const unsigned char _binary_libncursesw_so_6_5_start[];
extern const unsigned char _binary_libncursesw_so_6_5_end[];
#define VERSION_STR "2c6adbc6:AFAKESU"
#define VERSION_CODE "27000"
#define DEFAULT_SHELL "/system/bin/sh"
#define ANDROID_PATH "/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin"
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
static const char **g_pos = NULL;
static size_t g_pos_cnt = 0;
static long g_uid = -1;
static long g_gid = -1;
const char *g_selinux_ctx = NULL;

/* /system/bin/id would report the *real* process context (getselfattr is
 * outside PRoot's interception), so the session's `id` is our own sh
 * wrapper that answers like a genuine root shell. */
static void emit_id_wrapper(FILE *f, const char *ctx)
{
	fprintf(f, "#!/system/bin/sh\n"
		"case \"$*\" in\n"
		"  *-Z*) echo '%s' ;;\n"
		"  *-un) echo root ;;\n"
		"  *-u*) echo 0 ;;\n"
		"  *-g*) echo 0 ;;\n"
		"  *-G) echo 0 ;;\n"
		"  *) echo \"uid=0(root) gid=0(root) groups=0(root) context=%s\" ;;\n"
		"esac\n",
		ctx, ctx);
}

/*
 * Persisted property overrides.  Real root flips read-only props with
 * resetprop/setprop (ro.debuggable, ro.build.*, ro.product.*, ...); a
 * fake root owns a store that shadows the real property service for every
 * process under the session's PATH (getprop/resetprop/setprop first).
 */
static void emit_prop_wrappers(const char *ctx_path)
{
	const char *wd = g_work_dir ? g_work_dir : "/data/local/tmp/termux";
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
			"  X=\n"
			"  while IFS='=' read -r k v; do X=\"${X}${k}|\"; done < \"$P\"\n"
			"  X=$(echo \"$X\" | sed 's/|$//')\n"
			"  if [ -n \"$X\" ]; then\n"
			"    /system/bin/getprop \"$@\" | grep -vE \"^\\[($X)\\]\"\n"
			"  else\n"
			"    /system/bin/getprop \"$@\"\n"
			"  fi\n"
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
				"name=\"\"; val=\"\"; args=0\n"
				"for a in \"$@\"; do\n"
				"  case \"$a\" in -*) : ;; *)\n"
				"    args=$((args+1))\n"
				"    [ $args -eq 1 ] && name=\"$a\"\n"
				"    [ $args -eq 2 ] && val=\"$a\"\n"
				"  esac\n"
				"done\n"
				"if [ $args -ge 2 ] && [ -n \"$name\" ]; then\n"
				"  [ -f \"$P\" ] && sed -i \"/^$name=/d\" \"$P\"\n"
				"  echo \"$name=$val\" >> \"$P\"\n"
				"  exit 0\n"
				"fi\n"
				"if [ $args -eq 1 ] && [ -n \"$name\" ] && [ -f \"$P\" ]; then\n"
				"  while IFS='=' read -r k v; do\n"
				"    [ \"$k\" = \"$name\" ] && { echo \"$v\"; exit 0; }\n"
				"  done < \"$P\"\n"
				"fi\n"
				"exec /system/bin/%s \"$@\"\n",
				st, names[i]);
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
		g_work_dir ? g_work_dir : "/data/local/tmp/termux");
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
		"  -g, --group GROUP             Specify the primary group\n"
		"  -G, --supp-group GROUP        Specify a supplementary group.\n"
		"                                The first specified supplementary group is also used\n"
		"                                as a primary group if the option -g is not specified.\n"
		"  -Z, --context CONTEXT         Change SELinux context\n"
		"  -t, --target PID              PID to take mount namespace from\n"
		"  -h, --help                    Display this help message and exit\n"
		"  -, -l, --login                Pretend the shell to be a login shell\n"
		"  -m, -p,\n"
		"  --preserve-environment        Preserve the entire environment\n"
		"  -s, --shell SHELL             Use SHELL instead of the default /system/bin/sh\n"
		"  -v, --version                 Display version number and exit\n"
		"  -V                            Display version code and exit\n"
		"  -mm, -M,\n"
		"  --mount-master                Force run in the global mount namespace\n"
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
	"uprobestuds:1093\n"
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
	{ "system", "u:r:system_app:s0" },
	{ "radio", "u:r:radio:s0" },
	{ "bluetooth", "u:r:mtk_hal_bluetooth:s0" },
	{ "wifi", "u:r:wificond:s0" },
	{ "media", "u:r:mediametrics:s0" },
	{ "keystore", "u:r:keystore:s0" },
	{ "drm", "u:r:drmserver:s0" },
	{ "gps", "u:r:mnld:s0" },
};
static bool valid_uint_str(const char *s, long *out);
static const char *aid_name_for_uid(long uid, char *name, size_t namesz)
{
	char *table = strdup(AID_USERS);
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
static void ensure_terhijack(void)
{
	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/terhijack", g_work_dir);
	struct stat st;
	if (stat(path, &st) == 0)
		return;
	char tmp[PATH_MAX];
	snprintf(tmp, sizeof(tmp), "%s/terhijack.tmp", g_work_dir);
	int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0755);
	if (fd < 0)
		return;
	const unsigned char *p = (const unsigned char *)_binary_terhijack_bin_start;
	size_t left = (size_t)(_binary_terhijack_bin_end - _binary_terhijack_bin_start);
	while (left > 0) {
		ssize_t n = write(fd, p, left);
		if (n < 0) {
			if (errno == EINTR) continue;
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
			repl = "CapPrm:\t000001ffffffffff\n";
		else if (strncmp(line, "CapEff:", 7) == 0)
			repl = "CapEff:\t000001ffffffffff\n";
		else if (strncmp(line, "CapBnd:", 7) == 0)
			repl = "CapBnd:\t000001ffffffffff\n";
		else if (strncmp(line, "CapAmb:", 7) == 0)
			repl = "CapAmb:\t000001ffffffffff\n";
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
	FILE *sf = fopen(path, "w");
	if (sf) {
		fwrite(buf, 1, n, sf);
		fclose(sf);
	}
	free(buf);
}

static void build_and_run_proot(void)
{
	bool enoexec_case = false;
	const char *shell = g_shell;
	const char *shell_argv0 = g_shell;
	if (strcmp(g_shell, DEFAULT_SHELL) == 0) {
		/*
		 * $SHELL was unusable (or absent): fall back to our embedded
		 * bash as the auxiliary session shell.  It carries the stock
		 * argv0 "/system/bin/sh" so ps/comm keep looking like the
		 * device's default mksh (TerHijack hooks need bash syntax the
		 * real mksh would reject with "syntax error: unexpected ('").
		 */
		struct stat st;
		char embedded[PATH_MAX];
		snprintf(embedded, sizeof(embedded), "%s/bash", g_work_dir);
		if (stat(embedded, &st) == 0) {
			shell = strdup(embedded);
			shell_argv0 = "/system/bin/sh";
		}
	}
	if (shell != g_shell) {
		strncpy(g_proot_argv0, shell_argv0, sizeof(g_proot_argv0) - 1);
		g_proot_argv0[sizeof(g_proot_argv0) - 1] = '\0';
		g_proot_argv0_fix = 1;
	}
	int rc = shell_validate(shell);
	if (rc == 1)
		exit(0);
	if (rc == 2) {
		enoexec_case = true;
		shell = "/bin/sh";
	}
	char *binds[8];
	int bind_cnt = 0;
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
	}
	if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
		/* The ctx file is consumed via the AFAKESU /proc redirect
		 * (readlink_proc "attr/*" self-binding), NOT a proot bind:
		 * a bind would let stat() show the plain file (size 14,
		 * mode 1777) instead of a procfs-shaped entry. */
		char ctx_path[PATH_MAX];
		snprintf(ctx_path, sizeof(ctx_path), "%s/ctx", g_work_dir);
		FILE *cf = fopen(ctx_path, "w");
		if (cf) {
			fprintf(cf, "%s\n", g_selinux_ctx);
			fclose(cf);
		}
	}
	{
		/* Fuse: the workdir fake-status file is redirected by proc.c's
		 * readlink_proc (component "status"), not by a proot bind: binds
		 * can't match /proc paths because self/thread-self canonicalize
		 * to /proc/<pid>, which is unknown at bind time. */
		struct stat sst;
		char st[PATH_MAX];
		snprintf(st, sizeof(st), "%s/status", g_work_dir);
		if (stat(st, &sst) != 0 || sst.st_size <= 0)
			gen_fake_status();

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
		char tmppath[PATH_MAX];
		snprintf(tmppath, sizeof(tmppath), "%s/bin", g_work_dir);
		const char *cur = getenv("PATH");
		char path_env[PATH_MAX];
		snprintf(path_env, sizeof(path_env), "%s:/sbin:/system/bin:/system/xbin:/system/sbin:%s",
			 tmppath, cur ? cur : ANDROID_PATH);
		setenv_str("PATH", path_env);
		if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
			char ctx_path[PATH_MAX];
			snprintf(ctx_path, sizeof(ctx_path), "%s/bin", g_work_dir);
			mkdir(ctx_path, 0755);
			char id_wrapper[PATH_MAX];
			snprintf(id_wrapper, sizeof(id_wrapper), "%s/id", ctx_path);
			FILE *f = fopen(id_wrapper, "w");
			if (f) {
				emit_id_wrapper(f, g_selinux_ctx);
				fclose(f);
				chmod(id_wrapper, 0755);
			}
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
			const char *cur = getenv("PATH");
			snprintf(path_env, sizeof(path_env), "%s:%s", ctx_path, cur ? cur : ANDROID_PATH);
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
		char u0[32];
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
char *wrapped = malloc(strlen(g_cmd) + 1024);
		if (wrapped) {
			char thpath[PATH_MAX];
			snprintf(thpath, sizeof(thpath), "%s/terhijack", g_work_dir);
			snprintf(wrapped, strlen(g_cmd) + 1024,
				"export __THJ_BIN=%s && "
				"eval \"$(%s --init 2>/dev/null)\" && "
				"eval \"$(%s -c 'id -Z' -o '%s' 2>/dev/null)\" && "
				"eval \"$(%s -c 'cat /proc/self/attr/current' -o '%s' 2>/dev/null)\" && "
				"eval \"$(%s -c 'ls -Z /' -r 'ls /' 2>/dev/null)\" && "
				"eval \"$(%s -c 'ps -Z' -r 'ps' 2>/dev/null)\" && "
				"eval \"$(%s -c 'getenforce' -o 'Enforcing' 2>/dev/null)\" && "
				"%s",
				thpath, thpath,
				g_selinux_ctx ? g_selinux_ctx : "u:r:shell:s0",
				thpath, g_selinux_ctx ? g_selinux_ctx : "u:r:shell:s0",
				thpath,
				thpath,
				thpath,
				thpath,
				g_cmd);
			argv[argc++] = wrapped;
		} else {
			argv[argc++] = g_cmd;
		}
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
		char uid_str[32], gid_str[32];
		if (comma) {
			size_t n = (size_t)(comma - tmp);
			if (n >= sizeof(uid_str))
				n = sizeof(uid_str) - 1;
			memcpy(uid_str, tmp, n);
			uid_str[n] = '\0';
			const char *rest = comma + 1;
			const char *comma2 = strchr(rest, ',');
			if (comma2) {
				n = (size_t)(comma2 - rest);
				if (n >= sizeof(gid_str))
					n = sizeof(gid_str) - 1;
				memcpy(gid_str, rest, n);
				gid_str[n] = '\0';
			} else {
				strncpy(gid_str, rest, sizeof(gid_str) - 1);
				gid_str[sizeof(gid_str) - 1] = '\0';
			}
			if (gid_str[0] == '\0')
				strcpy(gid_str, uid_str);
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
		char *table = strdup(AID_USERS);
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
					g_selinux_ctx = val;
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
					g_selinux_ctx = argv[i++];
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
						g_selinux_ctx = arg;
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
	mkdir(g_work_dir, 0777);
	chmod(g_work_dir, 0777);
	ensure_bash();
	ensure_libs();
	ensure_terhijack();
	setup_ld_library_path();
	linker_config_setup();
	aid_files_setup();
	g_pos = malloc(sizeof(char *) * (size_t)(argc + 1));
	g_suppg = malloc(sizeof(char *) * (size_t)(argc + 1));
	if (!g_pos || !g_suppg) {
		perror("malloc");
		return 1;
	}
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
	ensure_current_uid_in_files();
	if (g_selinux_ctx == NULL)
		g_selinux_ctx = selinux_ctx_for_user(user);
	if (g_selinux_ctx != NULL && g_selinux_ctx[0] != '\0') {
		char ctx_path[PATH_MAX];
		snprintf(ctx_path, sizeof(ctx_path), "%s/ctx", g_work_dir);
		FILE *cf = fopen(ctx_path, "w");
		if (cf) { fprintf(cf, "%s\n", g_selinux_ctx); fclose(cf); }
		char idp[PATH_MAX];
		snprintf(idp, sizeof(idp), "%s/bin", g_work_dir);
		mkdir(idp, 0755);
		char idf[PATH_MAX];
		snprintf(idf, sizeof(idf), "%s/id", idp);
		FILE *f = fopen(idf, "w");
		if (f) {
			emit_id_wrapper(f, g_selinux_ctx);
			fclose(f); chmod(idf, 0755);
		}
	}
	{
		/*
		 * Session shell = the caller's current shell ($SHELL), so the
		 * su session runs the user's real environment; the embedded bash
		 * is only an auxiliary fallback (unset/unusable $SHELL).  The
		 * stock /system/bin/sh (mksh) would reject the bash-only TerHijack
		 * hook scripts, hence the embedded fallback keeps argv0 "/system/bin/sh".
		 * The $SHELL binary must be executable by the *current* run user
		 * (a termux-private 0700 bash is not, under rishq's uid 2000).
		 */
		const char *env_shell = getenv("SHELL");
		if (strcmp(g_shell, DEFAULT_SHELL) == 0
		    && env_shell != NULL && env_shell[0] != '\0'
		    && strcmp(env_shell, DEFAULT_SHELL) != 0
		    && strcmp(env_shell, "/bin/sh") != 0) {
			/* A "sh"-named shell is the stock mksh (it rejects the
			 * bash-only TerHijack hooks): treat it as the default and
			 * let the embedded bash stay as the auxiliary session
			 * shell.  Any other current shell that is executable by
			 * the current run user wins. */
			const char *bn = strrchr(env_shell, '/');
			bn = bn ? bn + 1 : env_shell;
			if (strcmp(bn, "sh") != 0 && access(env_shell, X_OK) == 0)
				g_shell = env_shell;
		}
	}
	{
		const char *s = g_shell;
		if (s == NULL || strcmp(s, DEFAULT_SHELL) == 0)
			g_fake_name = "sh";
		else {
			const char *p = strrchr(s, '/');
			g_fake_name = (p && p[1]) ? p + 1 : s;
		}
	}
	gen_fake_status();
	if (g_tgt != NULL)
		fprintf(stderr, "su: taking mount namespace of PID %s\n", g_tgt);
	if (g_mount)
		fprintf(stderr, "su: entering global mount namespace\n");
	build_and_run_proot();
	return 0;
}
