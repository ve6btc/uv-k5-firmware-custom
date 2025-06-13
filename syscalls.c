#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

extern char _ebss;   // End of bss, defined in linker script
extern char _estack; // Top of stack

static char *heap_end;

void *_sbrk(ptrdiff_t incr)
{
    if (heap_end == 0)
        heap_end = &_ebss;

    char *prev_heap_end = heap_end;
    char *stack = &_estack;

    if (heap_end + incr > stack) {
        errno = ENOMEM;
        return (void *)-1;
    }

    heap_end += incr;
    return (void *)prev_heap_end;
}

int _close(int file)
{
    (void)file;
    return -1;
}

int _fstat(int file, struct stat *st)
{
    (void)file;
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file)
{
    (void)file;
    return 1;
}

off_t _lseek(int file, off_t ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    return 0;
}

ssize_t _read(int file, void *ptr, size_t len)
{
    (void)file;
    (void)ptr;
    (void)len;
    return 0;
}

ssize_t _write(int file, const void *ptr, size_t len)
{
    (void)file;
    (void)ptr;
    return (ssize_t)len;
}

int _kill(int pid, int sig)
{
    (void)pid;
    (void)sig;
    errno = EINVAL;
    return -1;
}

int _getpid(void)
{
    return 1;
}

void _exit(int status)
{
    (void)status;
    while (1) {
    }
}
