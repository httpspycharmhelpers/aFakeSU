/* aFakeSU: 验证 proot 嵌入接口 (proot_main + proot_quiet)
 * 用法: test_embed <quiet:0|1> <args...> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cli/cli.h"
#include "cli/note.h"

int main(int argc, char **argv)
{
	int quiet = atoi(argv[1]);

	proot_quiet = quiet;

	fprintf(stderr, "[harness] calling proot_main(quiet=%d)\n", quiet);
	return proot_main(argc - 2, argv + 2);
}
