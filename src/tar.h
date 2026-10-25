
#ifndef PKGMAN_TAR_H
#define PKGMAN_TAR_H

#include <stdint.h>
#include <sys/types.h>

#define PKGMAN_TAR_BLOCK_SIZE 512
#define PKGMAN_TAR_NAME_MAX   256
#define PKGMAN_TAR_LINK_MAX   101

struct pkgman_tar_entry {
    char name[PKGMAN_TAR_NAME_MAX];
    char linkname[PKGMAN_TAR_LINK_MAX];
    uint64_t size;
    mode_t mode;
    char type;
};

/*
 * Read the next entry from a TAR archive.
 *
 * fd must refer to an uncompressed TAR archive positioned at
 * the beginning of the next header.
 *
 * Returns:
 *   1  Entry successfully read
 *   0  End of archive
 *  -1  Invalid archive or I/O error
 *
 * After a successful call, the file descriptor is positioned at
 * the beginning of the entry's data. Call pkgman_tar_skip()
 * or read the data and its padding before reading the next entry.
 */
int pkgman_tar_next(int fd, struct pkgman_tar_entry *entry);

/*
 * Skip an entry's data and block padding.
 *
 * Call this after pkgman_tar_next() when you do not need the
 * entry's contents.
 *
 * Returns 0 on success, -1 on error.
 */
int pkgman_tar_skip(int fd, uint64_t size);

/*
 * List the contents of a TAR archive to stdout.
 *
 * fd must refer to an uncompressed TAR archive.
 * The function consumes the archive from its current position.
 *
 * Returns 0 on success, -1 on error.
 */
int pkgman_tar_list(int fd);

/*
 * Extract a TAR archive into an existing destination directory.
 *
 * destination must be an absolute path to an existing directory.
 * Only regular files and directories are extracted.
 *
 * Existing files are not overwritten.
 *
 * Returns 0 on success, -1 on error.
 */
int pkgman_tar_extract(int fd, const char *destination);

#endif
