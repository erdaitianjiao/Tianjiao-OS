#include "stdint.h"
#include "buildin_cmd.h"
#include "string.h"
#include "debug.h"
#include "fs.h"
#include "file.h"
#include "syscall.h"
#include "stdio.h"
#include "shell.h"
#include "dir.h"
#include "syscall.h"

// 将路径old_abd_path中的..和.转换为实际路径后new_abs_path
static void wash_path(char* old_abs_path, char* new_abs_path) {

    ASSERT(old_abs_path[0] == '/');
    char name[MAX_FILE_NAME_LEN] = {0};
    char* sub_path = old_abs_path;
    sub_path = path_parse(sub_path, name);

    if (name[0] == 0) {

        new_abs_path[0] = '/';
        new_abs_path[1] = 0;
        return;

    }
    // 避免传给new_pas_path的值不干净
    new_abs_path[0] = 0;
    strcat(new_abs_path, "/");
    while (name[0]) {

        // 如果是上一级目录
        if (!strcmp("..", name)) {

            char* slash_ptr = strrchr(new_abs_path, '/');
            // 如果未到new_abs_path中的顶层目录 就将最右边的'/'替换成0
            if (slash_ptr != new_abs_path) {

                *slash_ptr = 0;

            } else {

                *(slash_ptr + 1) = 0;

            }

        } else if (strcmp(".", name)) {

            // 如果路径不是. 就将name 拼接到new_abs_path后面
            if (strcmp(new_abs_path, "/")) {

                strcat(new_abs_path, "/");

            }
            strcat(new_abs_path, name);

        }
        // 继续遍历下一层目录
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (sub_path) {

            sub_path = path_parse(sub_path, name);

        }
    
    }

}

// 将path处理成不含.和..的绝对目录 存储在final_path中
void make_clear_abs_path(char* path, char* final_path) {

    char abs_path[MAX_PATH_LEN] = {0};
    // 先判断是否输入的是绝对路径
    if (path[0] != '/') {

        memset(abs_path, 0, MAX_PATH_LEN);
        if (getcwd(abs_path, MAX_PATH_LEN) != NULL) {

            if (!((abs_path[0] == '/') && (abs_path[1] == 0))) {

                strcat(abs_path, "/");

            }

        }

    }
    strcat(abs_path, path);
    wash_path(abs_path, final_path);

}

// pwd命令的内部构建
void buildin_pwd(uint32_t argc, char** argv UNUSED) {

    if (argc != 1) {

        printf("pwd: no argument support!\n");
        return;

    } else {

        if (getcwd(final_path, MAX_PATH_LEN) != NULL) {

            printf("%s\n", final_path);

        } else {

	        printf("pwd: get current work directory failed.\n");

        } 
        

    }

}

// cd命令内部构建
char* buildin_cd(uint32_t argc, char** argv) {

    if (argc > 2) {

        printf("cd: only support 1 argument!\n");
        return NULL;

    }
    
    // 若是只键入cd而无参数 直接返回到根目录
    if (argc == 1) {

        final_path[0] = '/';
        final_path[1] = 0;

    } else {

        make_clear_abs_path(argv[1], final_path);

    }

    if (chdir(final_path) == -1) {
        printf("cd: no such directory %s\n", final_path);
        return NULL;

    }

    return final_path;

}

// ls命令内部构建函数
void buildin_ls(uint32_t argc, char** argv) {


    char* pathname = NULL;
    struct stat file_stat;
    memset(&file_stat, 0, sizeof(struct stat));
    bool long_info = false;
    uint32_t arg_path_nr = 0;
    uint32_t arg_idx = 1;
    while (arg_idx < argc) {
        
        if (argv[arg_idx][0] == '-') {
        
            if (!strcmp(argv[arg_idx], "-l")) {

                long_info = true;

            } else if (!strcmp(argv[arg_idx], "-h")) {

                printf("usage: -l list all infomation about the file\n-h for help\nlist all files in thr current dirctory if no option\n");
                return;

            } else {

                printf("ls: invalid option %s\nTry 'ls -h' for more information\n", argv[arg_idx]);
                return;

            }

        } else {

            // ls 的路径参数
            if (arg_path_nr == 0) {

                pathname = argv[arg_idx];
                arg_path_nr = 1;

            } else {

                printf("ls: only support ont path");
                return;

            }

        }
        arg_idx ++;

    }

    if (pathname == NULL) {

        // 默认当前目录
        if (getcwd(final_path, MAX_PATH_LEN) != NULL) {

            pathname = final_path;

        } else {

            printf("ls: getcwd for default path failed\n");
            return;

        }

    } else {

        make_clear_abs_path(pathname, final_path);
        pathname = final_path;

    }
    if (stat(pathname, &file_stat) == -1) {

        printf("ls: cannot access %s: No sucn file or directory\n", pathname);
        return;

    }
    if (file_stat.st_filetype == FT_DIRECTORY) {

        struct dir* dir = opendir(pathname);
        struct dir_entry* dir_e = NULL;
        char sub_pathname[MAX_PATH_LEN] = {0};
        uint32_t pathname_len = strlen(pathname);
        uint32_t last_char_idx = pathname_len - 1;
        memcpy(sub_pathname, pathname, pathname_len);
        if (sub_pathname[last_char_idx] != '/') {

            sub_pathname[pathname_len] = '/';
            pathname_len ++;

        }
        rewinddir(dir);
        if (long_info) {

            char ftype;
            printf("total: %d\n", file_stat.st_size);
            while ((dir_e = readdir(dir))) {

                ftype = 'd';
                if (dir_e->f_type == FT_REGULAR) {

                    ftype = '-';

                }
                sub_pathname[pathname_len] = 0;
                strcat(sub_pathname, dir_e->filename);
                memset(&file_stat, 0, sizeof(struct stat));
                if (stat(sub_pathname, &file_stat) == -1) {

                    printf("ls: cannot access %s: No such file or directory\n", dir_e->filename);
                    return;

                }
                printf("%c  %d  %d  %s\n", ftype, dir_e->i_no, file_stat.st_size, dir_e->filename);

            }

        } else {

            while ((dir_e = readdir(dir))) {

                printf("%s ", dir_e->filename);

            }
            printf("\n");

        }
        closedir(dir);

    } else {

        if (long_info) {

            printf("-  %d  %d  %s\n", file_stat.st_ino, file_stat.st_size, pathname);

        } else {

            printf("%s\n", pathname);

        }

    }

}

// ps命令内建函数
void buildin_ps(uint32_t argc, char** argv UNUSED) {

    if (argc != 1) {

        printf("ps: no argument support!\n");
        return;

    }
    ps();

}

// clear 命令内建函数
void buildin_clear(uint32_t argc, char** argv UNUSED) {

    int32_t ret = -1;
    if (argc != 1) {

        printf("mkdir: no support  argument!\n");
        return;

    } 
    clear();

}

// mkdir命令内建函数
void buildin_mkdir(uint32_t argc, char** argv) {

    int32_t ret = -1;
    if (argc != 2) {

        printf("mkdir: only support 1 argument!\n");

    } else {

        make_clear_abs_path(argv[1], final_path);
        if (strcmp("/", final_path)) {

            if (mkdir(final_path) == 0) {

                ret = 0;

            } else {

                printf("mkdir: create directory %s failed\n", argv[1]);

            }

        }

    }
    return ret;

}

// rmdir命令内建函数
int32_t buildin_rmdir(uint32_t argc, char** argv) {

    int32_t ret = -1;
    if (argc != 2) {

        printf("rmdir: only support 1 argument!\n");

    } else {

        make_clear_abs_path(argv[1], final_path);
        if (strcmp(final_path, "/")) {

            if (rmdir(final_path) == 0) {

                ret = 0;

            } else {

                printf("rmdir: remove %s failed\n", argv[1]);

            }

        }

    }
    return ret;

}

// rm命令内建函数
int32_t buildin_rm(uint32_t argc, char** argv) {

    int32_t ret = -1;
    if (argc != 2) {

        printf("rm: only support 1 argument!\n");

    } else {

        make_clear_abs_path(argv[1], final_path);
        if (strcmp(final_path, "/")) {

            if (unlink(final_path) == 0) {

                ret = 0;

            } else {

                printf("rm: delete %s failed\n", argv[1]);

            }

        }

    }

}

char tianasc[] = 
"#   _   _              _ _ \n"             
"#  | |_(_) __ _ _ __  (_|_) __ _  ___      \n"  
"#  | __| |/ _` | '_ \ | | |/ _` |/ _ \     \n" 
"#  | |_| | (_| | | | || | | (_| | (_) |    \n"
"#   \__|_|\__,_|_| |_|/ |_|\__,_|\___/     \n" 
"#                   |__/                   \n";

char tianasc2[] =
"           d8,                       d8,   d8,                     \n"                  
"    d8P   `8P                       `8P   `8P\"                    \n"                  
" d888888P                                                          \n"
"   ?88'    88b d888b8b    88bd88b   d88    88b d888b8b   d8888b    \n"
"   88P     88Pd8P' ?88    88P' ?8b  ?88    88Pd8P' ?88  d8P' ?88   \n"
"   88b    d88 88b  ,88b  d88   88P   88b  d88 88b  ,88b 88b  d88   \n"
"   `?8b  d88' `?88P'`88bd88'   88b   `88bd88' `?88P'`88b`?8888P'   \n"
"                                      )88                          \n"
"                                     ,88P                          \n"
"                                  `?888P                           \n";

char tianasc3[] =                     
"         88                           88  88 \n"
"  ,d     \"\"                           \"\"  \"\" \n"
"  88 \n"
"MM88MMM  88  ,adPPYYba,  8b,dPPYba,   88  88  ,adPPYYba,   ,adPPYba,\n"
"  88     88  \"\"     `Y8  88P'   `\"8a  88  88  \"\"     `Y8  a8\"     \"8a\n"
"  88     88  ,adPPPPP88  88       88  88  88  ,adPPPPP88  8b       d8\n"
"  88,    88  88,    ,88  88       88  88  88  88,    ,88  \"8a,   ,a8\"\n"
"  \"Y888  88  `\"8bbdP\"Y8  88       88  88  88  `\"8bbdP\"Y8   `\"YbbdP\"'\n"
"                                     ,88  \n"
"                                   888P\" \n";

void buildin_tianjiao(uint32_t argc, char** argv) {

    // printf("%s", tianasc3);
    printf("%s\n", tianasc2);
    printf("Name: tianjiaoOS      Uptime: 2025.9.24       Version: 0.1\n");
    printf("Host: erdaitianjiao  ");
    return;

}