/******************************************************************************
* Simple application that tries to execute machine code
* from many different memory locations.
* 
* original author:
* https://github.com/ebobby/random-challenges-exercises
* Francisco Soto <ebobby@ebobby.org>
*
* improved by:
* Christian Evans
* https://github.com/Frodox/execute-machine-code-from-memory
******************************************************************************/

/**
 * How to verify system permissions to execute code on stack ?
 * http://stackoverflow.com/questions/15583832/memory-access-permission-need-to-execute-code-from-stack-how-to-verify-system
 * http://linux.die.net/man/8/execstack
 **/

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/shm.h>
#include <sys/stat.h>        /* For mode constants */
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <malloc.h>


#define UNUSED(x) (void)(x)

#define handle_error(msg) \
           do { perror(msg); exit(EXIT_FAILURE); } while (0)

// Execute the given function pointer.
#define EXECUTE_FUNC(FROM, FUNC)                            \
    printf("Executing machine code from '"FROM"': ");       \
    printf("Executed successfully (%d).\n", (FUNC)() );


/******************************************************************************
* Machine code that returns the stack pointer upon entry on the function.
* Since this is defined globally and it is initialized
* it will end up in our data segment.
*******************************************************************************/

// just return exit code: 55
// get from disassembling simple C-code.

unsigned char code[] = {
0x55,                           //    	push   %rbp
0x48, 0x89, 0xe5,               //    	mov    %rsp,%rbp
0xb8, 0x37, 0x00, 0x00, 0x00,   //    	mov    $0x37,%eax
0xc9,                           //    	leaveq 
0xc3                            //    	retq 
};


/******************************************************************************
* This is defined globally but it is NOT initialized so it will end up
* in our bss segment.
******************************************************************************/
unsigned char bss_code[sizeof(code)];

/* Segmentation fault handler */
static void sigsegv_handler (int sig, siginfo_t *si, void *unused) {
    UNUSED(unused);
    printf("SIGSEGV(%d) recieved at address: 0x%lx.\n", sig, (long) si->si_addr);
    exit(EXIT_FAILURE);
}


/* Data bus error handler
 * (usually occurs when accessing unmapped addressing space) */
void sigbus_handler (int sig) {
    printf("SIGBUS(%d) recieved.\n", sig);
    exit(0);
}


/* Execute code off the data segment. */
void execute_from_data_segment () {
    EXECUTE_FUNC("data segment", (unsigned long (*)()) code);
}


/* Execute code off the bss segment. */
void execute_from_bss_segment () {
    memcpy(bss_code, code, sizeof(code));
    EXECUTE_FUNC("bss segment", (unsigned long (*)()) bss_code);
}


/*** STACK ***/

/* just execute code off the stack. */
void execute_from_stack () {
    unsigned char stack_code[sizeof(code)];

    memcpy(stack_code, code, sizeof(code));

    EXECUTE_FUNC("stack", (unsigned long (*)()) stack_code);
}


/* make stack executable and exec */
void execute_from_stack_exec ()
{
    unsigned char stack_code[sizeof(code)];

    memcpy(stack_code, code, sizeof(code));


    /* some dancing to make stack executable */
    
    //  Find page size for this system.
    size_t pagesize = sysconf(_SC_PAGESIZE);

    //  Calculate start and end addresses for the write.
    uintptr_t start = (uintptr_t) &stack_code;
    uintptr_t end = start + sizeof stack_code;

    //  Calculate start of page for mprotect.
    uintptr_t pagestart = start & -pagesize;

    if (-1 == mprotect((void *)pagestart, end - pagestart, PROT_EXEC | PROT_READ | PROT_WRITE))
    {
        perror("mprotect");
        exit(1);
    }

    EXECUTE_FUNC("stack-execed", (unsigned long (*)()) stack_code);
}


/*** HEAP ***/

/* default malloced memory (rw I guess) */
void execute_from_malloc_rw ()
{
    char *ptr = malloc(sizeof(code));

    memcpy(ptr, code, sizeof(code));

    EXECUTE_FUNC("malloc memory (rw)", (unsigned long (*)()) ptr);
}

/* malloced memory rw+x */
void execute_from_malloc_rw_x ()
{
    int pagesize;
    char *buffer;

    pagesize = sysconf(_SC_PAGE_SIZE);
    if (pagesize == -1)
        handle_error("sysconf");

    /* Allocate a buffer aligned on a page boundary;
              initial protection is PROT_READ | PROT_WRITE */
    buffer = memalign(pagesize, 4 * pagesize);
    if (buffer == NULL) {
        perror("memalign");
        exit(EXIT_FAILURE);
    }

    printf("Start of region:        0x%lx\n", (long) buffer);


    if ( mprotect(buffer + pagesize * 2, pagesize, PROT_READ | PROT_WRITE | PROT_EXEC) == -1)
        handle_error("mprotect");

    memcpy(buffer + pagesize * 2, code, sizeof(code));

    EXECUTE_FUNC("malloc memory (rw+x)", (unsigned long (*)()) (buffer + pagesize * 2) );
}


/*** MMAP ***/

/* rw permission */
void execute_from_mmap_write ()
{
    char *ptr = mmap(NULL, sizeof(code), PROT_READ | PROT_WRITE
                                       , MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    memcpy(ptr, code, sizeof(code));

    EXECUTE_FUNC("mmap (rw) memory", (unsigned long (*)()) ptr);
}

/* rwx permission */
void execute_from_mmap_rwx ()
{
    char *ptr = mmap(NULL, sizeof(code), PROT_READ | PROT_WRITE | PROT_EXEC
                                       , MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    memcpy(ptr, code, sizeof(code));

    EXECUTE_FUNC("mmap (rwx) memory", (unsigned long (*)()) ptr);
}

/* set X flag for rw-memory block */
void execute_from_mmap_rw_x ()
{
    char *ptr = mmap(NULL, sizeof(code), PROT_READ | PROT_WRITE
                                       , MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

    memcpy(ptr, code, sizeof(code));

    int rc = mprotect(ptr, sizeof(code), PROT_READ | PROT_WRITE | PROT_EXEC);
    if (-1 == rc) {
        perror("mprotect");
        exit(EXIT_FAILURE);
    }

    EXECUTE_FUNC("mmap (rw+x) memory", (unsigned long (*)()) ptr);
}


/*** SHARED MEMORY  ***/

/* shm_open and mmap with rwx */
void execute_from_shm_open_exec ()
{
    const char *mem_key = "ipc-mem-exec-test";
    size_t mem_length = sizeof(code);

    int fd = shm_open(mem_key, O_RDWR | O_CREAT | O_TRUNC, 0777);
    if (-1 == fd) { perror("shm_open"); exit(1); }

    ftruncate(fd, mem_length);

    char *ptr = mmap(NULL, mem_length, PROT_WRITE | PROT_EXEC
                                     , MAP_ANONYMOUS | MAP_PRIVATE, fd, 0);
    close(fd);

    memcpy(ptr, code, mem_length);

    EXECUTE_FUNC("shm_open memory", (unsigned long (*)()) ptr);

    munmap(ptr, mem_length);
    shm_unlink(mem_key);
}


/* shm_get with rw */
void execute_from_shmget_rw()
{
    key_t key;
    int shmid; // hope, you haven't memory with such id... :D
    char *ptr;

    /* make the key */
    if ((key = ftok("/bin/bash", 'Z')) == -1) // 'Z' - just random key
    {   printf("> /bin/bash\n");
        perror("ftok");
        exit(1);
    }

    /* connect to the shm */
    if ((shmid = shmget(key, sizeof(code), 0666 | IPC_CREAT)) == -1) {
        perror("shmget"); exit(1);
    }

    /* attach to the shm segment  */
    ptr = shmat(shmid, (void *)0, 0);
    if (ptr == (char *)(-1)) { perror("shmat"); exit(1); }

    memcpy(ptr, code, sizeof(code));

    EXECUTE_FUNC("shmget (write) memory", (unsigned long (*)()) ptr);

    /* detach from the segment: */
    if (shmdt(ptr) == -1) { perror("shmdt"); exit(1); }

    // DELETE SHM BLOCK
    if (-1 == shmctl(shmid, IPC_RMID, NULL)) { perror("shmctl"); exit(1); }
}

/* shm_get with rwx */
void execute_from_shmget_rwx()
{
    key_t key;
    int shmid; // hope, you haven't memory with such id... :D
    char *ptr;

    /* make the key */
    if ((key = ftok("/bin/bash", 'Z')) == -1) // 'Z' - just random key
    {   printf("> /bin/bash\n");
        perror("ftok");
        exit(1);
    }

    /* connect to the shm */
    if ((shmid = shmget(key, sizeof(code), 0777 | IPC_CREAT)) == -1) {
        perror("shmget"); exit(1);
    }

    /* attach to the shm segment  */
    ptr = shmat(shmid, (void *)0, 0);
    if (ptr == (char *)(-1)) { perror("shmat"); exit(1); }

    memcpy(ptr, code, sizeof(code));

    EXECUTE_FUNC("shmget (write) memory", (unsigned long (*)()) ptr);

    /* detach from the segment: */
    if (shmdt(ptr) == -1) { perror("shmdt"); exit(1); }

    // DELETE SHM BLOCK
    if (-1 == shmctl(shmid, IPC_RMID, NULL)) { perror("shmctl"); exit(1); }
}

/* shm_get with rw+x */
void execute_from_shmget_rw_x()
{
    key_t key;
    int shmid; // hope, you haven't memory with such id... :D
    char *ptr;

    /* make the key */
    if ((key = ftok("/bin/bash", 'Z')) == -1) // 'Z' - just random key
    {   printf("> /bin/bash\n");
        perror("ftok");
        exit(1);
    }

    /* connect to the shm */
    if ((shmid = shmget(key, sizeof(code), 0666 | IPC_CREAT)) == -1) {
        perror("shmget"); exit(1);
    }

    /* attach to the shm segment  */
    ptr = shmat(shmid, (void *)0, 0);
    if (ptr == (char *)(-1)) { perror("shmat"); exit(1); }

    memcpy(ptr, code, sizeof(code));

    mprotect(ptr, sizeof(code), PROT_EXEC | PROT_READ | PROT_WRITE);

    EXECUTE_FUNC("shmget (write) memory", (unsigned long (*)()) ptr);

    /* detach from the segment: */
    if (shmdt(ptr) == -1) { perror("shmdt"); exit(1); }

    // DELETE SHM BLOCK
    if (-1 == shmctl(shmid, IPC_RMID, NULL)) { perror("shmctl"); exit(1); }
}




void help(char *prog);

int main (int argc, char *argv[]) {

    /* Trap invalid memory related signals. */
    //signal(SIGSEGV, sigsegv_handler);
    //signal(SIGBUS,  sigbus_handler);
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = sigsegv_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1)
        perror("sigaction");


    if (argc < 2)
    {
        help(argv[0]);
        exit(1);
    }

         if (!strcmp("data", argv[1]))
        execute_from_data_segment();
    else if (!strcmp("bss", argv[1]))
        execute_from_bss_segment();
    else if (!strcmp("stack", argv[1]))
        execute_from_stack();
    else if (!strcmp("stack-exec", argv[1]))
        execute_from_stack_exec();
    else if (!strcmp("malloc-rw", argv[1]))
        execute_from_malloc_rw();
    else if (!strcmp("malloc-rw-x", argv[1]))
        execute_from_malloc_rw_x();
    else if (!strcmp("mmap-rw", argv[1]))
        execute_from_mmap_write();
    else if (!strcmp("mmap-rwx", argv[1]))
        execute_from_mmap_rwx();
    else if (!strcmp("mmap-rw-x", argv[1]))
        execute_from_mmap_rw_x();
    else if (!strcmp("shm-open-rwx", argv[1]))
        execute_from_shm_open_exec();
    else if (!strcmp("shmget-rw", argv[1]))
        execute_from_shmget_rw();
    else if (!strcmp("shmget-rwx", argv[1]))
        execute_from_shmget_rwx();
    else if (!strcmp("shmget-rw-x", argv[1]))
        execute_from_shmget_rw_x();
    else {
        help(argv[0]);
        exit(1);
    }

    return 0;
}

void help(char *prog)
{
    printf("Usage: %s [data|bss|stack|stack-exec|malloc-rw|malloc-rw-x|mmap-rw|mmap-rwx|mmap-rw-x|shm-open-rwx|shmget-rw|shmget-rwx|shmget-rw-x]\n", prog);
}
