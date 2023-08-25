#include <syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>

#define __MY_syscall 335

int main(void)
{
    size_t sz = 16 * 4096;
    char *ptr = (char *) mmap(NULL, sz, 
                                PROT_READ | PROT_WRITE, 
                                MAP_ANON | MAP_PRIVATE, 
                                -1, 0);
    printf("ptr: %p\n", ptr);

    char * carr[3];
    for (int i = 0; i < 3; i ++) {
        carr[i] = ptr + i * 4096;
    }

    // mapped page
    *carr[0] = 1;

    // zero page
    char tmp = *carr[1];
    
    // unmapped page
    char *unmapped_page = carr[2];
    *(unmapped_page) = 2;
    void *ret = mmap(unmapped_page, 4096, 
                        PROT_READ | PROT_WRITE, 
                        MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(ret == unmapped_page);

    printf("%d %d %d\n", *ptr, *(ptr + 4096), *unmapped_page);

    printf("addr: %p %p %p\n", carr[0], carr[1], carr[2]);

    unsigned long sret = syscall(__MY_syscall, carr[0], carr[1], carr[2]);
    // unsigned long sret = syscall(__MY_syscall, 6, 7, 8);
    printf("%d\n", (int)sret);
    return 0;
}