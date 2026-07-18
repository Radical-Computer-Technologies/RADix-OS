#ifndef RAD_SYSROOT_DIRENT_H
#define RAD_SYSROOT_DIRENT_H

struct dirent {
    unsigned char d_type;
    char d_name[256];
};

typedef struct DIR DIR;

#define DT_REG 1
#define DT_DIR 2
#define DT_LNK 3
#define DT_UNKNOWN 0

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
int closedir(DIR *dirp);
int dirfd(DIR *dirp);

#endif
