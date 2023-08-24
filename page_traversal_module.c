#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/mm.h>
#include <linux/unistd.h>
#include <linux/time.h>
#include <asm/uaccess.h>
#include <linux/kallsyms.h>
#include <asm/current.h>


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Process Page Traversal Module");

#define __MY_syscall 335
unsigned long *sys_call_table = 0;

/* 定义一个函数指针，用来保存原来的系统调用*/
static int (*orig_syscall_saved)(void);

unsigned int clear_and_return_cr0(void);
void setback_cr0(unsigned int val);
static int add_syscall(void);
static int remove_syscall(void);

static int sys_mycall(void);

/*
 * 设置cr0寄存器的第16位为0
 */
unsigned int clear_and_return_cr0(void)
{
    unsigned int cr0 = 0;
    unsigned int ret;

    /* 将cr0寄存器的值移动到rax寄存器中，同时输出到cr0变量中 */
    asm volatile ("movq %%cr0, %%rax" : "=a"(cr0));

    ret = cr0;
    cr0 &= 0xfffeffff;  /* 将cr0变量值中的第16位清0，将修改后的值写入cr0寄存器 */

    /* 读取cr0的值到rax寄存器，再将rax寄存器的值放入cr0中 */
    asm volatile ("movq %%rax, %%cr0" :: "a"(cr0));

    return ret;
}

/*
 * 还原cr0寄存器的值为val
 */
void setback_cr0(unsigned int val)
{
    asm volatile ("movq %%rax, %%cr0" :: "a"(val));
}

/*
 * 自己编写的系统调用函数
 */
static int sys_mycall(void)
{
    printk("My syscall is added successfully!!!\n");
    pr_info("the process is\"%s\"(pid %i)\n",current->comm, current->pid);
    return 0;
}

static int add_syscall() {
    int orig_cr0;

    printk("Hack syscall is starting...\n");

    /* 获取 sys_call_table 虚拟内存地址 */
    sys_call_table = (unsigned long *)kallsyms_lookup_name("sys_call_table");

    /* 保存原始系统调用 */
    orig_syscall_saved = (int(*)(void))(sys_call_table[__MY_syscall]);

    orig_cr0 = clear_and_return_cr0(); /* 设置cr0寄存器的第16位为0 */
    sys_call_table[__MY_syscall] = (unsigned long)&sys_mycall; /* 替换成我们编写的函数 */
    setback_cr0(orig_cr0); /* 还原cr0寄存器的值 */

    return 0;
}

static int remove_syscall(void) {
    int orig_cr0;

    orig_cr0 = clear_and_return_cr0();
    sys_call_table[__MY_syscall] = (unsigned long)orig_syscall_saved; /* 设置为原来的系统调用 */
    setback_cr0(orig_cr0);

    printk("Hack syscall is exited....\n");
    return 0;
}

static int __init page_traversal_module_init(void) {
    printk(KERN_INFO "Page Traversal Module: Module loaded\n");
    add_syscall();

    return 0;
}

static void __exit page_traversal_module_exit(void) {
    printk(KERN_INFO "Page Traversal Module: Module unloaded\n");
    remove_syscall();
}

module_init(page_traversal_module_init);
module_exit(page_traversal_module_exit);

