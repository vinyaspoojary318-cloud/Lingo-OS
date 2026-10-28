#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define FS_MAX_NAME     32
#define FS_MAX_CHILDREN 16
#define FS_MAX_NODES    128
#define FS_MAX_CONTENT  256   // max text content per file

struct fs_node {
    char name[FS_MAX_NAME];
    bool is_directory;
    struct fs_node *parent;
    struct fs_node *children[FS_MAX_CHILDREN];
    size_t child_count;

    // For files only
    char content[FS_MAX_CONTENT];
    size_t content_len;
};

// Initialize the filesystem (creates root "/")
void fs_init(void);

// Get current working directory
struct fs_node *fs_get_cwd(void);

// Create a folder in the current directory
bool fs_create_folder(const char *name);

// Create a file in the current directory (empty or with content)
bool fs_create_file(const char *name, const char *content);

// Delete a file or empty folder by name in current directory
bool fs_delete(const char *name);

// Change directory
bool fs_change_dir(const char *path);

// List contents of current directory
void fs_list(void);

// Print current path
void fs_print_path(void);

// Read file content (returns false if not found or is directory)
bool fs_read_file(const char *name, char *out, size_t max_len);
