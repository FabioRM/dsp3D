/* SPDX-License-Identifier: BSD-3-Clause */
#include <sys/stat.h>
#include <errno.h>
#include <stddef.h>
extern unsigned char _end,_heap_end;
void *_sbrk(ptrdiff_t increment){
    static unsigned char *current;
    if(!current)current=&_end;
    if(increment<0||increment>&_heap_end-current){errno=ENOMEM;return (void *)-1;}
    unsigned char *previous=current;current+=increment;return previous;
}
int _write(int fd,const char *p,int n){(void)fd;(void)p;return n;}
int _read(int fd,char *p,int n){(void)fd;(void)p;(void)n;return 0;}
int _close(int fd){(void)fd;return -1;}
int _fstat(int fd,struct stat *st){(void)fd;st->st_mode=S_IFCHR;return 0;}
int _isatty(int fd){(void)fd;return 1;}
int _lseek(int fd,int offset,int whence){(void)fd;(void)offset;(void)whence;return 0;}
int _getpid(void){return 1;}
int _kill(int pid,int sig){(void)pid;(void)sig;errno=EINVAL;return -1;}
void _exit(int status){(void)status;for(;;){}}
