#### 练习1: 加载应用程序并执行（需要编码）

**`do_execv`**函数调用`load_icode`（位于`kern/process/proc.c`中）来加载并解析一个处于内存中的ELF执行文件格式的应用程序。你需要补充`load_icode`的第6步，建立相应的用户内存空间来放置应用程序的代码段、数据段等，且要设置好`proc_struct`结构中的成员变量`trapframe`中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。需设置正确的`trapframe`内容。

请在实验报告中简要说明你的设计实现过程。

`load_icode`的第6步的代码补充如下，这段代码设置了应用进程的中断帧`trapframe`中的内容，确保在执行此进程后，能够从应用程序设定的起始执行地址开始执行。

- `tf->gpr.sp = USTACKTOP`：这句代码设置了用户栈顶。`tf->gpr.sp`是用户栈顶指针，`USTACKTOP`是用户栈的顶部地址，这样当进程返回用户态时，栈指针会指向正确的位置。
- `tf->epc = elf->e_entry`：这句代码设置了用户程序入口。`tf->epc`是用户程序的入口点，`elf->e_entry`是可执行文件（ELF格式）中指定的入口点地址，这样当进程返回用户态时，会从这个地址开始执行。
- `tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE`：这行代码设置了中断帧的`status`字段，使其适用于用户程序，然后清除`SSTATUS_SPP`位并设置`SSTATUS_SPIE`位为1。
  - `SSTATUS_SPP`是`sstatus`寄存器中的一个位，用于指示当前模式是用户模式（0）还是特权模式（1）。这里将其清零，表示设置为用户模式。
  - `SSTATUS_SPIE`是`sstatus`寄存器中的一个位，用于指示是否启用中断。这里将其置为1，表示在用户模式下启用中断。

```C
//(6) setup trapframe for user environment
    struct trapframe *tf = current->tf;
    // Keep sstatus
    uintptr_t sstatus = tf->status;
    memset(tf, 0, sizeof(struct trapframe));
    /* LAB5:EXERCISE1 YOUR CODE
     * should set tf->gpr.sp, tf->epc, tf->status
     * NOTICE: If we set trapframe correctly, then the user level process can return to USER MODE from kernel. So
     *          tf->gpr.sp should be user stack top (the value of sp)
     *          tf->epc should be entry point of user program (the value of sepc)
     *          tf->status should be appropriate for user program (the value of sstatus)
     *          hint: check meaning of SPP, SPIE in SSTATUS, use them by SSTATUS_SPP, SSTATUS_SPIE(defined in risv.h)
     */
    tf->gpr.sp = USTACKTOP;  // 设置用户栈顶
    tf->epc = elf->e_entry;  // 设置用户程序入口
    tf->status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;  // 设置状态为用户态
```



1. 请简要描述这个用户态进程被`ucore`选择占用CPU执行（RUNNING态）到具体执行应用程序第一条指令的整个经过。

   - 当操作系统的调度器从就绪进程队列中选择了一个就绪进程后，通过执行进程切换，就让这个被选上的就绪进程执行了，此时进程就处于运行（running）态了，此时程序进入`user_main`函数中。
   - `user_main`函数是`user_main`内核线程的入口点。在这个函数中，根据是否定义了`TEST`宏，它将执行`TEST`程序或`exit`程序。`KERNEL_EXECVE`和`KERNEL_EXECVE2`宏用于简化系统调用`sys_exec`的触发过程。这些宏封装了系统调用的参数，并使用内联汇编来触发`ebreak`异常，从而进入内核态执行系统调用sys_exec。
   - `sys_exec`中调用`do_execve`函数。`do_execve`函数被调用来加载用户程序。
     - 它首先清理当前进程的内存空间：如果mm不为NULL，则设置页表为内核空间页表，且进一步判断mm的引用计数减1后是否为0，如果为0，则表明没有进程再需要此进程所占用的内存空间，为此将根据mm中的记录，释放进程所占用户空间内存和进程页表本身所占空间。最后把当前进程的mm内存管理指针为空。
     - 然后调用`load_icode`函数，根据ELF格式的文件信息将用户程序加载到内存中。这里涉及到读ELF格式的文件，申请内存空间，建立用户态虚存空间，加载应用程序执行码等。
   - `load_icode`函数的主要工作就是给用户进程建立一个能够让用户进程正常运行的用户环境。
     - 初始化内存管理数据结构
     - 创建页目录表
     - 解析ELF格式
     - 分配物理内存并建立映射
     - 设置用户栈
     - 更新页目录表
     - 设置中断帧

   中断处理完毕后，通过`trapret`，CPU会切换回用户态，并开始执行用户进程的入口点处的第一条指令。

#### 练习2: 父进程复制自己的内存空间给子进程（需要编码）

创建子进程的函数`do_fork`在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过`copy_range`函数（位于`kern/mm/pmm.c`中）实现的，请补充`copy_range`的实现，确保能够正确执行。

请在实验报告中简要说明你的设计实现过程。

- 如何设计实现`Copy on Write`机制？给出概要设计，鼓励给出详细设计。

> Copy-on-write（简称COW）的基本概念是指如果有多个使用者对一个资源A（比如内存块）进行读操作，则每个使用者只需获得一个指向同一个资源A的指针，就可以该资源了。若某使用者需要对这个资源A进行写操作，系统会对该资源进行拷贝操作，从而使得该“写操作”使用者获得一个该资源A的“私有”拷贝—资源B，可对资源B进行写操作。该“写操作”使用者对资源B的改变对于其他的使用者而言是不可见的，因为其他使用者看到的还是资源A。

#### 练习3: 阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

请在实验报告中简要说明你对 fork/exec/wait/exit函数的分析。并回答如下问题：

- 请分析fork/exec/wait/exit的执行流程。重点关注哪些操作是在用户态完成，哪些是在内核态完成？内核态与用户态程序是如何交错执行的？内核态执行结果是如何返回给用户程序的？
- 请给出ucore中一个用户态进程的执行状态生命周期图（包执行状态，执行状态之间的变换关系，以及产生变换的事件或函数调用）。（字符方式画即可）

执行：make grade。如果所显示的应用程序检测都输出ok，则基本正确。（使用的是qemu-1.0.1）

#### 扩展练习 Challenge

1. 实现 Copy on Write （COW）机制

   给出实现源码,测试用例和设计报告（包括在cow情况下的各种状态转换（类似有限状态自动机）的说明）。

   这个扩展练习涉及到本实验和上一个实验“虚拟内存管理”。在ucore操作系统中，当一个用户父进程创建自己的子进程时，父进程会把其申请的用户空间设置为只读，子进程可共享父进程占用的用户内存空间中的页面（这就是一个共享的资源）。当其中任何一个进程修改此用户内存空间中的某页面时，ucore会通过page fault异常获知该操作，并完成拷贝内存页面，使得两个进程都有各自的内存页面。这样一个进程所做的修改不会被另外一个进程可见了。请在ucore中实现这样的COW机制。

   由于COW实现比较复杂，容易引入bug，请参考 https://dirtycow.ninja/ 看看能否在ucore的COW实现中模拟这个错误和解决方案。需要有解释。

   这是一个big challenge.

2. 说明该用户程序是何时被预先加载到内存中的？与我们常用操作系统的加载有何区别，原因是什么？
