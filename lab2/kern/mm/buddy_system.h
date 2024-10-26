#ifndef __KERN_MM_BUDDY_PMM_H__
#define  __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

extern const struct pmm_manager buddy_pmm_manager;

// extern const struct pmm_manager buddy_pmm_manager;

#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) ( ( (index) + 1 ) / 2 - 1 )

#define IS_POWER_OF_2(x) (!( (x) & ((x) - 1) ))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define MAX_SIZE 10

#endif /* ! __KERN_MM_BUDDY_PMM_H__ */
