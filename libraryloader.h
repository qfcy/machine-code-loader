#pragma once
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif
#include <stdexcept>
#include <string>

class LibraryLoader {
public:
    LibraryLoader(const char *libraryName) {
#ifdef _WIN32
        handle = reinterpret_cast<size_t>(LoadLibraryA(libraryName));
        if (handle == 0)
            throw std::runtime_error("Error loading library: " + std::to_string(GetLastError()));
#else
        handle = reinterpret_cast<size_t>(dlopen(libraryName, RTLD_LAZY));
        if (handle == 0)
            throw std::runtime_error("Error loading library: " + std::string(dlerror()));
#endif
    }

    // 获取符号地址
    void *getSymbol(const char *symbolName) {
        if (handle == 0) throw std::runtime_error("Library not loaded");
#ifdef _WIN32
        void *symbol=(void *)GetProcAddress(reinterpret_cast<HMODULE>(handle), symbolName);
#else
        void *symbol=dlsym(reinterpret_cast<void*>(handle), symbolName);
#endif
        return symbol;
    }

    ~LibraryLoader() {
        if (handle != 0) {
#ifdef _WIN32
            FreeLibrary(reinterpret_cast<HMODULE>(handle));
#else
            dlclose(reinterpret_cast<void*>(handle));
#endif
        }
        handle = 0;
    }

    size_t handle; // 库句柄
};
