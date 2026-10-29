#include "shell.h"
#include "console.h"
#include "keyboard.h"
#include "pmm.h"
#include "fs.h"
#include "process.h"
#include "timer.h"
#include <stddef.h>
#include <stdint.h>

#define INPUT_MAX 128
static char input_buffer[INPUT_MAX];
static size_t input_len = 0;

static int str_eq(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; } return *a == *b;
}
static int str_starts_with(const char *s, const char *p) {
    while (*p) { if (*s != *p) return 0; s++; p++; } return 1;
}
static char to_lower(char c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int contains_word(const char *str, const char *word) {
    while (*str) {
        while (*str == ' ') str++;
        if (!*str) break;
        const char *w = word, *s = str;
        while (*w && *s && to_lower(*s) == to_lower(*w)) { s++; w++; }
        if (*w == 0 && (*s == 0 || *s == ' ')) return 1;
        while (*str && *str != ' ') str++;
    }
    return 0;
}
static int contains_phrase(const char *str, const char *phrase) {
    while (*str) {
        const char *p = phrase, *s = str;
        while (*p && *s && to_lower(*s) == to_lower(*p)) { s++; p++; }
        if (*p == 0) return 1;
        str++;
    }
    return 0;
}
static const char *last_word(const char *str) {
    const char *last = str, *s = str;
    while (*s) { if (*s == ' ') last = s + 1; s++; }
    return (last == str || !*last) ? NULL : last;
}
static int parse_int(const char *s) {
    int v = 0; while (*s >= '0' && *s <= '9') { v = v*10 + (*s-'0'); s++; } return v;
}

static void print_prompt(void) {
    console_set_color(0x00FFAA);
    console_print("natunux:");
    console_set_color(0x55AAFF);
    fs_print_path();
    console_set_color(0x00FFAA);
    console_print("> ");
    console_set_color(0xFFFFFF);
}

static void cmd_help(void) {
    console_set_color(0x55FF55);
    console_print("FS: ls write read rm cd mkdir\n");
    console_print("Proc: ps create process kill yield userdemo runuser\n");
    console_print("Sys: help clear mem about ticks uptime sleep\n\n");
}
static void cmd_clear(void) { console_clear(0x0A0A1A); }
static void cmd_mem(void) {
    console_print("Total "); console_print_dec(pmm_get_total_memory()/1024/1024);
    console_print(" Free "); console_print_dec(pmm_get_free_memory()/1024/1024);
    console_print(" MiB\n");
}
static void cmd_about(void) {
    console_set_color(0x00FFCC);
    console_print("Natunux v1.5\n");
    console_set_color(0xCCCCCC);
    console_print("Natural-language-first experimental OS.\n");
    console_print("Kernel, shell, FS, processes, dual-mode.\n\n");
}
static void cmd_ticks(void) { console_print("Ticks: "); console_print_dec(timer_get_ticks()); console_print("\n"); }
static void cmd_uptime(void) { console_print("Uptime: "); console_print_dec(timer_get_ticks()/100); console_print(" s\n"); }
static void cmd_sleep(const char *a) {
    int ms = parse_int(a); if (ms<=0) ms=1000;
    console_print("Sleeping...\n"); timer_sleep(ms); console_print("Done\n");
}
static void cmd_ls(void) { fs_list(); }
static void cmd_mkdir(const char *n) {
    if (n && fs_create_folder(n)) { console_set_color(0x55FF55); console_print("OK\n"); }
    else { console_set_color(0xFF5555); console_print("Fail\n"); }
}
static void cmd_write(const char *name, const char *content) {
    if (name && fs_write_file(name, content?content:"")) {
        console_set_color(0x55FF55); console_print("Wrote "); console_print(name); console_print("\n");
    } else { console_set_color(0xFF5555); console_print("Fail\n"); }
}
static void cmd_read(const char *n) {
    char buf[512];
    if (fs_read_file(n, buf, sizeof(buf))) {
        if (!buf[0]) console_print("(empty)\n"); else { console_print(buf); console_print("\n"); }
    } else { console_set_color(0xFF5555); console_print("Not found\n"); }
}
static void cmd_rm(const char *n) {
    if (fs_delete(n)) { console_set_color(0x55FF55); console_print("Deleted\n"); }
    else { console_set_color(0xFF5555); console_print("Fail\n"); }
}
static void cmd_cd(const char *p) {
    if (!fs_change_dir(p)) { console_set_color(0xFF5555); console_print("Not found\n"); }
}
static void cmd_ps(void) { process_list(); }
static void cmd_mkproc(const char *n) {
    int pid = process_create(n);
    if (pid>=0) { console_set_color(0x55FF55); console_print("PID "); console_print_dec(pid); console_print("\n"); }
    else { console_set_color(0xFF5555); console_print("Fail\n"); }
}
static void cmd_kill(const char *a) {
    if (process_kill(parse_int(a))==0) { console_set_color(0x55FF55); console_print("Killed\n"); }
    else { console_set_color(0xFF5555); console_print("Fail\n"); }
}
static void cmd_yield(void) { process_yield(); }
static void cmd_userdemo(void) { process_create_user_demo("userdemo"); }
static void cmd_runuser(void) { process_enter_user_demo(); }

static int try_nl(const char *line) {
    if (contains_word(line,"help")) { cmd_help(); return 1; }
    if (contains_word(line,"clear")) { cmd_clear(); return 1; }
    if (contains_word(line,"mem")||contains_word(line,"memory")) { cmd_mem(); return 1; }
    if (contains_word(line,"about")) { cmd_about(); return 1; }
    if (contains_word(line,"ticks")) { cmd_ticks(); return 1; }
    if (contains_word(line,"uptime")) { cmd_uptime(); return 1; }
    if (contains_word(line,"ls")||contains_phrase(line,"show files")) { cmd_ls(); return 1; }
    if (contains_word(line,"ps")||contains_phrase(line,"show processes")) { cmd_ps(); return 1; }
    if (contains_word(line,"yield")) { cmd_yield(); return 1; }
    if (contains_word(line,"userdemo")) { cmd_userdemo(); return 1; }
    if (contains_word(line,"runuser")||contains_phrase(line,"run user")) { cmd_runuser(); return 1; }
    if (contains_phrase(line,"create process")) {
        const char *n=last_word(line); if(n&&!str_eq(n,"process")) cmd_mkproc(n); return 1;
    }
    if (contains_word(line,"kill")) { const char *n=last_word(line); if(n) cmd_kill(n); return 1; }
    if (contains_phrase(line,"create folder")||contains_phrase(line,"make folder")) {
        const char *n=last_word(line); if(n&&!str_eq(n,"folder")) cmd_mkdir(n); return 1;
    }
    if (contains_word(line,"delete")||contains_word(line,"remove")) {
        const char *n=last_word(line); if(n) cmd_rm(n); return 1;
    }
    if (contains_word(line,"read")||contains_word(line,"open")) {
        const char *n=last_word(line); if(n) cmd_read(n); return 1;
    }
    if (contains_phrase(line,"go to")||str_starts_with(line,"cd ")) {
        const char *n=str_starts_with(line,"cd ")?line+3:last_word(line);
        while(n&&*n==' ')n++; if(n) cmd_cd(n); return 1;
    }
    if (contains_phrase(line,"go up")||str_eq(line,"cd ..")) { cmd_cd(".."); return 1; }
    if (contains_word(line,"sleep")) { const char *n=last_word(line); cmd_sleep(n?n:"1000"); return 1; }
    return 0;
}

static void process_command(char *line) {
    while (*line==' ') line++;
    if (!*line) return;
    if (str_eq(line,"help")) { cmd_help(); return; }
    if (str_eq(line,"clear")) { cmd_clear(); return; }
    if (str_eq(line,"mem")) { cmd_mem(); return; }
    if (str_eq(line,"about")) { cmd_about(); return; }
    if (str_eq(line,"ticks")) { cmd_ticks(); return; }
    if (str_eq(line,"uptime")) { cmd_uptime(); return; }
    if (str_eq(line,"ls")) { cmd_ls(); return; }
    if (str_eq(line,"ps")) { cmd_ps(); return; }
    if (str_eq(line,"yield")) { cmd_yield(); return; }
    if (str_eq(line,"userdemo")) { cmd_userdemo(); return; }
    if (str_eq(line,"runuser")) { cmd_runuser(); return; }
    if (str_eq(line,"pwd")) { fs_print_path(); console_print("\n"); return; }
    if (str_starts_with(line,"write ")) {
        char *p=line+6; while(*p==' ')p++;
        char *name=p; while(*p&&*p!=' ')p++;
        if(*p){*p=0;p++;while(*p==' ')p++;}
        cmd_write(name,p); return;
    }
    if (str_starts_with(line,"sleep ")) { cmd_sleep(line+6); return; }
    if (str_starts_with(line,"kill ")) { cmd_kill(line+5); return; }
    if (str_starts_with(line,"mkdir ")) { cmd_mkdir(line+6); return; }
    if (str_starts_with(line,"cd ")) { cmd_cd(line+3); return; }
    if (str_starts_with(line,"rm ")) { cmd_rm(line+3); return; }
    if (str_starts_with(line,"read ")) { cmd_read(line+5); return; }
    if (try_nl(line)) return;
    console_set_color(0xFF5555);
    console_print("Unknown: "); console_print(line); console_print("\n");
}

void shell_run(void) {
    console_set_color(0x00FFCC);
    console_print("Natunux Shell v1.5\n");
    console_set_color(0xCCCCCC);
    console_print("Natural language + classic commands supported.\n\n");
    print_prompt();
    for (;;) {
        if (!keyboard_has_char()) { asm volatile("hlt"); continue; }
        char c = keyboard_getchar();
        if (c=='\n') {
            console_print("\n");
            input_buffer[input_len]=0;
            process_command(input_buffer);
            input_len=0;
            print_prompt();
        } else if (c=='\b') {
            if (input_len>0) { input_len--; console_print("\b \b"); }
        } else if (c>=32 && c<127) {
            if (input_len<INPUT_MAX-1) {
                input_buffer[input_len++]=c;
                char b[2]={c,0}; console_print(b);
            }
        }
    }
}
