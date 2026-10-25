#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#define REPO_DIR  "/var/pkgman/repos"
#define REPO_FILE "/var/pkgman/repos/repos.list"
#define MAX_NAME  128
#define MAX_URL   2048

static int valid_name(const char *name)
{
    if (*name == '\0')
        return 0;

    for (const unsigned char *p = (const unsigned char *)name; *p; p++) {
        if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.')
            return 0;
    }

    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;

    return 1;
}

static int valid_url(const char *url)
{
    if (strncmp(url, "https://", 8) == 0)
        return url[8] != '\0';

    if (strncmp(url, "http://", 7) == 0)
        return url[7] != '\0';

    return 0;
}

static int repo_exists(FILE *file, const char *name)
{
    char line[MAX_NAME + MAX_URL + 4];
    char existing_name[MAX_NAME];

    rewind(file);

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || line[0] == '\n')
            continue;

        if (sscanf(line, "%127s", existing_name) == 1 &&
            strcmp(existing_name, name) == 0)
            return 1;
    }

    if (ferror(file))
        return -1;

    return 0;
}

int pkgman_add_repo(const char *name, const char *url)
{
    FILE *file;
    int exists;

    if (!name || !url ||
        strlen(name) >= MAX_NAME ||
        strlen(url) >= MAX_URL ||
        !valid_name(name) ||
        !valid_url(url)) {
        fprintf(stderr,
                "pkgman: invalid repository name or URL\n"
                "Usage: pkgman add-repo <name> <http(s)://url>\n");
        return 1;
    }

    if (mkdir("/var/pkgman", 0755) == -1 && errno != EEXIST) {
        perror("pkgman: mkdir /var/pkgman");
        return 1;
    }

    if (mkdir(REPO_DIR, 0755) == -1 && errno != EEXIST) {
        perror("pkgman: mkdir repository directory");
        return 1;
    }

    file = fopen(REPO_FILE, "a+");
    if (!file) {
        perror("pkgman: opening repository list");
        return 1;
    }

    exists = repo_exists(file, name);
    if (exists != 0) {
        if (exists > 0)
            fprintf(stderr, "pkgman: repository '%s' already exists\n",
                    name);
        else
            perror("pkgman: reading repository list");

        fclose(file);
        return 1;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        perror("pkgman: seeking repository list");
        fclose(file);
        return 1;
    }

    /* Ensure the new entry starts on its own line. */
    long end = ftell(file);
    if (end < 0) {
        perror("pkgman: checking repository list");
        fclose(file);
        return 1;
    }

    if (end > 0) {
        if (fseek(file, -1, SEEK_END) != 0) {
            perror("pkgman: checking final line");
            fclose(file);
            return 1;
        }

        int last = fgetc(file);
        if (last != '\n' && fputc('\n', file) == EOF) {
            perror("pkgman: writing repository list");
            fclose(file);
            return 1;
        }
    }

    if (fprintf(file, "%s %s\n", name, url) < 0 ||
        fflush(file) != 0) {
        perror("pkgman: writing repository list");
        fclose(file);
        return 1;
    }

    if (fclose(file) != 0) {
        perror("pkgman: closing repository list");
        return 1;
    }

    printf("Added repository '%s'\n", name);
    return 0;
}