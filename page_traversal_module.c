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

static int sys_mycall(struct pt_regs *regs);

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

static unsigned long get_pte(unsigned long virt_addr) 
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;

    // Get the page global directory (PGD) for the virtual address
    pgd = pgd_offset(current->mm, virt_addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) {
        return -1; // PGD not found or invalid
    }

    // Get p4d
    p4d = p4d_offset(pgd, virt_addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) {
        return -1;
    }

    // Get the page upper directory (PUD)
    pud = pud_offset(p4d, virt_addr);
    if (pud_none(*pud) || pud_bad(*pud)) {
        return -1; // PUD not found or invalid
    }

    // Get the page middle directory (PMD)
    pmd = pmd_offset(pud, virt_addr);
    if (pmd_none(*pmd) || pmd_bad(*pmd)) {
        return -1; // PMD not found or invalid
    }

    // Get the page table entry (PTE)
    ptep = pte_offset_kernel(pmd, virt_addr);
    if (!ptep) {
        return -1; // PTE not found
    }

    // Read the page table entry
    pte = *ptep;

    pr_info("VA: %lx PTE: %lx\n", virt_addr, pte.pte);
    return pte.pte;

}

static unsigned long zero_page_pte = 0;
static unsigned long get_zero_page_pte(void) 
{
    // void *ptr = vmalloc(4096);
    // int i = * (int *)ptr;
    // pr_info("ZERO PAGE PTE:\n");
    // get_pte( (unsigned long) ptr);
    // return 0;
    if (zero_page_pte == 0) 
    {
        pgprot_t pgp = {.pgprot = 0};
        pte_t pte = pte_mkspecial(pfn_pte(my_zero_pfn(0), pgp));
        pr_info("ZERO PAGE PTE: %lx\n", pte.pte >> 8);
        zero_page_pte = (pte.pte >> 8);
    }
    return zero_page_pte;
}

static int is_zero_page(unsigned long virt_addr)
{
    unsigned long virt_pte = (get_pte(virt_addr) << 1) >> 9;
    pr_info("is_zero_page: %lx \t %lx \n", virt_pte, zero_page_pte);
    if (virt_pte == zero_page_pte)
    {
        return 1;
    }
    return 0;
}

static int shoud_sweep(unsigned long virt_addr) 
{
    unsigned long vir_pte = get_pte(virt_addr);

    if (vir_pte == 0) // NULL PTE
    {
        return 0;
    }
    else if (is_zero_page(virt_addr)) // zero page
    {
        return 0;
    }

    return 1;
}

static int pin_one_page(unsigned long virt_addr)
{
    struct page *pages[1];

    get_user_pages_fast(virt_addr & PAGE_MASK, 1, 0, pages);
    return 0;
}

/*
 * 自己编写的系统调用函数
 */
static int sys_mycall(struct pt_regs *regs)
{
    printk("My syscall is added successfully!!!\n");
    pr_info("the process is\"%s\"(pid %i)\n",current->comm, current->pid);

    // 数传递顺序为：rdi，rsi，rdx，r10，r8，r9
    pr_info("arg: %lx, %lx, %lx\n", regs->di, regs->si, regs->dx);

    // char kchar;

    // char *cptr = (char *) regs->di;
    // char c1 = *cptr;

    // copy_from_user(&kchar, cptr, 1);

    // pr_info("char: %d\n", kchar);

    pr_info("Before Pinning\n");
    // check page's mapping information
    get_pte(regs->di);
    get_pte(regs->si);
    get_pte(regs->dx);

    pin_one_page(regs->di);
    pin_one_page(regs->si);
    pin_one_page(regs->dx);

    pr_info("After Pinning\n");
    get_pte(regs->di);
    get_pte(regs->si);
    get_pte(regs->dx);
    
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

    get_zero_page_pte();

    return 0;
}

static void __exit page_traversal_module_exit(void) {
    printk(KERN_INFO "Page Traversal Module: Module unloaded\n");
    remove_syscall();
}

module_init(page_traversal_module_init);
module_exit(page_traversal_module_exit);

