#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "memory.h"
#include "timer.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"
#include "fs.h"

// 负责初始化所有模块
void init_all(void) {

    put_str("init_all\n"); 
    idt_init();                 // 初始化中断
    mem_init();                 // 初始化内存
    thread_init();              // 初始化线程
    timer_init();               // 初始化PIT
    console_init();             // 控制态初始化最好在开中断之前
    keyboard_init();            // 初始化键盘
    tss_init();                 // tss初始化
    syscall_init();             // 初始化系统调用
    intr_enable();              // 后面需要 
    ide_init();                 // 初始化硬盘
	
	sys_malloc(1);

    filesys_init();             // 文件系统初始化

}
