#include "runtime_env.h"
#include "utils.h"
#include "constants.h"
#include <cstdio>
#include <cstring>
#include <climits>
#include <cerrno>
#include <stdexcept>
#include <algorithm>
using namespace std;

using uchar=unsigned char;
using ushort=unsigned short;

// 机器码处理
const uchar RET=0xc3;
const uchar NOP=0x90;

size_t getFuncCodeSize(void *funcptr,size_t maxsize=SIZE_MAX>>1){
    uchar *delta=(uchar *)memchr(funcptr,RET,maxsize);
    if(delta==nullptr) return SIZE_MAX;
    return (delta-(uchar *)funcptr)+1;
}
/* 判断RET之后的特征指令，避免仅依赖RET指令的导出函数不完整 (备用)
const uchar FEATURE_CODE1=0x66;
const ushort FEATURE_CODE2=0x1f0f; // 0f 1f
size_t getFuncCodeSize(void *funcptr,size_t maxsize=SIZE_MAX>>1){
    uchar *fcode=(uchar *)funcptr;
    for(size_t i=0;i<maxsize;i++){
        uchar byte=fcode[i];
        if(byte==RET){ // 判断特征码
            if(i<maxsize-1 && fcode[i+1]==FEATURE_CODE1) // NOP)
                return i+1;
            else if(i<maxsize-2 && *((ushort *)&fcode[i+1])==FEATURE_CODE2)
                return i+1;
        }
    }
    return maxsize;
}*/
void dumpFunctoFile(void *funcptr,const char *filename,
                    size_t maxsize=SIZE_MAX>>1,size_t minsize=0){
    // minsize:getFuncCodeSize返回值过小时的最小导出大小，避免导出不完整
    size_t size=max(minsize,getFuncCodeSize(funcptr,maxsize));
    dumpMemory(funcptr,filename,size);
}

#define DUMP_BIN(func){\
    dumpFunctoFile((void *)(func),#func".bin");\
    printf("Successfully generated "#func".bin.\n");\
}
#define DUMP_BIN_SIZE(func,minsize,maxsize){\
    dumpFunctoFile((void *)(func),#func".bin",(maxsize),(minsize));\
    printf("Successfully generated "#func".bin.\n");\
}
#define DUMP_BIN_MINSIZE(func,minsize) DUMP_BIN_SIZE(func,(minsize),SIZE_MAX>>1)
