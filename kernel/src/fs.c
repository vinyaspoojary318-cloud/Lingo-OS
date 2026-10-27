#include "fs.h"
#include "console.h"
#include <stddef.h>

static struct fs_node nodes[FS_MAX_NODES];
static size_t node_count = 0;
static struct fs_node *root = NULL;
static struct fs_node *cwd = NULL;

static void str_copy(char *dest, const char *src, size_t max) {
    size_t i = 0;
    while (src[i] && i < max - 1) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = 0;
}

static int str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return *a == *b;
}

static struct fs_node *alloc_node(const char *name, bool is_dir, struct fs_node *parent) {
    if (node_count >= FS_MAX_NODES) return NULL;

    struct fs_node *n = &nodes[node_count++];
    str_copy(n->name, name, FS_MAX_NAME);
    n->is_directory = is_dir;
    n->parent = parent;
    n->child_count = 0;
    for (int i = 0; i < FS_MAX_CHILDREN; i++) n->children[i] = NULL;

    if (parent && parent->child_count < FS_MAX_CHILDREN) {
        parent->children[parent->child_count++] = n;
    }

    return n;
}

void fs_init(void) {
    node_count = 0;
    root = alloc_node("/", true, NULL);
    cwd = root;

    // Create a few default folders so it feels alive
    fs_create_folder("home");
    fs_create_folder("docs");
    fs_create_folder("system");
}

struct fs_node *fs_get_cwd(void) {
    return cwd;
}

bool fs_create_folder(const char *name) {
    if (!name || !*name) return false;
    if (str_eq(name, ".") || str_eq(name, "..")) return false;

    // Check if already exists
    for (size_t i = 0; i < cwd->child_count; i++) {
        if (str_eq(cwd->children[i]->name, name)) {
            return false; // already exists
        }
    }

    if (cwd->child_count >= FS_MAX_CHILDREN) return false;

    struct fs_node *n = alloc_node(name, true, cwd);
    return n != NULL;
}

bool fs_change_dir(const char *path) {
    if (!path || !*path) return false;

    if (str_eq(path, "/")) {
        cwd = root;
        return true;
    }

    if (str_eq(path, "..")) {
        if (cwd->parent) cwd = cwd->parent;
        return true;
    }

    if (str_eq(path, ".")) {
        return true;
    }

    // Look for child
    for (size_t i = 0; i < cwd->child_count; i++) {
        if (str_eq(cwd->children[i]->name, path) && cwd->children[i]->is_directory) {
            cwd = cwd->children[i];
            return true;
        }
    }

    return false;
}

void fs_list(void) {
    if (cwd->child_count == 0) {
        console_set_color(0x888888);
        console_print("(empty)\n");
        return;
    }

    for (size_t i = 0; i < cwd->child_count; i++) {
        struct fs_node *n = cwd->children[i];
        if (n->is_directory) {
            console_set_color(0x55AAFF);
            console_print("[DIR]  ");
        } else {
            console_set_color(0xCCCCCC);
            console_print("       ");
        }
        console_print(n->name);
        console_print("\n");
    }
    console_print("\n");
}

void fs_print_path(void) {
    // Build path by walking up to root
    char parts[16][FS_MAX_NAME];
    int depth = 0;

    struct fs_node *n = cwd;
    while (n && n != root && depth < 16) {
        str_copy(parts[depth], n->name, FS_MAX_NAME);
        depth++;
        n = n->parent;
    }

    console_print("/");
    for (int i = depth - 1; i >= 0; i--) {
        console_print(parts[i]);
        if (i > 0) console_print("/");
    }
}
