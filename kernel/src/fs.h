#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FS_MAX_NAME     32
#define FS_MAX_CHILDREN 16
#define FS_MAX_NODES    128
#define FS_MAX_CONTENT  512

struct fs_node {
    char name[FS_MAX_NAME];
    bool is_directory;
    struct fs_node *parent;
    struct fs_node *children[FS_MAX_CHILDREN];
    size_t child_count;
    char content[FS_MAX_CONTENT];
    size_t content_len;
};

void fs_init(void);
struct fs_node *fs_get_cwd(void);
bool fs_create_folder(const char *name);
bool fs_create_file(const char *name, const char *content);
bool fs_write_file(const char *name, const char *content);
bool fs_delete(const char *name);
bool fs_change_dir(const char *path);
void fs_list(void);
void fs_print_path(void);
bool fs_read_file(const char *name, char *out, size_t max_len);
