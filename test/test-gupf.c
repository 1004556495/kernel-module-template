#include <syscall.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdlib.h>

#define MADV_PAGEOUT 21

#define __MY_syscall 335

int is_swapped_out(void *vadd)
{
    unsigned char vec;
    int ret = mincore(vadd, 4096, &vec);
    printf("ret:%d, vec:%d\n", ret, vec);
    return (int)vec;

}

int check_swap_out(void *vaddr) 
{
    return 0;
}

int main(void)
{
    void *ret;
    unsigned long sret;

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
    *carr[0] = 16;

    // read/write
    *carr[1] = 17;
    ret = mmap(carr[1], 4096, 
                PROT_WRITE | PROT_READ, 
                MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(ret == carr[1]);
    char tmpc = *carr[1];
    
    // prot_none
    *carr[2] = 18;
    ret = mmap(carr[2], 4096, 
                PROT_NONE, 
                MAP_ANON | MAP_PRIVATE | MAP_FIXED, -1, 0);
    assert(ret == carr[2]);

    sret = syscall(__MY_syscall, carr[0], carr[1], carr[2]);
    printf("%d\n", (int)sret);

    is_swapped_out(carr[0]);
    int madvret = madvise(carr[0], 4096, MADV_PAGEOUT); 
    assert(madvret == 0);

    // sret = syscall(__MY_syscall, carr[0], carr[1], carr[2]);
    // printf("%d\n", (int)sret);

    // int y = 0;
    // int swap_ret = is_swapped_out(carr[0]);
    // while (swap_ret == 1)
    // {
    //     y ++;
    //     printf("y = %d\n", y);
    //     char *ptr = (char *) malloc(4096);
    //     *ptr = 1;
    //     swap_ret = is_swapped_out(carr[0]);
    // }

    // for (int i = 0; i < 836547; i ++) 
    // {
    //     char *cptr = (char *) malloc(4096);
    //     *cptr = 2;
    //     // swap_ret = is_swapped_out(carr[0]);
    // }
    

    // sret = syscall(__MY_syscall, carr[0], carr[1], carr[2]);
    // printf("%d\n", (int)sret);

    // // is_swapped_out(carr[0]);
    // *carr[0] = 18;
    // is_swapped_out(carr[0]);
    
    // sret = syscall(__MY_syscall, carr[0], carr[1], carr[2]);
    // printf("%d\n", (int)sret);

    return 0;
}