
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
#define VERSION_STR "2c6adbc6:MAGISKSU"
#define VERSION_CODE "27000"
#define DEFAULT_SHELL "/system/bin/sh"
#define ANDROID_PATH "/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin"
#define INT_MAX_VALUE 2147483647
#define WORK_DIR_NAME ".tmp"
#define REAL_FILE "real_uids.txt"
#define RISHQ_NAME "rishq"
static char g_self_exe[PATH_MAX];
static char g_self_dir[PATH_MAX];
static char g_work_dir[PATH_MAX];
static char *g_cmd = NULL;
static size_t g_cmd_cap = 0, g_cmd_len = 0;
static const char *g_grp = NULL;
static const char **g_suppg = NULL;
static size_t g_suppg_cnt = 0;
static const char *g_ctx = NULL;
static const char *g_tgt = NULL;
static const char *g_shell = DEFAULT_SHELL;
static bool g_login = false;
static bool g_preserve = false;
static bool g_mount = false;
static const char **g_pos = NULL;
static size_t g_pos_cnt = 0;
static long g_uid = -1;
static long g_gid = -1;
static void print_help(FILE *f)
{
	fprintf(f,
		"MagiskSU\n"
		"\n"
		"Usage: su [options] [-] [user [argument...]]\n"
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
	if (stat(ncu, &st_ncu) != 0) {
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
			symlink("libncursesw.so.6.5", ncu);
			symlink("libncursesw.so.6", ncu_sym);
			char sico[PATH_MAX];
			snprintf(sico, sizeof(sico), "%s/libiconv.so", srcs[i]);
			if (stat(sico, &st) == 0)
				copy_file(sico, iconv_src);
			break;
		}
	} else {
		struct stat st_iconv;
		if (stat(iconv_src, &st_iconv) != 0) {
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
		}
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
	if (!fp || !fg) {
		if (fp)
			fclose(fp);
		if (fg)
			fclose(fg);
		return;
	}
	char *all = malloc(strlen(AID_USERS) + strlen(AID_GAPS) + 2);
	if (!all) {
		fclose(fp);
		fclose(fg);
		return;
	}
	sprintf(all, "%s\n%s", AID_USERS, AID_GAPS);
	char *save = NULL;
	for (char *line = strtok_r(all, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
		char *colon = strchr(line, ':');
		if (!colon)
			continue;
		*colon = '\0';
		fprintf(fp, "system_%s:x:%s:%s:%s:/:/system/bin/sh\n", line, colon + 1, colon + 1, line);
		fprintf(fg, "system_%s:x:%s:\n", line, colon + 1);
	}
	free(all);
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
static void build_and_run_proot(void)
{
	bool enoexec_case = false;
	const char *shell = g_shell;
	const char *shell_argv0 = g_shell;
	if (strcmp(g_shell, DEFAULT_SHELL) == 0) {
		struct stat st;
		if (stat(DEFAULT_SHELL, &st) != 0) {
			char embedded[PATH_MAX];
			snprintf(embedded, sizeof(embedded), "%s/bash", g_work_dir);
			if (stat(embedded, &st) == 0) {
				shell = strdup(embedded);
				shell_argv0 = "/system/bin/sh";
			}
		}
	}
	int rc = shell_validate(shell);
	if (rc == 1)
		exit(0);
	if (rc == 2) {
		enoexec_case = true;
		shell = "/bin/sh";
	}
	char *binds[6];
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
		setenv_str("PATH", ANDROID_PATH);
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
					g_ctx = val;
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
					g_ctx = argv[i++];
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
						g_ctx = arg;
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
	if (g_tgt != NULL)
		fprintf(stderr, "su: taking mount namespace of PID %s\n", g_tgt);
	if (g_mount)
		fprintf(stderr, "su: entering global mount namespace\n");
	build_and_run_proot();
	return 0;
}
