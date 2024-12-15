#include "bin_dk.h"

using ul=unsigned long;
ul fibs(ul num){
	ul i,a=0,b=1,c=0;
	for (i=0;i<num;i++){
		c=a;
		a=a+b;
		b=c;
	}
	return a;
}
int main_bin(int argc,const char *argv[],RuntimeEnv *env){ // 真正的.bin文件的入口函数
    char func[]="fibs",tip[]="Enter a number: ",
         fmt[]="%lu",print_fmt[]="Result: %lu\n",
         errmsg[]="Error importing %s.\n";
    ul num;
    if(env->import(func)!=IMPORT_SUCCESS){
        env->printf(errmsg,func);
        return 1;
    }
    env->printf(tip);
    env->scanf(fmt,&num);
    ul (*fibs)(ul)=(ul (*)(ul))env->getFunc(func);
    int result=static_cast<int>(fibs(num));
    env->printf(print_fmt,result);
    return 0;
}
int main_bin2(int argc,const char *argv[],RuntimeEnv *env){
    char dllname[]="msvcrt.dll",funcname[]="printf",
         msg[]="Args: \n",fmt[]="%s ",fmt_end[]="%s\n\n";
    decltype(printf) *printf_func=(decltype(printf) *)(env->getLibraryFunc(dllname,funcname));
    printf_func(msg);
    for(int i=0;i<argc-1;i++){
        printf_func(fmt,argv[i]);
    }
    if(argc>0)printf_func(fmt_end,argv[argc-1]);
    env->debugModuleInfo();
    return 0;
}
int main() { // 仅用于导出机器码到.bin文件
    DUMP_BIN_MINSIZE(fibs,80);
    DUMP_BIN_MINSIZE(main_bin,512);
    DUMP_BIN_MINSIZE(main_bin2,512);
    return 0;
}