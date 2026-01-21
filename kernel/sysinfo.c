#include "sysinfo.h"

uint64
sys_sysinfo(void){
    struct sysinfo info;
    dq_freebytes(&info.freemem);
    dq_procnum(&info.nproc);
    uint64 dstaddr;
    argaddr(0,&dstaddr);
    if(copyout(myproc()->pagetable,dstaddr,(char*)&info,sizeofinfo)){
        return -1;
    }
    return 0;
}
