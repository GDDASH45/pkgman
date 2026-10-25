
#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "tar.h"

#define PKGMAN_VERSION "0.1.0-rc0"

int pkgman_add_repo(const char *name, const char *url);

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
    puts("  add-repo   Add a package repository");
    puts("  list-tar   List the contents of a TAR archive");
    puts("  extract-tar Extract a TAR archive into a directory");
}

static int command_version(void)
{
    printf("pkgman %s\n", PKGMAN_VERSION);
    return 0;
}

static int command_list_tar(const char *archive)
{
    int fd = open(archive, O_RDONLY);

    if (fd < 0) {
        fprintf(stderr, "pkgman: cannot open '%s': %s\n",
                archive, strerror(errno));
        return 1;
    }

    int result = pkgman_tar_list(fd);

    if (result < 0) {
        fprintf(stderr, "pkgman: failed to read TAR archive '%s': %s\n",
                archive, strerror(errno));
    }

    close(fd);
    return result < 0 ? 1 : 0;
}

static int command_extract_tar(const char *archive,
                               const char *destination)
{
    int fd = open(archive, O_RDONLY);

    if (fd < 0) {
        fprintf(stderr, "pkgman: cannot open '%s': %s\n",
                archive, strerror(errno));
        return 1;
    }

    int result = pkgman_tar_extract(fd, destination);

    if (result < 0) {
        fprintf(stderr, "pkgman: failed to extract '%s': %s\n",
                archive, strerror(errno));
    }

    close(fd);
    return result < 0 ? 1 : 0;
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

    if (strcmp(command, "add-repo") == 0) {
        if (argc != 4) {
            fprintf(stderr,
                    "Usage: %s add-repo <name> <url>\n",
                    argv[0]);
            return 2;
        }

        return pkgman_add_repo(argv[2], argv[3]);
    }

    if (strcmp(command, "list-tar") == 0) {
        if (argc != 3) {
            fprintf(stderr, "Usage: %s list-tar <archive>\n",
                    argv[0]);
            return 2;
        }

        return command_list_tar(argv[2]);
    }

    if (strcmp(command, "extract-tar") == 0) {
        if (argc != 4) {
            fprintf(stderr,
                    "Usage: %s extract-tar <archive> <destination>\n",
                    argv[0]);
            return 2;
        }

        return command_extract_tar(argv[2], argv[3]);
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
