#include "shell.h"
#include "assert.h"
#include "buildin_cmd.h"
#include "dir.h"
#include "exec.h"
#include "file.h"
#include "fs.h"
#include "global.h"
#include "stdio.h"
#include "string.h"
#include "../lib/user/syscall.h"
#define cmd_len 128    // 最大支持键入128个字符的命令行输入
#define MAX_ARG_NR 16  // 加上命令名外,最多支持15个参数

/* 存储输入的命令 */
static char cmd_line[cmd_len] = {0};
char final_path[MAX_PATH_LEN] = {0};  // 用于洗路径时的缓冲
/* 用来记录当前目录,是当前目录的缓存,每次执行cd命令时会更新此内容 */
char cwd_cache[64] = {0};
/* 输出提示符 */
void print_prompt(void) { printf("[rabbit@localhost %s]$ ", cwd_cache); }

static void readline(char* buf, int32_t count)
{
    assert(buf != NULL && count > 0);
    char* pos = buf;
    while (read(stdin_no,pos,1)!=-1&&(pos-buf)<count)//逐字读取字符
    {
        switch (*pos){
            //\n和\r是命令结尾
            case '\n':
            case '\r':
                *pos = 0;
                put_char("\n");
                return;
            case '\b'://返回一个字符
                if (cmd_line[0] != '\b') {  // 阻止删除非本次输入的信息
                    --pos;  // 退回到缓冲区cmd_line中上一个字符
                    putchar('\b');
                }
                break;
            case 'l'-'a'://清屏
                *pos = 0;
                clear();
                print_prompt();
                printf("%s", buf);
                break;
            case 'u' - 'a'://撤回
                while (buf!=pos)
                {
                    putchar('\b');
	                *(pos--) = 0;
                }
                break;
            default://如果是普通字符就回显
                putchar(*pos);
                pos++;
        }
    }
    printf("readline: can`t find enter_key in the cmd_line, max num of char is 128\n");
}
static int32_t cmd_parse(char* cmd_str, char** argv, char token)
{
     assert(cmd_str != NULL);
    int32_t arg_idx = 0;
    while(arg_idx < MAX_ARG_NR) {//初始化命令
        argv[arg_idx] = NULL;
        arg_idx++;
    }
    char* next = cmd_str;
    int32_t argc = 0;
    while (*next)
    {
        while (*next == token)//跳过空格字符
        {
            next++;
        }
        if (*next == 0)//如果结尾是空格，说明到了结尾
        {
            break;
        }
        argv[argc] = next;//将空格后字符的第一个字符的地址
        while (*next && *next == token) {//跳过正常字符
            next++;
        }
        if (*next)//如果没到结尾就在最后设置为0,将命令割开
        {
            *next++=0;
        }
        if (argc > MAX_ARG_NR) {//命令参数超过了最大的限制
	        return -1;
        }
        argc++;
    }
    return argc;
}
char* argv[MAX_ARG_NR];    // argv为全局变量，为了以后exec的程序可访问参数
int32_t argc = -1;
static void cmd_exectue(uint32_t argc, char** argv)
{
    if (!strcmp("ls", argv[0])) {
    buildin_ls(argc, argv);
    } else if (!strcmp("cd", argv[0])) {
        if (buildin_cd(argc, argv) != NULL) {
         memset(cwd_cache, 0, MAX_PATH_LEN);
         strcpy(cwd_cache, final_path);
        }
    } else if (!strcmp("cd", argv[0])) {
        buildin_cd(argc, argv);
    } else if (!strcmp("ps", argv[0])) {
        buildin_ps(argc, argv);
    } else if (!strcmp("clear", argv[0])) {
        buildin_clear(argc, argv);
    } else if (!strcmp("mkdir", argv[0])) {
        buildin_mkdir(argc, argv);
    } else if (!strcmp("rmdir", argv[0])) {
        buildin_rmdir(argc, argv);
    } else if (!strcmp("rm", argv[0])) {
        buildin_rm(argc, argv);
    } else if (!strcmp("touch", argv[0])) {
        buildin_touch(argc, argv);
    } else if (!strcmp("pwd", argv[0])) {
        buildin_pwd(argc, argv);
    } else if (!strcmp("help", argv[0])) {
        buildin_help(argc, argv);
    } else{// 如果是外部命令,需要从磁盘上加载
        int32_t pid = fork();
        if (pid) {  // 父进程
            int32_t status;
            int32_t child_pid = wait(&status);
            if (child_pid == -1) {
                panic("my_shell: no child\n");
            }
            printf("\n");
            printf("child_pid %d, it's status: %d\n", child_pid, status);
        }else
        {
            make_clear_abs_path(argv[0], final_path);
            struct stat file_stat;
            argv[0] = final_path;
            if (stat(argv[0], &file_stat))
            {
                printf("my_shell: cannot access %s,No such file or directory\n",
               argv[0]);
            }else
            {
                execv(argv[0], argv);
            }
        }
        int32_t arg_idx = 0;
        while (arg_idx < MAX_ARG_NR) {
            argv[arg_idx] = NULL;
            arg_idx++;
        }
    }
}
void my_shell(void)
{
    cwd_cache[0] = '/';
    while (1)
    {
        print_prompt();
        memset(final_path, 0, MAX_PATH_LEN);
        memset(cmd_line, 0, cmd_len);
        readline(cmd_line, cmd_len);//存入命令
        if (cmd_line[0]==0)
        {
            continue;//只输入了一个回车
        }
        char* pipe_symbol = strchr(cmd_line, '|');
        if (pipe_symbol)//有管道
        {
            int32_t fd[2] = {-1, -1};
            pipe(fd);
            fd_redirect(1, fd[1]);//重定向输出到
            char* each_cmd = cmd_line;
            pipe_symbol = strchr(cmd_line, '|');
            *pipe_symbol = 0;
            argc = -1;
            argc = cmd_parse(each_cmd, argv, ' ');
            cmd_exectue(argc, argv);
            each_cmd = pipe_symbol+ 1;//跨过|
            fd_redirect(0, fd[0]);
            while ((pipe_symbol = strchr(each_cmd, '|'))) {
                *pipe_symbol = 0;
                argc = -1;
                argc = cmd_parse(each_cmd, argv, ' ');
                cmd_exectue(argc, argv);
                each_cmd = pipe_symbol + 1;
            }
            fd_redirect(1, 1);
            argc = -1;
            argc = cmd_parse(each_cmd, argv, ' ');
            cmd_exectue(argc, argv);
            fd_redirect(0, 0);
            close(fd[0]);
            close(fd[1]);
        } else {
            argc = -1;
            argc = cmd_parse(cmd_line, argv, ' ');
            if (argc == -1)
            {
                printf("num of arguments exceed %d\n", MAX_ARG_NR);
                continue;
            }
            cmd_exectue(argc, argv);
        }
    }
    
}