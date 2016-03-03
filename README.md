Execute machine code from memory
================================

Proof of concept example: executing machine code from different memory areas: stack, heap, shared memory

## Requires

* `execstack` - utility to mark stack as executable (`prelink.rpm`)

## How to use

```bash
$ make
$ ./run_test.sh
```

## Description

### types of memory

* stack
* heap (malloc)
* file/heap (mmap)
* shared memory (shmget)

### permissions on memory

* `rw` - allocate memory with read/write permissions
* `rwx` - allocate memory with read/write/execute permissions
* `rw-x` - allocate memory with read/write permissions and then manually change permissions to `rwx` with `mprotect` syscall

## Conceptual question


В разрезе "Обратной связи", интересует вопрос:
Насколько опасно и к чему может привести исполняемый код со стэка/кучи/разделяемой памяти?


Стоит учесть, что по умолчанию, у всех приложений стэк / куча / shm - не исполняемые. И если есть, например, косяк в проверке аргументов, и, как следствие, возможность переполнения буфера, ещё не факт что удастся выполнить какой-либо свой код,
так как память не исполняемая.


Приложение, может самостоятельно (создать | изменить атрибуты уже существующей) исполняемый участок памяти. Но стэк, по нормальному, никто исполняемым делать  не будет, а кучу или shm "переполнять" вроде и некуда, если можно.
