#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FS_MAX_NAME     32
#define FS_MAX_CHILDREN 16
#define FS_MAX_NODES    64

struct fs_node {
    char name[FS_MAX_NAME];
    bool is_directory;
    struct fs_node *parent;
    struct fs_node *children[FS_MAX_CHILDREN];
    size_t child_count;
};

// Initialize the filesystem (creates root "/")
void fs_init(void);

// Get current working directory
struct fs_node *fs_get_cwd(void);

// Create a folder in the current directory. Returns true on success.
bool fs_create_folder(const char *name);

// Change directory. Supports "..", absolute and relative names.
bool fs_change_dir(const char *path);

// List contents of current directory
void fs_list(void);

// Print current path (e.g. /home/docs)
void fs_print_path(void);
