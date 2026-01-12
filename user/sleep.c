#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc,char *argv[]){
    int time;
    if(argc!=2){
        fprintf(2,"Wrong command,please input one and only one parameter\n");
    }
    time = atoi(argv[1]);
    int remain=sleep(time);
    if(remain==-1){
        fprintf(2,"system error\n");
    }
    while(remain>0){
        remain =sleep(remain);
    }
    
    exit(0);
}