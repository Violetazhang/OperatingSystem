# Lab 2

## 练习1：理解first-fit 连续物理内存分配算法

#### 1.`default_init`函数

```c++
static void
default_init(void) {
    list_init(&free_list);
    nr_free = 0;
}
```

`default_init`是一个初始化函数，它的作用如下：

- 初始化了一个空闲链表`free_list`
- 将可用的物理页面数`nr_free`初始化为0

#### 2.`default_init_memmap`函数

```c++
static void
default_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;
    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }
}
```

`default_init_memmap`函数负责在内存管理中初始化页框。具体而言：

- **初始化每个物理页**
  - 函数使用循环遍历从 `base` 开始的 `n` 个页，判断是否Reserved，即页面是否被操作系统保留，仅当页面被保留时才可用。将每个页的 `flags` 和 `property` 初始化为0，表示物理页是有效的。使用 `set_page_ref(p, 0)` 将该页的引用计数设置为0，表示该页当前没有被任何引用。
- **设置基本属性**
  - 将块首页`base` 页的 `property` 设置为 `n`，记录了块所管理的页的数量。`SetPageProperty(base)`将base页标记为有效可用。
- **更新空闲页数量。**
  - `nr_free += n`
- **添加页到空闲链表**
  - 检查空闲链表 `free_list` 是否为空。如果为空，将 `base` 页添加到链表中。
  - 如果不为空，使用一个循环将 `base` 页插入到正确的位置，以保持 `free_list` 的有序性。
    - 如果 `base` 页的地址小于当前页，则将其插入当前位置之前。
    - 如果到达链表尾部（即没有小于的页），则将其添加到链表的底部。
  - 注意空闲链表中存的是各个块的base页，相当于存的空闲块。

#### 3.`default_alloc_pages`函数

```c++
static struct Page *
default_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > nr_free) {
        return NULL;
    }
    struct Page *page = NULL;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        if (p->property >= n) {
            page = p;
            break;
        }
    }
    if (page != NULL) {
        list_entry_t* prev = list_prev(&(page->page_link));
        list_del(&(page->page_link));
        if (page->property > n) {
            struct Page *p = page + n;
            p->property = page->property - n;
            SetPageProperty(p);
            list_add(prev, &(p->page_link));
        }
        nr_free -= n;
        ClearPageProperty(page);
    }
    return page;
}
```

`default_alloc_pages`函数的作用是从空闲链表中分配一定数量的页。

- **检查可用页**
  - 如果请求的页数 `n` 大于可用的页数 `nr_free`，则返回NULL。
- ***查找合适的空闲块***
  - 遍历空闲链表，使用 `le2page(le, page_link)` 将链表条目转换为 `struct Page *` 类型，指向当前的base页。检查当前base页的 `property` （即当前空闲块的页数）是否大于或等于请求的页数 `n`。如果是真的，将其保存到 `page` 变量中，表示找到可用页。
- **处理分配的块**
  - 如果找到合适的base页，获取当前页在链表中的前一个节点，将其存储在 `prev` 变量中。然后从链表中删除查找到的合适空闲块。如果 `page->property` 的值大于 `n`，说明块里还有未分配的部分，就要找到块里未分配的第一个页，把它设成base页，并设置其`property`为块里剩下的页数。最后将新的base页重新添加到空闲链表原来的位置。
- **更新可用页数**
  - 减少 `nr_free`的计数，表示n个页投入使用。最后清除分配出去的页中有关可用性的标记。

#### 4.`default_free_pages`函数

```c++
static void
default_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = 0;
        set_page_ref(p, 0);
    }
    base->property = n;
    SetPageProperty(base);
    nr_free += n;

    if (list_empty(&free_list)) {
        list_add(&free_list, &(base->page_link));
    } else {
        list_entry_t* le = &free_list;
        while ((le = list_next(le)) != &free_list) {
            struct Page* page = le2page(le, page_link);
            if (base < page) {
                list_add_before(le, &(base->page_link));
                break;
            } else if (list_next(le) == &free_list) {
                list_add(le, &(base->page_link));
            }
        }
    }

    list_entry_t* le = list_prev(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (p + p->property == base) {
            p->property += base->property;
            ClearPageProperty(base);
            list_del(&(base->page_link));
            base = p;
        }
    }

    le = list_next(&(base->page_link));
    if (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property == p) {
            base->property += p->property;
            ClearPageProperty(p);
            list_del(&(p->page_link));
        }
    }
}
```

`default_free_pages` 函数的主要作用是将一块已分配的块释放回空闲链表中。

- **清理要释放的页**
  - 循环遍历从 `base` 开始到 `base + n` 的页，确保当前页不是被保留的页。然后将页的 `flags` 字段重置为0，表示该页现在是可用的。将当前页的引用计数设置为0，标记其没有被使用。
- **更新页属性和可用页数**
  - 将base页所在块的页数设置为当前释放的页数 n ，然后调用 `SetPageProperty(base)`将base页的状态标记为有效且可用。由于n页被释放，可用页数+n。
- **将释放的页插入到空闲链表**
  - 和之前初始化页同理进行插入。
- **合并相邻的空闲页**
  - 获取 `base` 上一个节点，检查它是不是链表的头部。
    - 如果不是链表头部，且它和base在内存上是连续的，则将它们合并，并清除原本base的状态，合并后的块的首页成为新的base。
    - 如果是，则无法向前合并。
  - 获得 `base` 下一个节点，判断后进行向后合并，逻辑与上述相同。

#### **5.其余函数**

- **`default_nr_free_pages`函数**：返回可用页面数量。
- **`basic_check`函数**：确保内存分配和回收功能的正确性，验证页面管理中的基本功能，如页面的唯一性、引用计数以及空闲链表的维护是否如预期工作。
- **`default_check`函数**：内存管理系统的单元测试，用于验证内存分配和释放的逻辑是否正常，检查了页面的状态，特别是分配、释放、合并以及维护空闲链表的功能。

#### 6. first fit算法是否有进一步的改进空间？

`default_init_memmap`函数和`default_free_pages`函数都有插入页的操作，在用双向链表存储空闲链表的情况下遍历它寻找插入位置，其时间复杂度是`O(n)`，可以改用平衡二叉搜索树来存储，这样时间复杂度能降到`O(logn)`。

## 扩展练习Challenge：硬件可用物理内存范围的获取方法

如果 OS 无法提前知道当前硬件的可用物理内存范围，可用下列办法让 OS 获取可用物理内存范围：

- #### **`BIOS/ UEFI` 访问**

  -  `BIOS`（基本输入输出系统）是一种固件，它在计算机启动时加载并初始化硬件组件，并在此后将控制权转交给操作系统。`UEFI`（统一可扩展固件接口）是 `BIOS` 的继任者，具有更强的扩展性。
  - 在系统启动时，操作系统可以访问 `BIOS` 或 `UEFI` 提供的信息。这些固件常常会提供内存映射的相关信息，包括已经分配的内存和可用的内存区域。操作系统可以通过系统调用来获取这些信息。

- #### `ACPI`

  - `ACPI`（高级配置与电源接口）是一种开放标准，允许操作系统直接控制计算机的电源管理和硬件配置。
  - `ACPI` 规范包含关于系统硬件的信息，包括可用内存区域。操作系统可以通过访问 `ACPI` 表来获取系统的内存配置细节。`ACPI` 通常通过在启动时读取特定内存地址来提供这些信息。

- #### `MMIO`

  - `MMIO`是一种将 I/O 设备的寄存器映射到系统的内存地址空间的方法。当 I/O 设备的控制寄存器和内存 buffers 映射到内存地址时，操作系统和应用程序可以通过读写这些内存地址来与设备进行交互。
  - 通过访问映射到设备的特定内存地址，操作系统能够判断这些地址是否被设备占用。比如操作系统可以尝试读取或写入 `MMIO` 地址，如果访问成功且响应正确，则表明对应的设备是可用的。如果没有响应或发生错误，说明没有占用这些范围。没有被设备占用的区域就是可用的。

- #### 使用内存测试工具

  - 在系统启动过程中，操作系统可以调用特定的内存测试程序（如 `MemTest86` 等）来扫描内存，识别出可用和无效的内存区域，从而让 OS 获取可用物理内存范围。

