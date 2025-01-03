#include "runtime_env.h"
#include "utils.h"
#include "constants.h"
#include "libraryloader.h"
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <climits>
#include <stdexcept>
#include <csignal>
#include <csetjmp>
#include <string>
#include <unordered_map>
#include <utility>
#ifdef _WIN32
#include <dbghelp.h>
#include <Psapi.h>
#else
#include <execinfo.h>
#endif
using namespace std;

using uchar=unsigned char;
using ushort=unsigned short;
const size_t MAX_STACKTRACE_SIZE=256;
const size_t MAX_STACKTRACE_NAMELEN_WIN=256;
#ifdef _WIN32
const uchar pathsep='\\';
const int current_platform=WIN32_;
#else
const uchar pathsep='/';
const int current_platform=POSIX;
#endif

unordered_map<string, LibraryLoader*> loaded_libs;
LibraryLoader *loadLibrary(const char *libname) {
    auto it = loaded_libs.find(libname);
    if (it != loaded_libs.end()) {
        return it->second; // 已找到，直接返回
    } else {
        // 未找到，尝试创建新的 LibraryLoader
        try {
            LibraryLoader *newLib = new LibraryLoader(libname);
            loaded_libs[libname] = newLib;
            return newLib;
        } catch (runtime_error) {
            return nullptr; // nullptr 表示加载失败
        }
    }
}
void *getLibraryFunc(const char *libname, const char *funcname) {
    LibraryLoader *lib = loadLibrary(libname);
    if(lib == nullptr) return nullptr;
    return lib->getSymbol(funcname);
}
void freeLibrary(const char *libname) {
    auto it = loaded_libs.find(libname);
    if (it != loaded_libs.end()) {
        LibraryLoader *lib = it->second;
        delete lib;
        loaded_libs.erase(it);
    } //else {
        //throw runtime_error("Attempt to free an unloaded library");
    //}
}

void *loadExecutable(const char *filename,size_t *memsize=nullptr){
    FILE *file = fopen(filename, "rb");
    if (file == nullptr)
        throw filenotfound(strerror(errno));
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);rewind(file);
    uchar *buffer = new uchar[size];

    size_t bytesRead = fread(buffer, 1, size, file);
    if (bytesRead != size) {
        delete[] buffer;fclose(file);
        throw runtime_error("Error reading file");
    }
    void *func=allocExecMemory(size);
    memcpy(func,buffer,size);
    if(memsize!=nullptr)*memsize=size;
    delete[] buffer;
    return func;
}

void *getFunc(const char *funcname){
    string name(funcname);
    if(imported_funcs.find(name)==imported_funcs.end())
        return nullptr;
    return imported_funcs[name].first;
}
int import(const char *modname,bool reload=false,void **return_ptr=nullptr){
    string path(modname);
    size_t name_start,sep_pos=path.find_last_of(pathsep);
    size_t ext_pos=path.find_last_of('.');
    if(ext_pos==string::npos){
        ext_pos=path.size();
        path+=FILEEXT;
    }
    if(sep_pos==string::npos)name_start=0;
    else name_start=sep_pos+1;
    string func_name=path.substr(sep_pos+1,ext_pos-(sep_pos+1));

    if(imported_funcs.find(func_name)!=imported_funcs.end() && !reload){
        return IMPORT_SUCCESS; // 模块已存在，并且不重新加载
    }
    try{
        size_t size;
        void *funcptr=loadExecutable(path.c_str(),&size);
        if(return_ptr!=nullptr)*return_ptr=funcptr;
        imported_funcs[func_name]=pair<void *,size_t>(funcptr,size);
    }catch(filenotfound){
        return MODULE_NOT_FOUND;
    }catch(runtime_error){
        return UNKNOWN_ERROR;
    }
    return IMPORT_SUCCESS;
}
int forceReload(const char *modname){
    return import(modname,true);
}
int loadModule(const char *modname){
    return import(modname,false);
}
void debugModuleInfo(){
    size_t total_size=0;char *converted;
    printf("Loaded modules:\n");
    for(auto &[func_name,value]:imported_funcs){
        size_t size=value.second;
        converted=convert_size(size);
        printf("%s (%s)\n",func_name.c_str(),converted);
        delete converted;
        total_size+=size;
    }
    converted=convert_size(total_size);
    printf("Total module memory: %s\n\n",converted);
    delete converted;
    printf("Loaded libraries:\n");
    if(loaded_libs.empty()){
        printf("(No libraries loaded)\n\n");
    } else {
        for(auto &[libname,lib]:loaded_libs){
            printf("%s (0x%llx)\n",libname.c_str(),lib->handle);
        }
        printf("\n");
    }
}
pair<string,void *> findModuleByAddress(void *stack_address){
    size_t address=(size_t)stack_address;
    for(const auto &[name,info]:imported_funcs){
        size_t addr=(size_t)info.first;
        size_t size=info.second;
        if(address>=(size_t)addr && address<(size_t)addr+size){
            return pair<string,void *>(name,info.first);
        }
    }
    return make_pair<string,void *>("",nullptr);
}
#ifdef _WIN32
void stackTrace() {  
    void *stack[MAX_STACKTRACE_SIZE];  
    ushort frames;  
    SYMBOL_INFO *symbol;  
    HANDLE process = GetCurrentProcess();  

    // 初始化符号处理  
    SymInitialize(process, NULL, TRUE);

    // 获取调用栈  
    frames = CaptureStackBackTrace(0, MAX_STACKTRACE_SIZE, stack, NULL);  
    symbol = (SYMBOL_INFO *)malloc(sizeof(SYMBOL_INFO) + \
              MAX_STACKTRACE_NAMELEN_WIN * sizeof(char));
    symbol->MaxNameLen = MAX_STACKTRACE_NAMELEN_WIN-1;
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

    fprintf(stderr, "Stacktrace:\n");
    for (ushort i = 0; i < frames; i++) {
        void *address=stack[i]; // 栈的当前地址
        // 从系统获取符号信息
        DWORD64 baseAddr = SymGetModuleBase(process, (DWORD64)address);
        DWORD64 funcOffset = (DWORD64)address - baseAddr; // 函数相对模块基地址的偏移量
        char filename[MAX_STACKTRACE_NAMELEN_WIN];
        IMAGEHLP_MODULE64 moduleInfo;
        moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
        const char *moduleName;
        if (SymGetModuleInfo64(process, (DWORD64)address, &moduleInfo)){
            char *token,*context;
            moduleName=moduleInfo.ImageName;
            token=strtok_s(moduleInfo.ImageName,"/\\",&context);
            while(token!=nullptr){
                moduleName=token;
                token=strtok_s(nullptr,"/\\",&context);
            }
        } else moduleName=nullptr;
        const char *funcName;size_t funcAddress=0; // 函数的绝对地址
        if(SymFromAddr(process, (DWORD64)address, 0, symbol)){
            funcName=symbol->Name;
            funcAddress=symbol->Address;
        } else funcName=nullptr;

        //从加载的bin文件自身获取符号
        if(!moduleName && !funcName){
            auto info=findModuleByAddress(address);
            if(!info.first.empty()){
                funcName=info.first.c_str();
                funcAddress=(size_t)info.second;
            }
        }
        const char *base_msg=(moduleName)?"ModuleBase + ":"";
        if(funcName){
            fprintf(stderr, "%s ! %s (%s0x%llx + 0x%llx)\n", 
                    defaultVal((const char*)moduleName,"<Unknown module>"), funcName,
                    base_msg, funcAddress-baseAddr, address-funcAddress);
        } else {
            fprintf(stderr, "%s ! <Unknown function> (%s0x%llx)\n", 
                    defaultVal((const char*)moduleName,"<Unknown module>"), 
                    base_msg, (DWORD64)address - baseAddr);
        }
    }

    free(symbol);
    SymCleanup(process);
}
#else
void stackTrace() {
    void *array[MAX_STACKTRACE_SIZE];  
    size_t size;  

    // 获取堆栈中的地址  
    size = backtrace(array, MAX_STACKTRACE_SIZE);  

    // 打印堆栈信息  
    fprintf(stderr, "Stacktrace:\n");
    backtrace_symbols_fd(array, size, STDERR_FILENO);
}
#endif
FILE *getstdin(){return stdin;} // stdin为调用__acrt_iob_func的宏
FILE *getstdout(){return stdout;}
FILE *getstderr(){return stderr;}
void abort_(){raise(SIGABRT);} // 不使用标准库的abort

static RuntimeEnv *runtime_env=new RuntimeEnv();
using ExecutableMain=int (*)(int,const char**,RuntimeEnv*);
void initRuntimeEnv(RuntimeEnv *runtime_env){
    runtime_env->version=RuntimeVersion{RUNTIME_VERSION_MAJOR,
        RUNTIME_VERSION_MINOR,RUNTIME_VERSION_REVISION};
    runtime_env->platform=current_platform;
    runtime_env->import=loadModule;
    runtime_env->getFunc=getFunc;
    runtime_env->getLibraryFunc=getLibraryFunc;
    runtime_env->freeLibrary=freeLibrary;
    runtime_env->debugModuleInfo=debugModuleInfo;
    runtime_env->getstdin=getstdin;
    runtime_env->getstdout=getstdout;
    runtime_env->getstderr=getstderr;
    runtime_env->stackTrace=stackTrace;
    runtime_env->abort=abort_;
}
int execExecutable(const char *filename,int argc,const char *argv[]){
    // argv的第0项是程序目录，从第1项开始是命令行参数
    size_t size;ExecutableMain mainfunc;
    int import_result=import(filename,&size,(void**)(&mainfunc));
    if(import_result!=0)
        throw runtime_error(
            "Import main module failed with code "+to_string(import_result));
    signal(SIGABRT, signal_handler);
    signal(SIGSEGV, signal_handler);int signum;
    if((signum=setjmp(jmp_env))==0){
        int result=mainfunc(argc,argv,runtime_env);
        signal(SIGABRT, SIG_DFL);signal(SIGSEGV, SIG_DFL);
        freeExecMemory((void *)mainfunc,size);
        return result;
    }else{
        switch(signum){
            case SIGABRT:
                printf("%s called abort(), exiting\n",filename);break;
            case SIGSEGV:
                printf("Segmentation fault caused from %s\n",filename);break;
            default:
                printf("Caught signal %d from %s\n",signum,filename);
        }
        stackTrace();
        signal(SIGABRT, SIG_DFL);signal(SIGSEGV, SIG_DFL);
        freeExecMemory((void *)mainfunc,size);
        return INT_MAX;
    }
}

int main(int argc,const char *argv[]) {
    initRuntimeEnv(runtime_env);

    if(argc>1){
        return execExecutable(argv[1],argc-1,argv+1);
    }
    printf("Usage: %s <%s file> args ...\n",argv[0],FILEEXT);
    return 0;
}