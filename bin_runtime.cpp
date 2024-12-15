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
using namespace std;

using uchar=unsigned char;
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
}
int execExecutable(const char *filename,int argc,const char *argv[]){
    // argv的第0项是程序目录，从第1项开始是命令行参数
    size_t size;ExecutableMain mainfunc;
    int import_result=import(filename,&size,(void**)(&mainfunc));
    if(import_result!=0)
        throw runtime_error(
            "Import main module failed with code "+to_string(import_result));
    signal(SIGSEGV, signal_handler);
    if(setjmp(jmp_env)==0){
        int result=mainfunc(argc,argv,runtime_env);
        signal(SIGSEGV, SIG_DFL);
        freeExecMemory((void *)mainfunc,size);
        return result;
    }else{
        printf("Segmentation fault caused when executing %s\n",filename);
        signal(SIGSEGV, SIG_DFL);
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