### 练习1：理解基于FIFO的页面替换算法（思考题）
##### swap_fifo.c
1. `_fifo_init_mm` 
   初始化函数，初始化FIFO替换算法的队列，并将内核的页面替换指针指向这个指针的相同位置
   ```c
   static int
    _fifo_init_mm(struct mm_struct *mm)
    {     
        list_init(&pra_list_head);
        mm->sm_priv = &pra_list_head;
        //cprintf(" mm->sm_priv %x in fifo_init_mm\n",mm->sm_priv);
        return 0;
    }
   ```
2. `_fifo_map_swappable`
   实现将传入的虚拟地址对应的页面换入队列中：
   `entry`是即将插入队列的页面的指针，head是指向FIFO队列头部的指针；`assert`函数检查确保即将插入的页面的指针和FIFO队列的指针都是有效的；使用 `list_add` 将 page 对应的节点 `entry` 添加到 `head` 后面，从而实现FIFO队列的效果，即最新的页面插入到队列末尾。
   ```c
   static int
    _fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
    {
        list_entry_t *head=(list_entry_t*) mm->sm_priv;
        list_entry_t *entry=&(page->pra_page_link);
    
        assert(entry != NULL && head != NULL);

        list_add(head, entry);
        return 0;
    }
    ```
3. `_fifo_swap_out_victim`
   这个函数用于选择被替换出去的页面，即实现FIFO算法的核心逻辑——将最早进入的页面淘汰出去；
   传入内存管理结构体指针`mm`，包含了进程的内存管理信息，`ptr_page`是指向Page结构体的指针，用于返回选中的淘汰页面（即将选中的页面赋值到这个指针上，`in_tick`是用于确认FIFO策略的参数，要求为0。  
   ___
    两个断言函数，用于确保FIFO队列不为空，同时确认策略参数为0
   ```c
    assert(head != NULL);
    assert(in_tick==0);
    ```
    ___
    通过`list_prev`函数获取队列中最早进入队列的页面,如果返回值不为空，则将这个页面从队列中删掉，即替换出去，同时将这个页面的地址赋值给`ptr_page`，虽然是在替换页面但不是删除页面，页面的地址需要保存下来，否则这片内存的管理就失去信息了。
    ```c
    list_entry_t* entry = list_prev(head);
    if (entry != head) {
        list_del(entry);
        *ptr_page = le2page(entry, pra_page_link);
    } else {
        *ptr_page = NULL;
    }
   ```
4. `_fifo_check_swap`
   这个函数通过访问不同的虚拟页面地址，检查是否发生缺页（pgfault）并记录缺页次数，确保FIFO策略正确工作
   ___
   前面的几次断言确认a,b,c,d页面是在队列中的，依次检查页面c,a,d,b，向虚拟地址（如0x3000）写入值，缺页数不增加，则代表这些虚拟地址的页面在队列中 
   ```c
   cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==4);
    cprintf("write Virt Page a in fifo_check_swap\n");
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==4);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==4);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==4);
    ```
    ___
    向虚拟地址0x5000写入0x0e，判定缺页数是否增加为5，如果增加了则说明队列中发生了页面替换，而接着又向虚拟地址0x2000写入内容，（按照我们设想的逻辑）缺页数没有增加，则说明被替换的页面不是页面b
    ```c
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==5);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==5);
    ```
    ___
    向虚拟地址0x1000写入信息，缺页数增加，说明刚才为了给页面e腾空间而替换出去的页面是页面a；向虚拟地址0x2000、0x3000、0x4000、0x5000写入，缺页数一直增加，到这里可以判断队列最多可以容纳4个虚拟页面，且测试开始前虚拟页面在队列中的顺序为：`a->b->c->d`，因为每访问一个页面就会有之前已经在队列中的页面被替换出去，并且已经实现了闭环。
    ```c
    *(unsigned char *)0x1000 = 0x0a;
    assert(pgfault_num==6);
    cprintf("write Virt Page b in fifo_check_swap\n");
    *(unsigned char *)0x2000 = 0x0b;
    assert(pgfault_num==7);
    cprintf("write Virt Page c in fifo_check_swap\n");
    *(unsigned char *)0x3000 = 0x0c;
    assert(pgfault_num==8);
    cprintf("write Virt Page d in fifo_check_swap\n");
    *(unsigned char *)0x4000 = 0x0d;
    assert(pgfault_num==9);
    cprintf("write Virt Page e in fifo_check_swap\n");
    *(unsigned char *)0x5000 = 0x0e;
    assert(pgfault_num==10);
    ```
5. `_fifo_init`
   FIFO算法初始化函数，但在这个函数里什么都不需要做，直接返回0：
   ```c
   static int
    _fifo_init(void)
    {
        return 0;
    }
    ```
6. `_fifo_set_unswappable`
   传入一个地址，这个函数可以用于将传入地址的页面设置为不可替换页面，但对于FIFO算法而言，ucore这部分功能并没有实现，即这个函数里什么都没有做，直接返回0：
   ```c
   static int
    _fifo_set_unswappable(struct mm_struct *mm, uintptr_t addr)
    {
        return 0;
    }
    ```
##### swap.c
这部分的代码不是FIFO算法的核心代码，但涉及了FIFO算法真正的调用过程  

7. `swap_init`
   用于初始化交换管理器，负责设置交换文件系统以及检查最大可用交换页面数和初始化页面置换算法
   ___
   调用 `swapfs_init`函数初始化交换文件系统，使系统可以存储和恢复页面：
   ```c
   swapfs_init();
   ```
   ___
   检查允许的最大交换页面数量是否在7和`MAX_SWAP_OFFSET_LIMIT`（这个全局变量在`swap.h`中定义为`1<<24`，即10进制的16777216）之间：
   ```c
   if (!(7 <= max_swap_offset &&
        max_swap_offset < MAX_SWAP_OFFSET_LIMIT)) {
        panic("bad max_swap_offset %08x.\n", max_swap_offset);
     }
   ```
   ___
   将页面置换管理器`sm`设置为`swap_manager_clock`，即使用时钟页面置换算法，实现页面替换,如果采用FIFO算法，在测试的时候需要在这个地方改成`swap_manager_fifo`，即在`swap_fifo.c`中包装好的好的管理器的名字
   ```c
   sm = &swap_manager_clock;
   ```
   ___
   调用替换算法的初始化函数，如果是使用FIFO算法则对应的是`swap_fifo.c`中的`_fifo_init`函数，但这个函数根据ucore的逻辑只返回0；如果得到初始化算法返回0的结果，则将页面替换算法完成的标志`swap_init_ok`设置为1，并执行`check_swap`函数，但要注意这个函数的功能与替换算法中的功能不是一样的
   ```c
   int r = sm->init();
     
    if (r == 0)
    {
        swap_init_ok = 1;
        cprintf("SWAP: manager = %s\n", sm->name);
        check_swap();
    }
   ```
   ___

8. `swap_out`
   换出页面函数：将内存中的n个页面交换到磁盘上，进入函数则马上进入一个n次的for循环，每次循环都处理一个页面的更换；完成更换后会返回成功更换的页面的数量，因为更换不是一定成功的，如果在处理某一个页面时，获取换出页面出现失败，则会中断整个函数，并返回当前成功更换的数量
   ___
   调用替换算法的选择更换对象函数，这个函数正常运行会在最后返回0，如果返回的不是0，则说明选择替换的页面过程出现错误
   ```c
    int r = sm->swap_out_victim(mm, &page, in_tick);
    if (r != 0) {
            cprintf("i %d, swap_out: call swap_out_victim failed\n",i);
            break;
    }
   ```
   ___
   调用`get_pte`函数，获取换出的页面的虚拟地址对应的页表项指针，并赋值给`ptep`，使用断言函数保证页表项是有效的，即获取的即将被换掉的页面在内存中是存在的
   ```c
    v=page->pra_vaddr; 
    pte_t *ptep = get_pte(mm->pgdir, v, 0);
    assert((*ptep & PTE_V) != 0);
   ```
   ___
   调用`swapfs_write`将页面内容保存到交换空间中，如果写入失败（swapfs_write 返回非零值），则输出错误信息，调用`map_swappable`函数（这个函数在FIFO中是`_fifo_map_swappable`）取消该页面的可交换状态，并跳过当前页面，继续处理下一个页面；
   如果成功写入，则显示交换到磁盘的虚拟地址和交换条目，并更新页表项，将页表项的物理地址更新为交换空间的偏移值（通过` page->pra_vaddr / PGSIZE + 1` 来计算交换空间的页号，左移8位表示写入的偏移量，左移8位后，页号被扩展为一个可以直接在交换文件中使用的偏移量）
   ```c
    if (swapfs_write( (page->pra_vaddr/PGSIZE+1)<<8, page) != 0) {
        cprintf("SWAP: failed to save\n");
        sm->map_swappable(mm, v, page, 0);
        continue;
    }
    else {
        cprintf("swap_out: i %d, store page in vaddr 0x%x to disk swap entry %d\n", i, v, page->pra_vaddr/PGSIZE+1);
        *ptep = (page->pra_vaddr/PGSIZE+1)<<8;
        free_page(page);
    }
   ```
   ___
   调用`tlb_invalidate`使被换出的页面的TLB快速缓存失效
   ```c
   tlb_invalidate(mm->pgdir, v);
   ```
9.  `swap_in`
    换入页面函数：这个函数的作用是将指定虚拟地址对应的页面从交换空间中读取到物理内存中，并将页面指针存储在ptr_result中
    ___
    调用alloc_page为即将更换的页面从物理内存管理器（如伙伴系统）中分配物理内存空间（简单的说，就是为即将从磁盘或交换空间读取的数据提供实际存放的内存空间），并将其指针存储在result中，同时调用断言函数确保空间分配成功
    ```c
    struct Page *result = alloc_page();
     assert(result!=NULL);
    ```
    ___
    调用`get_pte`函数获取指定虚拟地址在页表中的页表项指针`ptep`，`mm->pgdir`是进程的页目录地址，此操作是为了获取这个将要被换入物理内存的页面的虚拟地址在交换空间中的页表条目，从而确定该页面的在交换空间中的位置
    ```c
    pte_t *ptep = get_pte(mm->pgdir, addr, 0);
    ```
    ___
    swapfs_read函数将交换空间中的页面读取到result指向的物理页面中（前面分配好的空间），函数正确执行会返回0，if语句中断言函数保证读取过程是正确执行的，否则会出发断言
    ```c
    if ((r = swapfs_read((*ptep), result)) != 0)
    {
    assert(r!=0);
    }
    ```
    ___
    完成页面写到分配好的物理空间中这一步后，将页面指针赋值给`ptr_result`，这样外部函数可以获取该页面，并返回 0 表示成功完成操作。
    ```c
    *ptr_result=result;
    return 0;
    ```
10. `check_content_access`
    这个函数会调用替换管理算法中的`check_swap`函数，使用FIFO算法时即对应`_fifo_check_swap`函数，而这个函数则会在`check_swap`中被调用，确保替换算法的正确性，而在前面提到的，`check_swap`会在初始化交换管理器时被调用，即`swap_init`函数中
    ```c
    static inline int
    check_content_access(void)
    {
        int ret = sm->check_swap();
        return ret;
    }
    ```
11. `check_content_set`
    在`_fifo_check_swap`的设置中有描述测试时缺页次数最开始为4次，而且a,b,c,d页均已写入列表，这个工作就是在这个函数中完成的，这个函数同样会在`check_swap`函数中被调用，且先于`check_content_access`函数
    ```c
    static inline void
    check_content_set(void)
    {
        *(unsigned char *)0x1000 = 0x0a;
        assert(pgfault_num==1);
        *(unsigned char *)0x1010 = 0x0a;
        assert(pgfault_num==1);
        *(unsigned char *)0x2000 = 0x0b;
        assert(pgfault_num==2);
        *(unsigned char *)0x2010 = 0x0b;
        assert(pgfault_num==2);
        *(unsigned char *)0x3000 = 0x0c;
        assert(pgfault_num==3);
        *(unsigned char *)0x3010 = 0x0c;
        assert(pgfault_num==3);
        *(unsigned char *)0x4000 = 0x0d;
        assert(pgfault_num==4);
        *(unsigned char *)0x4010 = 0x0d;
        assert(pgfault_num==4);
    }
    ```

### 练习2：深入理解不同分页模式的工作原理（思考题）
##### `get_pte`函数的工作原理：
通过`PDX1`宏计算la（传入的虚拟地址，也就是页面）在一级页表中的索引，从而定位到这个虚拟地址在一级页表中的页目录项，并将这个页目录项赋值给pdep1
```c
pde_t *pdep1 = &pgdir[PDX1(la)];
```
___
`*pdep1 & PTE_V`是一种位运算，检查一级页目录项（即pdep1指向的页表项）是否包含 `PTE_V`标志，结果为真则表示包含此标志，则说明页目录项无效，也就是需要进行页目录项分配环节
```c
if (!(*pdep1 & PTE_V))
```
___
分配页表的核心操作需要在判断传入参数`create`的值后进行，也就是说这个页表项是否分配还是只执行查询操作是需要严格控制的，否则永远都会有属于这个虚拟地址的一页存在；同样还有一个额外的状况就是调用物理页面分配函数时失败，则也会直接返回NULL，即无法为虚拟地址分配一个页表项
```c
if (!create || (page = alloc_page()) == NULL) {
    return NULL;
}
```
___
成功分配一个页面后会将这个页面的引用数设置成1，并调用`page2pa`获取这个页面的物理地址并赋值给`pa`，调用`KADDR`函数将pa转换成相应的虚拟地址，`memset`函数会将虚拟地址的区域填充为0，并且填充的大小为PGSIZE，即一整个页面的大小，这样可以保证这是一个干净的页，不包含之前的数据；
调用`page2ppn`函数获取这个分配的`page`物理页的页框号，调用`pte_create`函数为这个页面创建一个页表项，标志位设置为`PTE_U | PTE_V`（`PTE_V `表示该页表项是有效的，`PTE_U`表示该页是用户可访问的），`PTE_U | PTE_V`进行按位“或”运算将两个标志位结合起来
这样在第一级页表中的工作就基本完成了
```c
set_page_ref(page, 1);
uintptr_t pa = page2pa(page);
memset(KADDR(pa), 0, PGSIZE);
*pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
```
___
`PDE_ADDR`宏可以获取当前页目录项中存储的物理地址（即指向页表的物理地址的低20位），`KADDR`进而将其转换成虚拟地址，然后将内核虚拟地址强制转换为指向页目录项的指针类型，通过`PDX0(la)`提取虚拟地址`la`的中间9位，可以获取虚拟地址在二级页表中的对应项，即页表项，最后将这个指针赋值给`pdep0`，就完成了一级页表的信息读取，可以进行二级页表的操作
```c
pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
```

##### 为何两段代码如此相似：
因为这两段代码的作用分别为：
1. 获取给定虚拟地址la在一级页表中的页表项
2. 获取给定虚拟地址la在二级页表中的页表项

RISC的三种页表模式`SV32`、`SV39`和`SV48`都采用多级页表结构，每一级页表负责一部分虚拟地址的映射查找，通过分层的方式逐级查找，逐步缩小范围，最终获得虚拟地址对应的物理页地址：
1. `SV32`使用32位虚拟地址的**两级**页表，每级用10位索引（共计20位）表示页面的页表项，每个页面大小为 4KB
2. `SV39`使用39位虚拟地址的**三级**页表，每级用9位索引（共计27位）表示页表项，每个页面大小为4KB
3. `SV48`使用48位虚拟地址的**四级**页表，每级用9位索引（共计36位）表示页表项，每个页面大小为4KB

这两段代码完成的工作十分相似，同样的执行的逻辑也几乎一模一样，都是从当前页表中**查找并分配缺失**的页，每层的操作也都类似地包含判断是否存在页表项、分配页表项、清零初始化等逻辑
当前ucore这部分的实现采用的是两级页表的模式，也就是SV32，如果采用更多级的页表模式，在对应的这个函数中也会重复增加这样的一段代码，以实现逐层查找缩小范围

##### 是否考虑拆分：

**综合考虑后的意见是**：分离出一个提供只查询服务的函数接口，同时保留现在实现的按需求分配模式的接口。
**现有模式的优点**：
1. 一个函数就可以满足所有需求，使操作系统更轻量化（但这个增益显然是比较小的）
2. 可以在处理“如果找不到就分配”任务时一个函数可以完成需求，而不需要返回找不到的结果后再调用分配函数进行分配，更利于代码的维护
3. 页表项返回函数始终有效，逻辑更加清晰，后续代码的编写过程可以更省心，而不需要在分配页表的工作逻辑处理上浪费时间

**现有模式的缺点**：
1. 但凡涉及有关访问页表项任务时都需要额外判断是否需要进行分配这个逻辑，然而有部分任务只需要返回查找结果，结果可以是找不到也不分配（只读操作），OS的只读操作并非只占少数，因而这种情况会导致浪费了一部分资源。
2. 可能会造成返回逻辑错误，如果调用函数时仅希望查找，而函数却返回分配错误（如内存不足），这可能会带来额外的处理成本

**为何只增加只读接口而不是读写分离**：
在我的考虑中，OS工作过程中，应该会比较少出现只写但不查询的任务（一般情况下不查询怎么能知道需要分配空间，只分配不查询会导致重复分配的可能），因此分离出一个专门用于分配的借口并不是那么实用

### 练习5：阅读代码和实现手册，理解页表映射方式相关知识（思考题）
#### 多级页表模式优点
##### 1. 离散存储页表
使用多级页表可以使得页表在内存中离散存储，除了一级页表外，其他层级的页表不一定需要占据一片连续的内存空间进行存储，可以是分散的（哪怕是同一级页表，或者紧邻的两项指向的页表，也可以分散在不同内存空间），在操作系统内存较为紧张时，或者内存空间被分配得比较碎片化时，这个特性显得尤为重要
##### 2. 节省页表内存空间
如果使用一级页表，则需要给出明确的一片空间去存储**所有的**页表项（否则没有空出空位给某些页表项，意味着永远都无法使用页表查找到对应的页面了），而采用多级页表，实际上只需要空出第一级页表页表项的空间，等到需要使用页面时再逐层进行分配即可（流程就如同get_pte函数这般），空余的空间使得OS本身更加轻盈、灵活、对内存有更大的可支配空间
##### 3. 灵活高效的权限设置
OS通常需要对不同的内存区域设置不同的访问权限，比如将某些内存区域设置为只读、可写、或仅限于特定权限用户（如内核态），多级页表模式可以轻松实现这样的权限分离和保护，因为分级页表的结构允许根据地址范围创建独立的页表，某种意义上一片连续的区域会有同一个父节点，对这个父节点进行权限设置可以辐射到所有叶子结点，即所有下辖页面；
而一级页表要实现类似的效果，可能需要手动标记和管理每个页表项，可以预见的这是一个十分冗余的操作
#### 一级大页表优点
##### 1. 高速查找
一级页表模式只需要进行一级查找，也就是只需要提取一部分的虚拟地址比特根据索引进行对应查找即可，这种方式在实现上可以达到更快的速度，降低了多次转换地址的开销；出于这个角度的考量，一级页表模式十分适用于**小内存空间的OS**设计，比如嵌入式系统
##### 2. 实现简单
一级页表结构更简单，不需要多级页面访问和复杂的页表切换逻辑，实现上更直接且便于理解，可以有效减少代码中的错误，并且便于维护

