#include "kheap.h"
#include "pmm.h"
#include "console.h"

// Very simple bump + free-list heap on top of PMM pages
#define HEAP_PAGES 64
#define HEAP_SIZE  (HEAP_PAGES * 4096ULL)

struct heap_block {
    size_t size;
    int free;
    struct heap_block *next;
};

static uint8_t *heap_base = NULL;
static struct heap_block *head = NULL;

void kheap_init(void) {
    uint64_t phys = pmm_alloc_pages(HEAP_PAGES);
    if (!phys) {
        console_set_color(0xFF5555);
        console_print("[FAIL] kheap: no memory\n");
        return;
    }
    heap_base = (uint8_t *)pmm_phys_to_virt(phys);
    head = (struct heap_block *)heap_base;
    head->size = HEAP_SIZE - sizeof(struct heap_block);
    head->free = 1;
    head->next = NULL;

    console_set_color(0x55FF55);
    console_print("[OK] Kernel heap (");
    console_print_dec(HEAP_PAGES * 4);
    console_print(" KiB)\n");
}

void *kmalloc(size_t size) {
    if (!head || size == 0) return NULL;
    // Align to 8
    size = (size + 7) & ~7ULL;

    struct heap_block *cur = head;
    while (cur) {
        if (cur->free && cur->size >= size) {
            // Split if large enough
            if (cur->size >= size + sizeof(struct heap_block) + 8) {
                struct heap_block *n = (struct heap_block *)((uint8_t *)cur + sizeof(struct heap_block) + size);
                n->size = cur->size - size - sizeof(struct heap_block);
                n->free = 1;
                n->next = cur->next;
                cur->size = size;
                cur->next = n;
            }
            cur->free = 0;
            return (void *)((uint8_t *)cur + sizeof(struct heap_block));
        }
        cur = cur->next;
    }
    return NULL;
}

void kfree(void *ptr) {
    if (!ptr || !heap_base) return;
    struct heap_block *b = (struct heap_block *)((uint8_t *)ptr - sizeof(struct heap_block));
    b->free = 1;

    // Simple merge with next
    if (b->next && b->next->free) {
        b->size += sizeof(struct heap_block) + b->next->size;
        b->next = b->next->next;
    }
}
