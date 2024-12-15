// 存放公共的常量、类型等
#pragma once
const size_t MEMORY_STEP=512; // 查找的内存地址间隔
const char *FILEEXT=".bin";
struct RuntimeVersion{
    unsigned short major;
    unsigned short minor;
    unsigned short revision;
};
const unsigned short RUNTIME_VERSION_MAJOR=1;
const unsigned short RUNTIME_VERSION_MINOR=0;
const unsigned short RUNTIME_VERSION_REVISION=0;
enum ImportResult{
    INVALID_ARGUMENT=-1,
    MODULE_NOT_FOUND=1,
    UNKNOWN_ERROR=3,
    IMPORT_SUCCESS=0,
};
enum Platforms{
    WIN32_=1,
    POSIX=2,
    UNKNOWN=0,
};