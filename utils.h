// 存储bin_dk和bin_runtime共用的函数、类等
#include "constants.h"
#include <cstdio>
#include <cstring>
#include <climits>
#include <cerrno>
#include <stdexcept>
#include <csignal>
#include <csetjmp>
#ifdef _WIN32
#include <windows.h>
#else  
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace _utils_h{
using namespace std;

// -- 工具函数、类等 --
using uchar=unsigned char;
class filenotfound : public std::exception {
public:
    explicit filenotfound(const char *msg):msg(msg){}
    virtual const char* what() const noexcept override {
        return msg.c_str();
    }
    string msg;
};
inline void assert(bool expr,const char *msg="Assertion failed",bool throw_err=true){
    if(!expr){
        if(throw_err)throw runtime_error(msg);
        else fprintf(stderr,"%s\n",msg);
    }
}
char *convert_size(size_t size){
	char *result = new char[20];

	const char *suffix[]={"B", "KB", "MB", "GB", "TB"};
    size_t index = 0;
	double num = size;
    while(num >= 1024 && index < sizeof(suffix) - 1){
        num /= 1024;
        index ++;
	}
    snprintf(result,20,"%.3f %s",num,suffix[index]);
	return result;
}

// -- 辅助操作内存函数 --
void* find_submem(const void* mem, size_t memsize, const void *submem,size_t submem_size) {
    // 在内存块中查找子内存，类似memmem函数
    for (size_t i = 0; i < memsize - submem_size; i++) {
        const uchar *cur=(uchar *)mem + i;
        if (memcmp(cur, submem, submem_size) == 0) {
            return const_cast<void*>((const void *)cur);
        }
    }
    return nullptr; // 未找到
}
void dumpMemory(void* start, const char *filename, size_t size) {
    FILE* dump_file = fopen(filename, "wb");
    if (!dump_file) {
        throw std::runtime_error(strerror(errno));
    }
    fwrite(start, 1, size, dump_file); // fwrite在遇到非法内存时，不会写入数据
    fclose(dump_file);
}
void showMemory(void *ptr,size_t size){
    uchar *fcode=(uchar *)ptr;
    for(size_t i=0;i<size;i++){
        printf("%02x ",fcode[i]);
    }
    putchar('\n');
}

// -- 依赖于段错误的内存检测函数 --
static jmp_buf jmp_env;
void signal_handler(int signum) {
    signal(signum, signal_handler); // 重新设置信号处理器，避免丢失
    longjmp(jmp_env, signum); // 跳转到jmp_env保存的环境
}
bool checkAccessibility(void *ptr,size_t size,size_t *newsize=nullptr){
    // 测试内存是否可访问，如果newsize不是NULL，则通过newsize返回真正的内存大小
    static size_t i;
    signal(SIGSEGV, signal_handler);
    if(setjmp(jmp_env)==0){
        for(i=0;i<size;i++)
            volatile uchar b=((uchar *)ptr)[i];
        signal(SIGSEGV, SIG_DFL);
        if(newsize)*newsize=size;
        return true;
    } else {
        signal(SIGSEGV, SIG_DFL);
        if(newsize)*newsize=i;
        return false;
    }
}
long __attribute__((optimize("O0"))) getHighBoundary(void *mem) {
    uchar *start = (uchar *)mem;
    uchar *cur = start;
    size_t step = MEMORY_STEP;
    // 间隔一段距离查找
    while (true) {
        if (setjmp(jmp_env) == 0) {
            volatile uchar byte = *cur;
            cur += step;
        } else break;
    }
    // 二分查找
    uchar *low = cur - step; // 上一个可访问地址
    while (low < cur) {
        uchar *mid = low + (cur - low) / 2;
        if (setjmp(jmp_env) == 0) {
            volatile uchar byte = *mid;
            low = mid + 1; // 可访问，低边界上移
        } else {
            cur = mid; // 触发 SIGSEGV，高边界下移
        }
    }
    return cur - start;
}
long __attribute__((optimize("O0"))) getLowBoundary(void *mem) {
    uchar *start = (uchar *)mem;
    uchar *cur = start;
    size_t step = MEMORY_STEP;
    // 间隔一段距离查找
    while (true) {
        if (setjmp(jmp_env) == 0) {
            volatile uchar byte = *cur;
            cur -= step;
        } else break;
    }
    // 二分查找
    uchar *high = cur + step; // 上一个可访问地址
    while (cur < high) {
        uchar *mid = cur + (high - cur) / 2;
        if (setjmp(jmp_env) == 0) {
            volatile uchar byte = *mid;
            high = mid; // 高边界下移
        } else {
            cur = mid + 1; // 低边界上移
        }
    }
    return cur - start;
}
void *getMemBlock(void *mem,size_t *memsize,bool getlow=true,bool gethigh=true){
    signal(SIGSEGV, signal_handler);
    long low=0,high=0;
    if(getlow)low=getLowBoundary(mem);
    if(gethigh)high=getHighBoundary(mem);
    signal(SIGSEGV, SIG_DFL);
    printf("low: %ld high:%ld\n",low,high);
    *memsize=high-low;
    return (void *)((uchar *)mem+low);
}

// -- 申请可执行的内存 (依赖特定平台) --
#ifdef _WIN32
void *allocExecMemory(size_t size){
    LPVOID pMemory = VirtualAlloc(NULL, size, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (pMemory == NULL) throw runtime_error("Cannot allocate memory for execution");
    return pMemory;
}
void freeExecMemory(void *pMemory, size_t size=0) {
    if (!VirtualFree(pMemory, 0, MEM_RELEASE))
        throw runtime_error("Cannot free virtual memory");
}
#else
void *allocExecMemory(size_t size){
    void *pMemory = mmap(nullptr, size,
        PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pMemory == MAP_FAILED)
        throw runtime_error("Cannot allocate memory for execution");
    return pMemory;
}
void freeExecMemory(void *pMemory, size_t size) {
    if (munmap(pMemory, size) != 0)
        throw runtime_error("Cannot free virtual memory");
}
#endif
}

using _utils_h::filenotfound;
using _utils_h::assert;
using _utils_h::convert_size;
using _utils_h::find_submem;
using _utils_h::dumpMemory;
using _utils_h::showMemory;
using _utils_h::signal_handler;
using _utils_h::jmp_env;
using _utils_h::checkAccessibility;
using _utils_h::getHighBoundary;
using _utils_h::getLowBoundary;
using _utils_h::getMemBlock;
using _utils_h::allocExecMemory;
using _utils_h::freeExecMemory;