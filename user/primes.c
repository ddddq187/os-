// user/primes.c
#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#define RD 0
#define WR 1
//接收左管道（左邻居）
void select_prime(int pleft[2]){
    //从left读int
    int p;
    read(pleft[RD],&p,sizeof(int));
    if(p==-1){
        exit(0);
    }
    printf("prime %d\n",p);//第一个数必然质数

    //新建管道
    int pright[2];
    pipe(pright);
    if(fork()==0){//右邻居
        close(pright[WR]);
        close(pleft[RD]);
        select_prime(pright);
        
    }else{
        close(pright[RD]);
        int buf;
        while((read(pleft[RD],&buf,sizeof(int)))&& buf!=-1){
            if(buf%p!=0){
                write(pright[WR],&buf,sizeof(int));
            }
        }

        buf=-1;
        write(pright[WR],&buf,sizeof(int));
        wait(0);
        exit(0);
    }
}
int main(int argc, char **argv){
    int first_pipe[2];
    pipe(first_pipe);
    if(fork()==0){
        close(first_pipe[WR]);
        select_prime(first_pipe);
        exit(0);
    }else{
        close(first_pipe[RD]);
        int i;
        for(i=2;i<=35;i++){			
            write(first_pipe[1], &i, sizeof(i));	// 向管道写入2~35的整数		
        }
        i=-1;
        write(first_pipe[1], &i, sizeof(i)); 
    }
    wait(0);
    exit(0);
}