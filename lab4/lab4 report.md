### 练习3：编写`proc_run` 函数（需要编码）

`proc_run`函数的作用是将指定的进程切换到CPU上运行。其代码如下所示：

```c++
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {  //如果要切换的进程不是当前正在运行的进程，才需要切换
        // LAB4:EXERCISE3 YOUR CODE
        /*
        * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
        * MACROs or Functions:
        *   local_intr_save():        Disable interrupts
        *   local_intr_restore():     Enable Interrupts
        *   lcr3():                   Modify the value of CR3 register
        *   switch_to():              Context switching between two processes
        */
        bool intr_flag;
        
        struct proc_struct *prev = current, *next = proc;   //设置prev指针和next指针指向当前进程和要切换的进程,

        local_intr_save(intr_flag); //禁用中断
        {
            current = proc;     //把要切换过去的进程记为当前进程
            lcr3(next->cr3);    //切换页表，以便使用新进程的地址空间
            switch_to(&(prev->context), &(next->context));  //实现两个进程的context切换
        }
        local_intr_restore(intr_flag);  //允许中断
    }
}
```

这段代码的逻辑是：

- 判断要切换的进程是否就是CPU当前运行的进程，如果不是才需要进行切换。
- 定义两个`proc_struct`指针，分别指向当前进程块和要切换的进程块。
- 使用`/kern/sync/sync.h`中定义好的宏`local_intr_save`禁用中断。
- 把要切换过去的进程记为当前进程
- 使用`/libs/riscv.h`中提供的`lcr3(unsigned int cr3)`函数，修改了cr3寄存器值，切换页表为新进程的页表，以便使用新进程的地址空间。
- 使用`/kern/process`中定义的`switch_to()`函数，实现从原进程到现进程的上下文切换。
- 使用`/kern/sync/sync.h`中定义好的宏`local_intr_restore`允许中断。

**问题：在本实验的执行过程中，创建且运行了几个内核线程？**

答：创建并运行了两个内核线程。

- `idleproc`线程：`idleproc`表示空闲线程。`idleproc`内核线程的工作就是不停地查询，看是否有其他内核线程可以执行了，如果有，马上让调度器选择那个内核线程执行。所以实际上它在`ucore`操作系统没有其他内核线程可执行的情况下才会被调用。主要目的是在系统没有其他任务需要执行时，占用 CPU 时间，同时便于进程调度的统一化。
- `initproc`线程：`initproc`内核线程的工作就是显示“Hello World”，表明自己存在且能正常工作了，证明我们的内核进程实现的没有问题。



### 扩展练习 Challenge：

- **说明语句`local_intr_save(intr_flag);....local_intr_restore(intr_flag);`是如何实现开关中断的？**

在`/kern/sync/sync.h`中定义了`local_intr_save(x)`和`local_intr_restore(x)`来实现关、开中断。具体代码如下所示：

```c++
static inline bool __intr_save(void) {
    if (read_csr(sstatus) & SSTATUS_SIE) {
        intr_disable();
        return 1;
    }
    return 0;
}

static inline void __intr_restore(bool flag) {
    if (flag) {
        intr_enable();
    }
}

#define local_intr_save(x) \
    do {                   \
        x = __intr_save(); \
    } while (0)
#define local_intr_restore(x) __intr_restore(x);

```

​	`local_intr_save(intr_flag)`语句首先在宏定义中调用了`__intr_save()` 函数，并将返回值赋给记录之前中断状态的`bool`变量`intr_flag`。而`__intr_save()` 函数首先使用`read_csr`函数读取了`CSR`寄存器的`sstatus`位，并检查`SSTATUS_SIE`是否被置位。如果`SSTATUS_SIE`被置位，则表示中断处于开启状态。这时调用`intr_disable()`关闭中断，并返回1，否则返回0。总而言之，如果中断之前是开启的，就关闭中断，`intr_flag`为真；如果之前中断是关闭的，`intr_flag`为假。

​	`local_intr_restore(intr_flag)`语句在宏定义中调用了`__intr_restore(x)`函数。该函数判断传入的`intr_flag`参数，如果为真，说明禁用中断前的中断状态是开启的，就调用`intr_enable()`函数开启中断；如果为假说明之前中断就是关闭的，不需要做额外操作。

