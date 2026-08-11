/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 mckulipaniang
 * Contact: mckulipaniang mckulipaniang@163.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>

#define MAX_CMD 512
#define MAX_ARGS 64

void print_prompt() {
    char cwd[256];
    getcwd(cwd, sizeof(cwd));
    
    for (int i = 0; cwd[i]; i++) {
        if (cwd[i] == '/') cwd[i] = '\\';
    }
    
    printf("%s> ", cwd);
}

void sigint_handler(int sig) {
    printf("\nCtrl+C is blocked. Type 'exit' to leave.\n");
    print_prompt();
    fflush(stdout);
}

void sigquit_handler(int sig) {
    printf("\nCtrl+\\ is blocked. Type 'exit' to leave.\n");
    print_prompt();
    fflush(stdout);
}

void cmd_dir(char **args) {
    DIR *d;
    struct dirent *dir;
    struct stat st;
    char *path = ".";
    int wide = 0;
    
    for (int i = 1; args[i] != NULL; i++) {
        if (strcmp(args[i], "/w") == 0 || strcmp(args[i], "-w") == 0) {
            wide = 1;
        } else if (args[i][0] != '/' && args[i][0] != '-') {
            path = args[i];
        }
    }
    
    d = opendir(path);
    if (d == NULL) {
        printf("Directory not found: %s\n", path);
        return;
    }
    
    struct winsize w;
    int term_width = 80;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        term_width = w.ws_col;
    }
    
    if (!wide) {
        printf("\n Volume in drive is Termux\n");
        printf(" Volume Serial Number: 1234-5678\n\n");
        printf(" Directory of %s\n\n", path);
    }
    
    int file_count = 0, dir_count = 0;
    long total_size = 0;
    int max_name_len = 0;
    
    while ((dir = readdir(d)) != NULL) {
        char fullpath[512];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
        if (stat(fullpath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                dir_count++;
            } else {
                file_count++;
                total_size += st.st_size;
            }
            int len = strlen(dir->d_name);
            if (len > max_name_len) max_name_len = len;
        }
    }
    
    rewinddir(d);
    
    if (wide) {
        int col = 0;
        int width = max_name_len + 2;
        if (width < 12) width = 12;
        int cols = term_width / width;
        if (cols < 1) cols = 1;
        
        while ((dir = readdir(d)) != NULL) {
            if (dir->d_name[0] != '.') {
                printf("%-*s", width, dir->d_name);
                col++;
                if (col % cols == 0) printf("\n");
            }
        }
        if (col % cols != 0) printf("\n");
        printf("\n  %d File(s)\n", file_count);
    } else {
        int time_width = 17;
        int size_width = 10;
        int gap = 2;
        
        int available = term_width - size_width - time_width - gap * 2;
        int name_width = max_name_len;
        
        if (name_width > available) {
            name_width = available;
        }
        if (name_width < 8) name_width = 8;
        
        printf("  %-*s  %-*s  %s\n", size_width, "Size", name_width, "Name", "Modified");
        printf("  %-*s  %-*s  %s\n", size_width, "----", name_width, "----", "--------");
        
        while ((dir = readdir(d)) != NULL) {
            char fullpath[512];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", path, dir->d_name);
            if (stat(fullpath, &st) == 0) {
                char time_str[64];
                struct tm *tm_info = localtime(&st.st_mtime);
                strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
                
                char display_name[256];
                strncpy(display_name, dir->d_name, name_width);
                display_name[name_width] = '\0';
                
                if (S_ISDIR(st.st_mode)) {
                    printf("  %-*s  %-*s  %s\n", size_width, "<DIR>", name_width, display_name, time_str);
                } else {
                    printf("  %-*ld  %-*s  %s\n", size_width, st.st_size, name_width, display_name, time_str);
                }
            }
        }
        printf("\n  %d File(s)  %ld bytes\n", file_count, total_size);
        printf("  %d Dir(s)\n", dir_count);
    }
    
    closedir(d);
}

void cmd_type(char **args) {
    if (args[1] == NULL) {
        printf("Usage: type <filename>\n");
        return;
    }
    
    FILE *fp = fopen(args[1], "r");
    if (fp == NULL) {
        printf("File not found: %s\n", args[1]);
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), fp) != NULL) {
        printf("%s", line);
    }
    fclose(fp);
}

void cmd_copy(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        printf("Usage: copy <source> <destination>\n");
        return;
    }
    
    FILE *src = fopen(args[1], "rb");
    if (src == NULL) {
        printf("Source file not found: %s\n", args[1]);
        return;
    }
    
    FILE *dst = fopen(args[2], "wb");
    if (dst == NULL) {
        printf("Cannot create destination file: %s\n", args[2]);
        fclose(src);
        return;
    }
    
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dst);
    }
    
    fclose(src);
    fclose(dst);
    printf("1 file(s) copied.\n");
}

void cmd_del(char **args) {
    if (args[1] == NULL) {
        printf("Usage: del <filename>\n");
        return;
    }
    
    if (remove(args[1]) == 0) {
        printf("Deleted: %s\n", args[1]);
    } else {
        printf("Cannot delete: %s\n", args[1]);
    }
}

void cmd_mkdir(char **args) {
    if (args[1] == NULL) {
        printf("Usage: mkdir <dirname>\n");
        return;
    }
    
    if (mkdir(args[1], 0755) == 0) {
        printf("Directory created: %s\n", args[1]);
    } else {
        printf("Cannot create directory: %s\n", args[1]);
    }
}

void cmd_rmdir(char **args) {
    if (args[1] == NULL) {
        printf("Usage: rmdir <dirname>\n");
        return;
    }
    
    if (rmdir(args[1]) == 0) {
        printf("Directory removed: %s\n", args[1]);
    } else {
        printf("Cannot remove directory: %s\n", args[1]);
    }
}

void cmd_rename(char **args) {
    if (args[1] == NULL || args[2] == NULL) {
        printf("Usage: rename <oldname> <newname>\n");
        return;
    }
    
    if (rename(args[1], args[2]) == 0) {
        printf("Renamed: %s -> %s\n", args[1], args[2]);
    } else {
        printf("Cannot rename: %s\n", args[1]);
    }
}

void cmd_echo(char **args) {
    if (args[1] == NULL) {
        printf("\n");
        return;
    }
    
    for (int i = 1; args[i] != NULL; i++) {
        printf("%s", args[i]);
        if (args[i+1] != NULL) printf(" ");
    }
    printf("\n");
}

void cmd_date() {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d", tm_info);
    printf("%s\n", buf);
}

void cmd_time() {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char buf[64];
    strftime(buf, sizeof(buf), "%H:%M:%S", tm_info);
    printf("%s\n", buf);
}

void execute_cmd(char *cmd) {
    char *args[MAX_ARGS];
    char *token = strtok(cmd, " ");
    int i = 0;
    
    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " ");
    }
    args[i] = NULL;
    
    if (i == 0) return;
    
    if (strcmp(args[0], "exit") == 0 || strcmp(args[0], "quit") == 0) {
        printf("Shell Exit...\n");
        exit(0);
    }
    else if (strcmp(args[0], "cd") == 0 || strcmp(args[0], "chdir") == 0) {
        if (args[1] == NULL) {
            chdir(getenv("HOME"));
        } else {
            if (chdir(args[1]) != 0) {
                printf("Directory not found: %s\n", args[1]);
            }
        }
        return;
    }
    else if (strcmp(args[0], "help") == 0 || strcmp(args[0], "?") == 0 || strcmp(args[0], "/?") == 0) {
        printf("\nHello Command! Shell - Available Commands:\n");
        printf("  cd      - Change directory\n");
        printf("  cls     - Clear screen\n");
        printf("  copy    - Copy file\n");
        printf("  date    - Show current date\n");
        printf("  del     - Delete file\n");
        printf("  dir     - List directory contents (/w for wide)\n");
        printf("  echo    - Display message\n");
        printf("  exit    - Exit shell\n");
        printf("  help    - Show this help\n");
        printf("  mkdir   - Create directory\n");
        printf("  quit    - Exit shell\n");
        printf("  rename  - Rename file\n");
        printf("  rmdir   - Remove directory\n");
        printf("  time    - Show current time\n");
        printf("  type    - Display file content\n");
        printf("  ver     - Show version info\n");
        printf("  /?      - Show this help\n");
        printf("\n  Other   - Execute external programs (current dir first)\n");
        printf("  Ctrl+C  - Blocked\n");
        printf("  Ctrl+\\  - Blocked\n\n");
        return;
    }
    else if (strcmp(args[0], "ver") == 0 || strcmp(args[0], "version") == 0) {
        printf("\nHello Command! Shell v1.0\n");
        printf("A Tribute to DOS COMMAND.COM\n");
        return;
    }
    else if (strcmp(args[0], "cls") == 0 || strcmp(args[0], "clear") == 0) {
        printf("\033[2J\033[1;1H");
        fflush(stdout);
        return;
    }
    else if (strcmp(args[0], "dir") == 0 || strcmp(args[0], "ls") == 0) {
        cmd_dir(args);
        return;
    }
    else if (strcmp(args[0], "type") == 0 || strcmp(args[0], "cat") == 0) {
        cmd_type(args);
        return;
    }
    else if (strcmp(args[0], "copy") == 0 || strcmp(args[0], "cp") == 0) {
        cmd_copy(args);
        return;
    }
    else if (strcmp(args[0], "del") == 0 || strcmp(args[0], "erase") == 0) {
        cmd_del(args);
        return;
    }
    else if (strcmp(args[0], "mkdir") == 0 || strcmp(args[0], "md") == 0) {
        cmd_mkdir(args);
        return;
    }
    else if (strcmp(args[0], "rmdir") == 0 || strcmp(args[0], "rd") == 0) {
        cmd_rmdir(args);
        return;
    }
    else if (strcmp(args[0], "rename") == 0 || strcmp(args[0], "ren") == 0) {
        cmd_rename(args);
        return;
    }
    else if (strcmp(args[0], "echo") == 0) {
        cmd_echo(args);
        return;
    }
    else if (strcmp(args[0], "date") == 0) {
        cmd_date();
        return;
    }
    else if (strcmp(args[0], "time") == 0) {
        cmd_time();
        return;
    }
    
    char full_path[512];
    char *cmd_path = NULL;
    
    snprintf(full_path, sizeof(full_path), "./%s", args[0]);
    if (access(full_path, X_OK) == 0) {
        cmd_path = full_path;
    }
    
    pid_t pid = fork();
    if (pid == 0) {
        if (cmd_path != NULL) {
            execv(cmd_path, args);
        } else {
            execvp(args[0], args);
        }
        printf("Command not found: %s\n", args[0]);
        exit(1);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork failed");
    }
}

int main() {
    char cmd[MAX_CMD];
    
    signal(SIGINT, sigint_handler);
    signal(SIGQUIT, sigquit_handler);
    
    printf("Hello Command! Shell v1.0\n");
    printf("Type 'help' for available commands.\n\n");
    
    while (1) {
        print_prompt();
        if (fgets(cmd, sizeof(cmd), stdin) == NULL) {
            printf("\nGoodbye!\n");
            break;
        }
        cmd[strcspn(cmd, "\n")] = 0;
        
        if (strlen(cmd) == 0) continue;
        
        execute_cmd(cmd);
    }
    return 0;
}
