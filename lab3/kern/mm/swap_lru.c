#include <defs.h>
#include <riscv.h>
#include <stdio.h>
#include <string.h>
#include <swap.h>
#include <swap_lru.h>
#include <list.h>

//复用fifo的代码即可

extern list_entry_t pra_list_head;

static int
_lru_init_mm(struct mm_struct *mm) 
{     
     list_init(&pra_list_head);    //初始化pra_list_head
     mm->sm_priv = &pra_list_head; //mm结构体在kern/mm/vmm.h中定义，sm_priv是它的成员指针，初始化来指向链表头部
    
     return 0;
}

static int
_lru_map_swappable(struct mm_struct *mm, uintptr_t addr, struct Page *page, int swap_in)
{
    list_entry_t *head=(list_entry_t*) mm->sm_priv;    //sm_priv被用作指向LRU链表头部
    
    list_entry_t *entry=&(page->pra_page_link); //kern/mm/memlayout.h中定义了page结构体，pra_page_link是其中的链表项，用于将页面链接到LRU链表中
 
    assert(entry != NULL && head != NULL);

    list_add(head, entry);    //将最近到达的页面链接到 pra_list_head 队列的头，紧紧跟随在head后面
    return 0;
}

static int
_lru_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int in_tick) //找受害者（要换出的页），并把它从链表里移出
{    
     //这里还是复用FIFO的代码，把链表头前面即链表尾的页换出，只不过FIFO是把最晚进来的页放在最前头，LRU是在每次又访问的时候，把原来的删掉，更新其在链表中的位置，更新位置这一点在_lru_tick_event重写，这里还是把表尾拿出来
     list_entry_t *head=(list_entry_t*) mm->sm_priv;
     assert(head != NULL);
     assert(in_tick==0);

    list_entry_t* entry = list_prev(head);   //把head前一个节点取到entry里，即队列中的最后一个节点
    if (entry != head) { //如果不是只有head一个节点
        list_del(entry); //就把它从链表删掉
        *ptr_page = le2page(entry, pra_page_link);
    } else {
        *ptr_page = NULL;
    }
    return 0;
}

static int
_lru_check_swap(void) {
    
    //根据swap.c中的check_content_set可知初始链表是 d(1) c(1) b(1) a(1)(最近到最远)
    swap_tick_event(check_mm_struct);   //调用下面写的tick event函数，其中vmm.c中定义了struct mm_struct *check_mm_struct
    //经过调用，会遍历一遍链表把=1的都调到前头并清0，链表是a(0) b(0) c(0) d(0)
    cprintf("换页,把e换进来\n");
    *(unsigned char *)0x5000 = 0x0e;    //写入虚拟页e对应的物理页地址为0x5000，但是不在内存里在硬盘里，引发缺页异常
    //换页的时候PTE A会置位1
    //链表是e(1) a(0) b(0) c(0) 最近最少访问的d被丢掉
    assert(pgfault_num==5);

    cprintf("访问内存已有的b\n");
    *(unsigned char *)0x2000 = 0x0b;
    pte_t *ptep = get_pte(check_mm_struct->pgdir, 0x2000, 0);//获取这页的页表指针
    //手动把页表项里的PTE_A置位
    *ptep |= PTE_A;
    //链表是e(1) a(0) b(1) c(0)
    assert(pgfault_num==5);
    swap_tick_event(check_mm_struct);
    //链表是b(0) e(0) a(0) c(0)

    cprintf("换页,把d换进来\n");
    *(unsigned char *)0x4000 = 0x0d;    
    //换页的时候PTE A会置位1
    //链表是d(1) b(0) e(0) a(0) 
    assert(pgfault_num==6);

    cprintf("换页,把c换进来\n");
    *(unsigned char *)0x3000 = 0x0c;    
    //换页的时候PTE A会置位1
    assert(pgfault_num==7);

    //链表是c(1) d(1) b(0) e(0) 访问b不会缺页,此时如果是FIFO的话会是cdea,访问b会缺页

    cprintf("访问内存已有的b\n");
    *(unsigned char *)0x2000 = 0x0b;
    ptep = get_pte(check_mm_struct->pgdir, 0x2000, 0);//获取这页的页表指针
    //手动把页表项里的PTE_A置位
    *ptep |= PTE_A;
    //链表是c(1) d(1) b(1) e(0)
    assert(pgfault_num==7); //如果LRU成功，这里还应该是7
    swap_tick_event(check_mm_struct);
    //链表是b(0) d(0) c(0) e(0)
    return 0;
}


static int
_lru_init(void)
{
    return 0;
}

static int
_lru_set_unswappable(struct mm_struct *mm, uintptr_t addr)
{
    return 0;
}

static int
_lru_tick_event(struct mm_struct *mm)
{ 
    list_entry_t *head=(list_entry_t*) mm->sm_priv; //获得链表头
    assert(head != NULL);
    list_entry_t *entry = list_next(head);   // 获取链表中第一个有效数据节点的指针 
    while(entry != head) {  //遍历
        struct Page *page = le2page(entry, pra_page_link);  //转换成Page类型
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);//获取这页的页表项指针
        if(*ptep & PTE_A) { //如果页表项存在PTE_A被置位，说明最近访问了内存里面的
            list_entry_t *before_entry=list_prev(entry);//获取该节点的上一个节点
            list_del(entry);//从链表里删除节点
            list_add(head, entry);//添加到链表最前头，表示是最近访问的
            *ptep &= ~PTE_A;//将 ptep 指向的页表项的访问位清除
            entry=before_entry;//更新指针位置回到该删除节点的上一个节点处
            tlb_invalidate(mm->pgdir, page->pra_vaddr);// 使该页在TLB中失效，以便下次访问时更新
            
        }
        entry=list_next(entry);
    }
    cprintf("_lru_tick_event is called!\n");
    return 0;
}


struct swap_manager swap_manager_lru =  //仿照FIFO，定义了swap_manager结构体的SS实例，并初始化成员
{
     .name            = "lru swap manager",
     .init            = &_lru_init,
     .init_mm         = &_lru_init_mm,
     .tick_event      = &_lru_tick_event,
     .map_swappable   = &_lru_map_swappable,
     .set_unswappable = &_lru_set_unswappable,
     .swap_out_victim = &_lru_swap_out_victim,
     .check_swap      = &_lru_check_swap,
};