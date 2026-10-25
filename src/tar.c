
#define _POSIX_C_SOURCE 200809L

#include "tar.h"

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif

struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

_Static_assert(sizeof(struct tar_header) == PKGMAN_TAR_BLOCK_SIZE,
               "TAR header must be exactly 512 bytes");

static int read_exact(int fd, void *buffer, size_t length)
{
    unsigned char *p = buffer;

    while (length > 0) {
        ssize_t n = read(fd, p, length);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0) {
            errno = EIO;
            return -1;
        }

        p += (size_t)n;
        length -= (size_t)n;
    }

    return 0;
}

static int write_exact(int fd, const void *buffer, size_t length)
{
    const unsigned char *p = buffer;

    while (length > 0) {
        ssize_t n = write(fd, p, length);

        if (n < 0) {
            if (errno == EINTR)
                continue;

            return -1;
        }

        if (n == 0) {
            errno = EIO;
            return -1;
        }

        p += (size_t)n;
        length -= (size_t)n;
    }

    return 0;
}

static int skip_bytes(int fd, uint64_t length)
{
    unsigned char buffer[4096];

    while (length > 0) {
        size_t chunk = length > sizeof(buffer)
                     ? sizeof(buffer)
                     : (size_t)length;

        if (read_exact(fd, buffer, chunk) < 0)
            return -1;

        length -= chunk;
    }

    return 0;
}

int pkgman_tar_skip(int fd, uint64_t size)
{
    uint64_t padding;

    if (fd < 0) {
        errno = EINVAL;
        return -1;
    }

    padding = (PKGMAN_TAR_BLOCK_SIZE -
               size % PKGMAN_TAR_BLOCK_SIZE) %
              PKGMAN_TAR_BLOCK_SIZE;

    if (skip_bytes(fd, size) < 0)
        return -1;

    return skip_bytes(fd, padding);
}

static int parse_octal(const char *field, size_t length,
                       uint64_t *value)
{
    uint64_t result = 0;
    size_t i = 0;
    int digits = 0;

    if (!field || !value || length == 0) {
        errno = EINVAL;
        return -1;
    }

    /* Accept leading spaces and NUL padding. */
    while (i < length &&
           (field[i] == ' ' || field[i] == '\0'))
        i++;

    for (; i < length && field[i] != '\0' && field[i] != ' '; i++) {
        unsigned digit;

        if (field[i] < '0' || field[i] > '7') {
            errno = EINVAL;
            return -1;
        }

        digit = (unsigned)(field[i] - '0');

        if (result > (UINT64_MAX - digit) / 8) {
            errno = EOVERFLOW;
            return -1;
        }

        result = result * 8 + digit;
        digits = 1;
    }

    if (!digits) {
        errno = EINVAL;
        return -1;
    }

    *value = result;
    return 0;
}

static int all_zero(const unsigned char *block)
{
    for (size_t i = 0; i < PKGMAN_TAR_BLOCK_SIZE; i++) {
        if (block[i] != 0)
            return 0;
    }

    return 1;
}

static int valid_checksum(const struct tar_header *header)
{
    const unsigned char *bytes =
        (const unsigned char *)header;

    uint64_t expected;
    uint64_t sum = 0;
    size_t checksum_offset =
        offsetof(struct tar_header, checksum);

    if (parse_octal(header->checksum,
                    sizeof(header->checksum),
                    &expected) < 0)
        return 0;

    for (size_t i = 0; i < sizeof(*header); i++) {
        if (i >= checksum_offset &&
            i < checksum_offset + sizeof(header->checksum))
            sum += ' ';
        else
            sum += bytes[i];
    }

    return sum == expected;
}

static int copy_field(char *destination, size_t destination_size,
                      const char *source, size_t source_size)
{
    size_t length = 0;

    while (length < source_size && source[length] != '\0')
        length++;

    if (length >= destination_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(destination, source, length);
    destination[length] = '\0';

    return 0;
}

int pkgman_tar_next(int fd, struct pkgman_tar_entry *entry)
{
    struct tar_header header;
    unsigned char block[PKGMAN_TAR_BLOCK_SIZE];
    uint64_t size;
    uint64_t mode;

    if (fd < 0 || !entry) {
        errno = EINVAL;
        return -1;
    }

    if (read_exact(fd, block, sizeof(block)) < 0)
        return -1;

    if (all_zero(block))
        return 0;

    memcpy(&header, block, sizeof(header));

    if (!valid_checksum(&header)) {
        errno = EINVAL;
        return -1;
    }

    if (parse_octal(header.size, sizeof(header.size), &size) < 0 ||
        parse_octal(header.mode, sizeof(header.mode), &mode) < 0)
        return -1;

    memset(entry, 0, sizeof(*entry));

    if (header.prefix[0] != '\0') {
        int n = snprintf(entry->name, sizeof(entry->name),
                         "%.*s/%.*s",
                         (int)sizeof(header.prefix), header.prefix,
                         (int)sizeof(header.name), header.name);

        if (n < 0 || (size_t)n >= sizeof(entry->name)) {
            errno = ENAMETOOLONG;
            return -1;
        }
    } else if (copy_field(entry->name, sizeof(entry->name),
                          header.name, sizeof(header.name)) < 0) {
        return -1;
    }

    if (copy_field(entry->linkname, sizeof(entry->linkname),
                   header.linkname, sizeof(header.linkname)) < 0)
        return -1;

    entry->size = size;
    entry->mode = (mode_t)(mode & 0777);
    entry->type = header.typeflag;

    return 1;
}

static int valid_path(const char *path)
{
    const char *p;

    if (!path || !*path || path[0] == '/') {
        errno = EINVAL;
        return 0;
    }

    p = path;

    while (*p) {
        const char *start = p;
        size_t length;

        while (*p && *p != '/')
            p++;

        length = (size_t)(p - start);

        if (length == 0 ||
            (length == 1 && start[0] == '.') ||
            (length == 2 && start[0] == '.' && start[1] == '.')) {
            errno = EINVAL;
            return 0;
        }

        if (*p == '/') {
            p++;

            if (*p == '\0')
                break;
        }
    }

    return 1;
}

/*
 * Walk a relative path from a directory descriptor.
 * Each component is opened without following symlinks.
 *
 * When create is nonzero, missing intermediate directories
 * are created with mode 0755.
 */
static int open_parent_directory(int rootfd, const char *path,
                                 int create, char *basename,
                                 size_t basename_size)
{
    char copy[PKGMAN_TAR_NAME_MAX];
    char *component;
    char *slash;
    int current;

    if (!valid_path(path))
        return -1;

    size_t length = strlen(path);

    if (length >= sizeof(copy)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    memcpy(copy, path, length + 1);

    slash = strrchr(copy, '/');

    if (!slash) {
        if (strlen(copy) >= basename_size) {
            errno = ENAMETOOLONG;
            return -1;
        }

        strcpy(basename, copy);

        current = dup(rootfd);
        return current;
    }

    *slash = '\0';

    if (strlen(slash + 1) >= basename_size) {
        errno = ENAMETOOLONG;
        return -1;
    }

    strcpy(basename, slash + 1);

    current = dup(rootfd);

    if (current < 0)
        return -1;

    component = copy;

    while (*component) {
        char *next = strchr(component, '/');

        if (next)
            *next = '\0';

        int nextfd = openat(current, component,
                            O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

        if (nextfd < 0 && errno == ENOENT && create) {
            if (mkdirat(current, component, 0755) < 0 &&
                errno != EEXIST) {
                int saved_errno = errno;
                close(current);
                errno = saved_errno;
                return -1;
            }

            nextfd = openat(current, component,
                            O_RDONLY | O_DIRECTORY | O_NOFOLLOW);
        }

        if (nextfd < 0) {
            int saved_errno = errno;
            close(current);
            errno = saved_errno;
            return -1;
        }

        close(current);
        current = nextfd;

        if (!next)
            break;

        component = next + 1;
    }

    return current;
}

static int extract_directory(int rootfd,
                             const struct pkgman_tar_entry *entry)
{
    char basename[PKGMAN_TAR_NAME_MAX];
    int parentfd;
    int result;

    parentfd = open_parent_directory(rootfd, entry->name, 1,
                                     basename, sizeof(basename));

    if (parentfd < 0)
        return -1;

    result = mkdirat(parentfd, basename, 0755);

    if (result < 0 && errno != EEXIST) {
        int saved_errno = errno;
        close(parentfd);
        errno = saved_errno;
        return -1;
    }

    int dirfd = openat(parentfd, basename,
                       O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

    if (dirfd < 0) {
        int saved_errno = errno;
        close(parentfd);
        errno = saved_errno;
        return -1;
    }

    result = fchmod(dirfd, entry->mode & 0777);

    int saved_errno = errno;

    close(dirfd);
    close(parentfd);

    if (result < 0)
        errno = saved_errno;

    return result;
}

static int extract_regular_file(int rootfd,
                                const struct pkgman_tar_entry *entry,
                                int archivefd)
{
    char basename[PKGMAN_TAR_NAME_MAX];
    unsigned char buffer[4096];
    uint64_t remaining = entry->size;
    int parentfd;
    int out;

    parentfd = open_parent_directory(rootfd, entry->name, 1,
                                    basename, sizeof(basename));

    if (parentfd < 0)
        return -1;

    out = openat(parentfd, basename,
                 O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW,
                 0600);

    if (out < 0) {
        int saved_errno = errno;
        close(parentfd);
        errno = saved_errno;
        return -1;
    }

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer)
                     ? sizeof(buffer)
                     : (size_t)remaining;

        if (read_exact(archivefd, buffer, chunk) < 0 ||
            write_exact(out, buffer, chunk) < 0) {
            int saved_errno = errno;
            close(out);
            close(parentfd);
            errno = saved_errno;
            return -1;
        }

        remaining -= chunk;
    }

    if (fchmod(out, entry->mode & 0777) < 0) {
        int saved_errno = errno;
        close(out);
        close(parentfd);
        errno = saved_errno;
        return -1;
    }

    if (close(out) < 0) {
        int saved_errno = errno;
        close(parentfd);
        errno = saved_errno;
        return -1;
    }

    if (close(parentfd) < 0)
        return -1;

    uint64_t padding =
        (PKGMAN_TAR_BLOCK_SIZE -
         entry->size % PKGMAN_TAR_BLOCK_SIZE) %
        PKGMAN_TAR_BLOCK_SIZE;

    return skip_bytes(archivefd, padding);
}

int pkgman_tar_list(int fd)
{
    struct pkgman_tar_entry entry;
    int result;

    for (;;) {
        result = pkgman_tar_next(fd, &entry);

        if (result == 0)
            return 0;

        if (result < 0)
            return -1;

        printf("%07o %" PRIu64 " %s\n",
               (unsigned)entry.mode,
               entry.size,
               entry.name);

        if (pkgman_tar_skip(fd, entry.size) < 0)
            return -1;
    }
}

int pkgman_tar_extract(int fd, const char *destination)
{
    struct pkgman_tar_entry entry;
    struct stat st;
    int rootfd;
    int result;

    if (fd < 0 || !destination || destination[0] != '/') {
        errno = EINVAL;
        return -1;
    }

    rootfd = open(destination, O_RDONLY | O_DIRECTORY | O_NOFOLLOW);

    if (rootfd < 0)
        return -1;

    if (fstat(rootfd, &st) < 0 || !S_ISDIR(st.st_mode)) {
        int saved_errno = errno ? errno : ENOTDIR;
        close(rootfd);
        errno = saved_errno;
        return -1;
    }

    for (;;) {
        result = pkgman_tar_next(fd, &entry);

        if (result == 0) {
            close(rootfd);
            return 0;
        }

        if (result < 0) {
            int saved_errno = errno;
            close(rootfd);
            errno = saved_errno;
            return -1;
        }

        if (entry.size > INT64_MAX) {
            close(rootfd);
            errno = EFBIG;
            return -1;
        }

        if (entry.type == '5') {
            if (entry.size != 0 ||
                extract_directory(rootfd, &entry) < 0) {
                int saved_errno = errno;
                close(rootfd);
                errno = saved_errno;
                return -1;
            }

            continue;
        }

        if (entry.type == '\0' || entry.type == '0') {
            if (extract_regular_file(rootfd, &entry, fd) < 0) {
                int saved_errno = errno;
                close(rootfd);
                errno = saved_errno;
                return -1;
            }

            continue;
        }

        /*
         * Unsupported entry type. Skip its payload so the
         * caller can receive a clear error without interpreting
         * links or device entries as regular files.
         */
        if (pkgman_tar_skip(fd, entry.size) < 0) {
            int saved_errno = errno;
            close(rootfd);
            errno = saved_errno;
            return -1;
        }

        close(rootfd);
        errno = ENOTSUP;
        return -1;
    }
}
