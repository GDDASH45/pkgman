
#include <stdio.h>
#include <string.h>

#define PKGMAN_VERSION "0.1.0-rc0"

static void print_help(const char *program)
{
    printf("pkgman %s\n", PKGMAN_VERSION);
    printf("Usage: %s <command> [arguments]\n\n", program);
    puts("Commands:");
    puts("  help       Display this help message");
    puts("  version    Display pkgman version");
    puts("  list       List installed packages");
    puts("  install    Install a package");
    puts("  remove     Remove an installed package");
    puts("  upgrade    Upgrade installed packages");
}

static int command_version(void)
{
    printf("pkgman %s\n", PKGMAN_VERSION);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        print_help(argv[0]);
        return 0;
    }

    const char *command = argv[1];

    if (strcmp(command, "help") == 0 ||
        strcmp(command, "--help") == 0 ||
        strcmp(command, "-h") == 0) {
        print_help(argv[0]);
        return 0;
    }

    if (strcmp(command, "version") == 0 ||
        strcmp(command, "--version") == 0) {
        return command_version();
    }

    if (strcmp(command, "list") == 0 ||
        strcmp(command, "install") == 0 ||
        strcmp(command, "remove") == 0 ||
        strcmp(command, "upgrade") == 0) {
        fprintf(stderr,
                "pkgman: command '%s' is not implemented yet\n",
                command);
        return 1;
    }

    fprintf(stderr, "pkgman: unknown command '%s'\n", command);
    fprintf(stderr, "Run '%s help' for usage.\n", argv[0]);

    return 2;
}
