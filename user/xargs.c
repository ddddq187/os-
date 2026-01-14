#include "kernel/types.h"
#include "user/user.h"
#include "kernel/param.h"
#include <stddef.h>

// -------------- FSM 核心枚举定义 --------------
// 1. FSM 状态：覆盖xargs解析参数的全生命周期
enum fsm_state {
    STATE_INIT,        
// 初始状态：等待输入（空格/空行/首个参数）
    STATE_READ_ARG,    
// 读取参数内容（非空格/非换行字符）
    STATE_ARG_END,     
// 参数结束（遇到空格，等待下一个参数）
    STATE_EXEC,        
// 触发执行（遇到换行/EOF，执行拼接后的命令）
    STATE_FINISH       
// 结束状态（输入耗尽）
};

// 2. 输入类型：抽象输入字符，简化状态判断
enum fsm_input {
    INPUT_SPACE,       
// 空格（参数分隔符）
    INPUT_CHAR,        
// 普通字符（参数内容）
    INPUT_NEWLINE,     
// 换行（命令执行触发符）
    INPUT_EOF          
// 输入结束（EOF）
};

// -------------- 常量定义 --------------
#define MAX_BUF 512    
// 输入缓冲区大小（适配xv6内存限制）

// -------------- FSM 辅助函数 --------------
/**
 * @brief 输入解码：原始字符 → 抽象输入类型
 * @param c 读取的字符（仅当read_ret=1时有效）
 * @param read_ret read系统调用的返回值（1=成功读取，0=EOF）
 * @return 抽象输入类型
 */
enum fsm_input get_input(char c, int read_ret) {
    if (read_ret != 1) {
        return INPUT_EOF;  
// 读不到字符=EOF
    }
    switch (c) {
        case ' ':  return INPUT_SPACE;
        case '\n': return INPUT_NEWLINE;
        default:   return INPUT_CHAR;
    }
}

/**
 * @brief 状态转换：纯逻辑，仅负责状态切换（无业务代码）
 * @param cur 当前状态
 * @param in 输入类型
 * @return 下一个状态
 */
enum fsm_state transform_state(enum fsm_state cur, enum fsm_input in) {
    switch (cur) {
        case STATE_INIT:
            if (in == INPUT_CHAR)      return STATE_READ_ARG;
            if (in == INPUT_SPACE)     return STATE_INIT;
            if (in == INPUT_NEWLINE)   return STATE_INIT;
            if (in == INPUT_EOF)       return STATE_FINISH;
            break;

        case STATE_READ_ARG:
            if (in == INPUT_CHAR)      return STATE_READ_ARG;
            if (in == INPUT_SPACE)     return STATE_ARG_END;
            if (in == INPUT_NEWLINE)   return STATE_EXEC;
            if (in == INPUT_EOF)       return STATE_EXEC;  
// EOF触发最后一次执行
            break;

        case STATE_ARG_END:
            if (in == INPUT_CHAR)      return STATE_READ_ARG;
            if (in == INPUT_SPACE)     return STATE_ARG_END;
            if (in == INPUT_NEWLINE)   return STATE_EXEC;
            if (in == INPUT_EOF)       return STATE_EXEC;
            break;

        case STATE_EXEC:
            if (in == INPUT_EOF)       return STATE_FINISH;
            else                       return STATE_INIT;  
// 执行后回到初始状态
            break;

        default:
            break;
    }
    return STATE_FINISH;  
// 异常兜底
}

/**
 * @brief 执行命令：封装fork/exec/wait，xv6风格
 * @param argv 拼接后的参数数组（以NULL结尾）
 * @return 执行状态（0=成功，-1=失败）
 */
int fork_exec(char *argv[]) {
    if (argv[0] == NULL) {
        return -1;  
// 无参数，不执行
    }

    int pid = fork();
    if (pid < 0) {
        fprintf(2, "xargs: fork failed\n");
        return -1;
    }

    if (pid == 0) {  
// 子进程：执行命令
        exec(argv[0], argv);
        fprintf(2, "xargs: exec %s failed\n", argv[0]);
        exit(1);
    } else {  
// 父进程：等待子进程结束
        wait(0);
    }
    return 0;
}

/**
 * @brief 重置参数数组：清空指定位置后的参数
 * @param argv 参数数组
 * @param start 起始清空下标
 */
void reset_argv(char *argv[], int start) {
    for (int i = start; i < MAXARG; i++) {
        argv[i] = NULL;
    }
}

// -------------- 主函数（FSM驱动入口） --------------
int main(int argc, char *argv[]) {
    // 1. 入参校验：必须指定要执行的命令（如 xargs echo）
    if (argc < 2) {
        fprintf(2, "usage: xargs <command> [args...]\n");
        exit(1);
    }

    // 2. 初始化FSM和运行时变量
    enum fsm_state cur_state = STATE_INIT;  
// FSM初始状态
    char buf[MAX_BUF] = {0};                
// 输入缓冲区
    int buf_idx = 0;                        
// 缓冲区当前写入位置
    char *x_argv[MAXARG] = {0};             
// 最终传递给exec的参数数组
    int arg_cnt = argc - 1;                 
// 初始参数计数（用户传入的命令+固定参数）

    // 3. 填充初始参数（如 xargs echo hello → x_argv[0]=echo, x_argv[1]=hello）
    for (int i = 1; i < argc; i++) {
        x_argv[i-1] = argv[i];
    }

    // 4. FSM主循环：驱动状态流转 + 执行业务逻辑
    while (cur_state != STATE_FINISH) {
        // 4.1 读取输入字符（逐字符读取，xv6标准方式）
        char c;
        int read_ret = read(0, &c, 1);  
// 从标准输入(0)读1个字符

        // 4.2 输入解码：原始字符 → 抽象输入类型
        enum fsm_input in = get_input(c, read_ret);

        // 4.3 状态转换：纯逻辑切换状态
        cur_state = transform_state(cur_state, in);

        // 4.4 按当前状态执行业务逻辑
        switch (cur_state) {
            case STATE_READ_ARG:
                // 业务：将字符写入缓冲区（检查缓冲区溢出）
                if (buf_idx >= MAX_BUF - 1) {  
// 留1位给'\0'
                    fprintf(2, "xargs: input buffer overflow\n");
                    exit(1);
                }
                buf[buf_idx++] = c;
                break;

            case STATE_ARG_END:
                // 业务：分割参数（缓冲区加'\0'，存入x_argv）
                if (buf_idx == 0) break;  
// 空参数跳过
                buf[buf_idx] = '\0';      
// 字符串结束符
                if (arg_cnt >= MAXARG) {  
// 检查参数数量超限
                    fprintf(2, "xargs: too many arguments (max %d)\n", MAXARG);
                    exit(1);
                }
                x_argv[arg_cnt++] = buf;  
// 缓冲区地址存入参数数组
                buf_idx = 0;              
// 重置缓冲区指针
                break;

            case STATE_EXEC:
                // 业务：执行命令（处理最后一个参数 + 执行 + 重置）
                if (buf_idx > 0) {  
// 缓冲区有未处理的参数
                    buf[buf_idx] = '\0';
                    if (arg_cnt >= MAXARG) {
                        fprintf(2, "xargs: too many arguments (max %d)\n", MAXARG);
                        exit(1);
                    }
                    x_argv[arg_cnt++] = buf;
                    buf_idx = 0;
                }
                fork_exec(x_argv);        
// 执行命令
                arg_cnt = argc - 1;       
// 重置参数计数（保留初始命令参数）
                reset_argv(x_argv, arg_cnt);  
// 清空新增的参数
                break;

            default:  
// STATE_INIT/STATE_FINISH 无业务逻辑
                break;
        }
    }

    exit(0);
}