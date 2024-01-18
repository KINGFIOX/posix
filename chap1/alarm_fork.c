#include "errors.h"
#include <sched.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wait.h>

int main(int argc, char* argv[])
{
    int status;
    char line[128];
    int seconds;
    pid_t pid;
    char message[64];

    while (1) {
        printf("Alarm> ");
        if (fgets(line, sizeof(line), stdin) == NULL) {
            exit(0);
        }
        if (strlen(line) <= 1) {
            continue;
        }
        // %64: 最多能够读取64个字节
        // [^\n]: 直到遇到换行符为止
        if (sscanf(line, "%d %64[^\n]", &seconds, message) < 2) {
            fprintf(stderr, "bad command\n");
        } else {
            pid = fork();
            if (pid == (pid_t)-1) {
                errno_abort("fork");
            }
            if (pid == (pid_t)0) {
                /* 子进程 */
                sleep(seconds);
                printf("(%d) %s\n", seconds, message);
            } else {
                /* 父进程 */
                do {
                    // pid=-1表示等待任意子进程，如果pid>0，那么就是等待pid的进程
                    // stat_loc用来存储进程状态的信息，如果为NULL，那么就是对进程状态不感兴趣
                    // W NO HANG，不阻塞，直接返回，如果进程没有返回，按么waitpid=0，
                    // 返回值，>0 表示pid
                    pid = waitpid((pid_t)-1, NULL, WNOHANG);
                    if (pid == (pid_t)-1) {
                        errno_abort("wait for child");
                    }
                } while (pid != (pid_t)0);
            }
        }
    }
}