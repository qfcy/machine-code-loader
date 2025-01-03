这是应用C++实现动态加载并执行机器码的项目，实现了类似操作系统加载可执行文件（如`.exe`）并执行的功能。  
`.bin`文件格式是一种跨平台的可执行文件格式，一个`.bin`文件包含一个函数的机器码。  
由于`.bin`文件仅依赖于通用的C标准库`RuntimeEnv`，在相同的处理器架构上（如x64），同一个`.bin`文件可以跨Windows和Linux等多种操作系统运行，具备了`.exe`等传统格式所没有的优势。  
此外，`.bin`文件能加载其他`.bin`文件的代码并相互调用，实现导入模块的功能。  

## 环境配置

项目使用[gcc](https://gcc.gnu.org/)编译，此外还需要安装[Python](https://www.python.org)，用于运行`runtime_env_generator.py`。  

## bin_dk.cpp

主程序，在这里编写`.bin`文件的代码，类似Java的JDK。  
用法: `bin_dk`，不带参数。  
运行之后`bin_dk`会从`bin_dk.exe`自身提取函数的机器指令，生成`.bin`文件。  

**bin文件的编写**  
编写bin文件和编写普通C/C++程序相同，但目前需要注意：  
- bin文件目前只能通过env调用外部函数，不能直接调用外部函数，因此无法使用C++的多数特性，甚至`new`、`delete`。(`static_cast`等部分不用调用外部函数的特性除外)
- bin文件不支持定义在常量存储区的字符串，如`const char *s="test";`，需要将常量字符串放在栈上分配，如`char s[]="test";`，由于编译器会将栈上分配的字符串数据存放在代码段，嵌入机器码中。
- `main`函数需要定义`DUMP_BIN`，或者`DUMP_BIN_SIZE`和`DUMP_BIN_MINSIZE`的宏，用来在编译后运行`bin_dk`时导出这些函数的机器码，生成bin文件。
如果导出的bin文件过小，运行时会出现段错误。可以通过在`DUMP_BIN_MINSIZE`中增加导出大小来解决。
- bin文件函数的参数是任意的，但如果要作为主程序运行，参数必须是`(int argc,const char *argv[],RuntimeEnv *env)`。不是这个参数的bin文件能被其他bin文件导入，但不能单独作为主程序运行。
`env`的作用是提供C的标准库函数，如`malloc`，`fopen`等。完整的支持函数列表参见`runtime_env.h`或`runtime_env_generator.py`。

这是一个示例，输出Hello world：  
```cpp
#include "bin_dk.h"

int main_bin(int argc,const char *argv[],RuntimeEnv *env){ // 真正的.bin文件的入口函数
    char msg[]="Hello world!\n";
    env->printf(msg);
    return 0;
}
int main() { // 仅用于导出机器码到.bin文件
    DUMP_BIN(main_bin);
    return 0;
}
```
**一些RuntimeEnv的特有函数和常量**
- `env->version`: 获取当前运行时的版本，如`env->version.major`, `env->version.minor`, `env->version.revision`。
- `env->platform`: 当前运行的平台，目前有`WIN32_`, `POSIX`和`UNKNOWN`。
- `int env->import(const char *modname)`: 导入外部的bin文件作为函数使用，`modname`的格式可以是`module`,`module.bin`,`path/module`,`path/module.bin`的任意一种。导入成功时返回`IMPORT_SUCCESS`，失败时返回其他值，具体值参考`constants.h`。
- `void* env->getFunc(const char *funcname)`: 获取导入的外部bin文件的函数指针，失败时返回`nullptr`。
- `void* env->getLibraryFunc(const char *libname, const char *funcname)`: 获取外部动态库(dll或so文件)的函数，libname是动态库的文件名，funcname是函数名，失败时返回`nullptr`。
动态库会在第一次调用`getLibraryFunc`时自动加载，无需手动加载。
- `void env->freeLibrary(const char *libname)`: 显式释放加载的动态库，释放后如果再次用相同库调用`getLibraryFunc`，库会被重新加载。
- `void env->debugModuleInfo()`: 向`stdout`输出当前已加载的其他bin文件模块，和加载的动态库的信息。
- `void env->stackTrace()`: 向`stderr`输出当前堆栈信息。

## bin_runtime.cpp

负责运行`.bin`文件的程序，提供了C标准库的运行环境，类似Java的JRE。  
命令行：
```
bin_runtime <主程序bin文件> [传递给bin文件的参数1 参数2 ...]
```
`bin_runtime`检测到段错误时，会自行处理错误并输出调试信息。  

## 部分其他文件

- `make.bat`: Windows上构建项目的脚本，不带参数运行。
- `bin_dk.h`: `bin_dk.cpp`开头必须包含的头文件。
- `runtime_env_generator.py`: 用于生成`runtime_env.h`头文件。由于`runtime_env.h`包含的标准库函数过多，难以维护，这里用了Python脚本自动生成`runtime_env.h`。
- `constants.h`: 包含一些常量以及类型。