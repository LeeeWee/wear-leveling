#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdint.h>

#define RESTRICT_ON_COALESCE_SIZE

#ifdef DEBUG
#define ASSERT(x) \
    do { \
        if (!(x)) { \
            fprintf(stderr, "ASSERT failed at line %d in file %s\n", __LINE__, __FILE__); \
        } \
    } while (0)
#else
#define ASSERT(x)
#endif

#define PTR_TO_OFFSET(p) (((char*)(p)) - start_address)
#define OFFSET_TO_PTR(o) (start_address + (o))

#define PAGE_SIZE 0x1000
#define CACHE_LINE_SIZE 64
#define NEW_ALLOC_PAGES (128 * 1024 / PAGE_SIZE) // 128 KB
#define NIL 0xFFFFFFFFFFFFFFFF
#define BASE_SIZE 64
#define BASE_SIZE_MASK  (BASE_SIZE - 1)
#define NR_SIZES (4 * 1024 / BASE_SIZE)

#define LIST_WEARCOUNT_LIMIT 200
#define INCREASE_LIMIT_THRESHOLD 200

static char *start_address;
static char *free_zone;
static char *end_address;
static unsigned nr_pages;
static uint32_t list_wearcount_limit = LIST_WEARCOUNT_LIMIT;
static uint32_t increase_wearcount_threshold = INCREASE_LIMIT_THRESHOLD;

typedef struct header {
    int64_t size;
} header;

typedef struct volatile_metadata {
    unsigned char state;
    uint32_t MWC; // metadata wear count 
    uint32_t DWC; // data wear count
} volatile_metadata;

// free list head
typedef struct list_head {
    uint64_t position;
    uint32_t wearcount;
    struct list_head *next;
} list_head;

static volatile_metadata **volatile_metadata_list;
static unsigned long volatile_metadata_list_size;
static list_head *free_lists[NR_SIZES + 1];

static uint32_t free_lists_size[NR_SIZES + 1] = {0};
static uint32_t free_lists_size_sum = 0;
static uint32_t list_wearcount_sum[NR_SIZES + 1] = {0};
static uint32_t lists_wearcount_sum = 0;


static inline void clflush(volatile char* __p) 
{
    return;
    asm volatile("clflush %0" : "+m" (__p));
}

static inline void mfence() 
{
    return;
    asm volatile("mfence":::"memory");
}

static inline void flushRange(char* start, char* end) 
{
    char* ptr;
    return;
    for (ptr = start; ptr < end + CACHE_LINE_SIZE; ptr += CACHE_LINE_SIZE) {
        clflush(ptr);
    }
}

// Return the index of the size that best fits an object of size size.
static inline int getBestFit(uint32_t size)
{
    int index;
    if (size > NR_SIZES * BASE_SIZE) {
        return NR_SIZES;
    }
    index = size / BASE_SIZE - 1;
    if (size & BASE_SIZE_MASK) { // size is integer multiple of BASE_SIZE_MASK
        index++;
    }
    return index;
}

static inline uint32_t isFreeForward(uint64_t location)
{
    uint32_t n = 0;
    int32_t index = location / BASE_SIZE;
    uint64_t end = free_zone - start_address;
    while (location < end && !(volatile_metadata_list[index]->state)) {
        n++;
        location += BASE_SIZE;
        index++;
    }
    return n;
}


static inline uint32_t isFreeBackward(uint64_t location) 
{
    uint32_t n = 0;
    int32_t index = location / BASE_SIZE;
    if (start_address + location > free_zone)
        return 0;
    
    index--;
    while (index >= 0 && !(volatile_metadata_list[index]->state)) {
        n++;
        index--;
    }
    return n;
}

// change the state of the volatile metadata to 1
static inline void makeOne(uint64_t offset, uint32_t size) 
{
    unsigned index = offset / BASE_SIZE;
    uint32_t end = index + size / BASE_SIZE;
    for (int i = index; i < end; i++) {
        volatile_metadata_list[i]->state = 1;
    }
}

// change the state of the volatile metadata to 0
static inline void makeZero(uint64_t offset, uint32_t size)
{
    unsigned index = offset / BASE_SIZE;
    uint32_t end = index + size / BASE_SIZE;
    for (int i = index; i < end; i++) {
        volatile_metadata_list[i]->state = 0;
    }
}

static inline void addMetadataWearCount(uint64_t offset, uint32_t size) 
{
    unsigned index = offset / BASE_SIZE;
    for (int i = index; i < index + size; i++) {
        volatile_metadata_list[i]->MWC += 1;
    }
}

// Get more memory from the OS, the size of the newly allocated memory has to be 
// at least size bytes, the default is 4 KB
static inline int getMoreMemory(uint32_t size) 
{
    uint32_t newPages = NEW_ALLOC_PAGES;
    char *addr;
    unsigned long new_volatile_metadata_list_size, i;

    while (size > newPages * PAGE_SIZE) {
        newPages++;
    }

#ifdef _GNU_SOURCE
    addr = (char*) mremap(start_address, nr_pages * PAGE_SIZE,
                          (newPages + nr_pages) * PAGE_SIZE, MREMAP_MAYMOVE);
#else
    addr = (char*) mmap(end_address, newPages * PAGE_SIZE, PROT_READ | PROT_WRITE,
                        MAP_ANON | MAP_PRIVATE | MAP_FIXED, 0, 0);
#endif

    if (addr == MAP_FAILED) {
        perror("MAP FAILED");
        return -1;
    }

#ifdef _GNU_SOURCE
    if (addr != start_address) {
        end_address = end_address + newPages * PAGE_SIZE - start_address + addr;
        free_zone = free_zone - start_address + addr;
        start_address = addr;
    } else {
        end_address += newPages * PAGE_SIZE;
    }
#else
    end_address += newPages * PAGE_SIZE;
#endif

    nr_pages += newPages;

    printf("get_more_memory mmap: %p ~ %p\n", end_address - newPages * PAGE_SIZE, end_address);

    // realloc volatile_metadata_list
    new_volatile_metadata_list_size = volatile_metadata_list_size + newPages * PAGE_SIZE / BASE_SIZE;
    volatile_metadata_list = (volatile_metadata **)realloc(volatile_metadata_list, new_volatile_metadata_list_size * sizeof(volatile_metadata *));
    for (i = volatile_metadata_list_size; i < new_volatile_metadata_list_size; i++) {
        volatile_metadata_list[i] = (volatile_metadata *)malloc(sizeof(volatile_metadata));
        volatile_metadata_list[i]->state = 0;
        volatile_metadata_list[i]->DWC = 0;
        volatile_metadata_list[i]->MWC = 0;
    }
    volatile_metadata_list_size = new_volatile_metadata_list_size;

    return 0;
}


static inline uint32_t sumWearCount(char *location, uint32_t size) 
{
    uint32_t swc = 0;
    uint32_t index = (uint64_t)(location - start_address) / BASE_SIZE;
    uint32_t end = index + size / BASE_SIZE;
    for (int i = index; i < end; i++) {
        swc += volatile_metadata_list[i]->MWC;
    }
    return swc;
}

static inline uint32_t averageWearCount(char *location, uint32_t size)
{
    uint32_t awc = sumWearCount(location, size) / (size / BASE_SIZE);
    return awc;
}

static inline uint32_t worstWearCount(char *location, uint32_t size)
{
    uint32_t wwc = 0;
    uint32_t index = (uint64_t)(location - start_address) / BASE_SIZE;
    uint32_t end = index + size / BASE_SIZE;
    for (int i = index; i < end; i++) {
        if (wwc < volatile_metadata_list[i]->MWC)
            wwc = volatile_metadata_list[i]->MWC;
    }
    return wwc;
}

static inline void removeListHeadFromFreeList(uint64_t offset, uint32_t index)
{
    // the indexed free_list shouldn't be empty
    ASSERT(free_lists[index]);
    list_head *lh = free_lists[index];
    // find the list_head
    if (lh->position == offset) { // if the list_head is the first list_head in the free_list
        if (lh->next) 
            free_lists[index] = lh->next;
        else 
            free_lists[index] = NULL;
    } else { // the list_head is not the first list_head in the free_list
        list_head *prevlh;
        while ( lh && lh->position != offset) {
            prevlh = lh;
            lh = lh->next;
        }

        ASSERT(lh); // Assert when the list_head isn't found in the indexed free_list

        if (lh->next) // the list_head isn't the last list_head
            prevlh->next = lh->next;
        else 
            prevlh->next = NULL;
    }
    
    char *location = OFFSET_TO_PTR(lh->position);
    header *hd = (header *)location;
    
    // ASSERT that the header's size is correct
    if (index < NR_SIZES) {
        ASSERT(-hd->size == (index + 1) * BASE_SIZE);
    } else {
        ASSERT(-hd->size > NR_SIZES * BASE_SIZE);
    }

    // subtract the sumWearCount of this list_head and reduce the free_lists size
    uint32_t sumOfWearCount = sumWearCount(location, -hd->size);
    list_wearcount_sum[index] -= sumOfWearCount;
    lists_wearcount_sum -= sumOfWearCount;
    free_lists_size[index] -= -hd->size / BASE_SIZE;
    free_lists_size_sum -= -hd->size / BASE_SIZE;
    // free this list_head
    free(lh);
}

// Try to extend the given free location with neighbouring free location,
// and remove the neighbouring free location from their related free_list
static inline void extendFreeLocation(char **location, uint32_t *size)
{
    uint32_t n, index;
    uint64_t offset = (uint64_t)(*location - start_address);

    makeZero(offset, *size);

#ifdef RESTRICT_ON_COALESCE_SIZE
    n = isFreeForward(offset);

    if (n * BASE_SIZE != *size) {
        // remove the forward list_head from its free_lists
        uint64_t forwardLocation = offset + *size;
        uint32_t forwardSize = n * BASE_SIZE - *size;
        while (forwardSize > 4096) {
            index = getBestFit(4096);
            removeListHeadFromFreeList(forwardLocation, index);
            forwardLocation += 4096;
            forwardSize -= 4096;
        }
        if (forwardSize > 0) {
            index = getBestFit(forwardSize);
            removeListHeadFromFreeList(forwardLocation, index);
        }
    }

    *size = n * BASE_SIZE;

    n = isFreeBackward(offset);
    if (n > 0) {
        // remove the backward list_head from its free_lists
        uint64_t backwardLocation = offset - n * BASE_SIZE;
        uint32_t backWardSize = n * BASE_SIZE;
        while (backWardSize > 4096) {
            index = getBestFit(4096);
            removeListHeadFromFreeList(backwardLocation, index);
            backwardLocation += 4096;
            backWardSize -= 4096;
        }
        if (backWardSize > 0) {
            index = getBestFit(backWardSize);
            removeListHeadFromFreeList(backwardLocation, index);
        }

        // reset the location value
        *location = (char *)(*location - n * BASE_SIZE);
    }
    *size = *size + n * BASE_SIZE;

#else
    n = isFreeForward(offset);
    if (n * BASE_SIZE != *size) {
        // remove the forward list_head from its free_lists
        uint64_t forwardLocation = offset + *size;
        index = getBestFit(n * BASE_SIZE - *size);
        removeListHeadFromFreeList(forwardLocation, index);
    }

    *size = n * BASE_SIZE;
    
    n = isFreeBackward(offset);
    if (n > 0) {
        // remove the backward list_head from its free_lists
        uint64_t backwardLocation = offset - n * BASE_SIZE;
        index = getBestFit(n * BASE_SIZE);
        removeListHeadFromFreeList(backwardLocation, index);

        // reset the location value
        *location = (char *)(*location - n * BASE_SIZE);
    }

    *size = *size + n * BASE_SIZE;
#endif
}

// Insert the free location in the appropriate free list
static inline void insertFreeLocation(char *location, uint32_t size)
{   
    ASSERT(!(size & BASE_SIZE_MASK));
   
    header *nh;
    list_head *lh;
    int index;

    // if the free blocks is adjacent to the top chunk, merge with top chunk
#ifdef MERGE_WITH_TOP_CHUNK
    if (location + size >= free_zone && location < free_zone) {
        free_zone = location;
        ((header*) location)->size = 0;
        mfence();
        clflush(location);
        mfence();
        return;
    }
#endif

    nh = (header *)location;
    nh->size = -(int32_t)size;
    
    // flush the header containing the new size to memory -- marks an unrevertible delete
    mfence();
    clflush((char *)nh);
    mfence();

    // put hint in free lists
    lh = (list_head*)malloc(sizeof(list_head));
    index = getBestFit(size);
    lh->position = (uint64_t)(location - start_address);

    uint32_t sumOfWearCount = sumWearCount(location, size);
 
#ifdef AVERAGE_WEAR_COUNT
    lh->wearcount = sumOfWearCount / (size / BASE_SIZE);
#else
    lh->wearcount = worstWearCount(location, size);
#endif
    
    // insert to the fit position of the related free_list
    if (!free_lists[index]) { // if the free_list is empty
        free_lists[index] = lh;
        lh->next = NULL;
    }
    else {
        list_head *tmp = free_lists[index];
        // if this list_head is the least allocated one, insert this list_head at the first place
        if (tmp->wearcount >= lh->wearcount) { 
            lh->next = free_lists[index];
            free_lists[index] = lh;
        } else {
            list_head *prev;
            while (tmp && tmp->wearcount < lh->wearcount) {
                prev = tmp;
                tmp = tmp->next;
            }
            if (tmp) {
                prev->next = lh;
                lh->next = tmp;
            } else { // this list_head is the most allocated one, insert this list_head at the tail of the free_list
                prev->next = lh;
                lh->next = NULL;
            }
        }
    }
    // calculate list_wearcout_sum and increase the free_list_size
    list_wearcount_sum[index] += sumOfWearCount;
    lists_wearcount_sum += sumOfWearCount;
    free_lists_size[index] += size / BASE_SIZE;
    free_lists_size_sum += size / BASE_SIZE;
}

static inline void splitAndInsertFreeLocation(char *location, uint32_t size)
{
    ASSERT(!(size & BASE_SIZE_MASK));

    if (size <= 4096) {
        insertFreeLocation(location, size);
    } else {
        uint32_t tmpSize = size;
        char *tmpLocation  = location;
        while (tmpSize > 4096) {
            insertFreeLocation(tmpLocation, 4096);
            tmpLocation = (char *)(tmpLocation + 4096);
            tmpSize -= 4096;
        }
        if (tmpSize > 0)
            insertFreeLocation(tmpLocation, tmpSize);
    }
}


// Given a size, the function returns a pointer to a free region of that size. The region's actual
// size (rounded to a  multiple of BASE_SIZE) is returned in the actualSize argument.
static char *getFreeLocation(uint32_t size, uint32_t *actSize) 
{
    int index = getBestFit(size);
    uint32_t actualSize;
    char *freeLocation = NULL;

    if (index < NR_SIZES) {
        actualSize = (index + 1) * BASE_SIZE;

        // find free space from the indexed free_list, if there is no list_head in the 
        // free_list, find from the larger free_list
        while (!freeLocation && index <= NR_SIZES) {
            if (!free_lists[index]) {
                index++;
                continue;
            }

            if (list_wearcount_sum[index] / free_lists_size[index] > list_wearcount_limit) {
                index++;
                continue;
            }

            list_head *lh = free_lists[index];
            uint64_t location = free_lists[index]->position;
            uint32_t free_space;

            header *hd = (header *)OFFSET_TO_PTR(location);

            // DEBUG
            uint32_t begin = location / BASE_SIZE;
            uint32_t end = begin + index;
            for (int i = begin; i < end; i++) {
                if (volatile_metadata_list[i]->MWC > 12850) {
                    // printf("offset: %ull, index: %u, i: %d\n", location, index, i);
                }   
            }
            
            //test that the free_space is correct
            ASSERT(hd->size < 0);
            if (index < NR_SIZES) {
                free_space = (index + 1) * BASE_SIZE;
                ASSERT(-hd->size == free_space);
            } else {
                free_space = -hd->size;
                ASSERT(-hd->size > NR_SIZES * BASE_SIZE);
            }

            free_lists[index] = free_lists[index]->next;

            // subtract the sumWearCount of this list_head and reduce the free_lists size
            uint32_t sumOfWearCount = sumWearCount((char *)OFFSET_TO_PTR(location), free_space);
            list_wearcount_sum[index] -= sumOfWearCount;
            lists_wearcount_sum -= sumOfWearCount;
            free_lists_size[index] -= free_space / BASE_SIZE;
            free_lists_size_sum -= free_space / BASE_SIZE;

            freeLocation = OFFSET_TO_PTR(location);
            makeOne((uint64_t)(freeLocation - start_address), actualSize);

            // if the free_space is larger than actualSize, split the free_space
            if (free_space > actualSize) {
                char *forwardLocation = OFFSET_TO_PTR(location + actualSize);
                uint32_t size = free_space - actualSize;
#ifdef RESTRICT_ON_COALESCE_SIZE
                extendFreeLocation(&forwardLocation, &size);
                splitAndInsertFreeLocation(forwardLocation, size);
#else
                insertFreeLocation(forwardLocation, size);
#endif
            }

            free(lh);
        }
    }  else {
        if (size & BASE_SIZE_MASK) {
            actualSize = (size & ~BASE_SIZE_MASK) + BASE_SIZE;
        } else {
            actualSize = size;
        }

        // find free location from the last free_list
        if (free_lists[NR_SIZES] && list_wearcount_sum[index] / free_lists_size[index] < list_wearcount_limit) {
            list_head *lh = free_lists[NR_SIZES];
            list_head *prevlh = NULL;
            do {
                header *hd = OFFSET_TO_PTR(lh->position);

                ASSERT(-hd->size > NR_SIZES * BASE_SIZE);
    
                if (-hd->size >= actualSize) {
                    if (!prevlh) { // the lh is the first list_head in this free_list
                        if (lh->next)
                            free_lists[NR_SIZES] = lh->next;
                        else   
                            free_lists[NR_SIZES] = NULL;
                    } else {
                        if (lh->next)
                            prevlh->next = lh->next;
                        else 
                            prevlh->next = NULL;
                    }
                    // subtract the sumWearCount of this list_head and reduce the free_lists size
                    uint32_t sumOfWearCount = sumWearCount((char *)OFFSET_TO_PTR(lh->position), -hd->size);
                    list_wearcount_sum[index] -= sumOfWearCount;
                    lists_wearcount_sum -= sumOfWearCount;
                    free_lists_size[index] -= -hd->size / BASE_SIZE;
                    free_lists_size_sum -= -hd->size / BASE_SIZE;

                    freeLocation = OFFSET_TO_PTR(lh->position);
                    makeOne((uint64_t)(freeLocation - start_address), actualSize);

                    // if the free_space is larger than actualSize, split the free_space
                    if (-hd->size > actualSize) {
                        char *forwardLocation = OFFSET_TO_PTR(lh->position + actualSize);
                        uint32_t size = -hd->size - actualSize;
#ifdef RESTRICT_ON_COALESCE_SIZE
                        extendFreeLocation(&forwardLocation, &size);
                        splitAndInsertFreeLocation(forwardLocation, size);
#else
                        insertFreeLocation(forwardLocation, size);
#endif
                    }
                    
                    // free this list_head
                    free(lh);

                } else {
                    prevlh = lh;
                    lh = lh->next;
                }
            } while (!freeLocation && lh);
        }
    }

    *actSize = actualSize;

    // can not find fit block in the free_lists, allocate memory at the free_zone
    if (!freeLocation) {
        if (free_zone + actualSize > end_address) {
            if (getMoreMemory(actualSize) < 0) {
                return NULL;
            }
        }
        freeLocation = free_zone;
        free_zone += actualSize;
        makeOne((uint64_t)(freeLocation - start_address), actualSize);
    }

    return freeLocation;
}

// Initialization function
void walloc_init(void) {
    int i;
#ifdef _GNU_SOURCE_
    start_address = (char*) mmap(NULL, NEW_ALLOC_PAGES * PAGE_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_ANONYMOUS | MAP_PRIVATE, 0, 0);
#else
    start_address = (char*) mmap(NULL, NEW_ALLOC_PAGES * PAGE_SIZE, PROT_READ | PROT_WRITE,
                                 MAP_ANON | MAP_PRIVATE, 0, 0);
#endif
    if ((void *)start_address == MAP_FAILED) {
        perror("MAP FAILED");
    }
    nr_pages = NEW_ALLOC_PAGES;
    free_zone = start_address;
    end_address = start_address + NEW_ALLOC_PAGES * PAGE_SIZE;

    printf("walloc_init mmap: %p ~ %p\n", start_address, end_address);

    // allocate memory for volatile_metadata_list
    volatile_metadata_list_size = NEW_ALLOC_PAGES * PAGE_SIZE / BASE_SIZE;
    volatile_metadata_list = (volatile_metadata **)malloc(volatile_metadata_list_size * sizeof(volatile_metadata *));
    void *volatile_metadata_start_addr = malloc(volatile_metadata_list_size * sizeof(volatile_metadata));
    for (int i = 0; i < volatile_metadata_list_size; i++) {
        volatile_metadata_list[i] = (volatile_metadata *)((volatile_metadata *)volatile_metadata_start_addr + i);
        volatile_metadata_list[i]->state = 0;
        volatile_metadata_list[i]->DWC = 0;
        volatile_metadata_list[i]->MWC = 0;
    }

    // initialize free_list
    for (i = 0; i <= NR_SIZES; i++) {
        free_lists[i] = NULL;
    } 
}

void walloc_exit(void)
{
    if (munmap((void*) start_address, nr_pages)) {
        perror("MUNMAP FAILED");
    }
}

void *walloc(uint32_t size) 
{
    char *location;
    uint32_t actualSize = 0;
    location = getFreeLocation(size + sizeof(header), &actualSize);

    if (!location) {
        return NULL;
    }

    ((header *) location)->size = actualSize;

    addMetadataWearCount((uint64_t)(location - start_address), actualSize / BASE_SIZE);

    // if (free_lists_size_sum > 0 && lists_wearcount_sum / free_lists_size_sum > increase_wearcount_threshold) {
    //     list_wearcount_limit = 2 * LIST_WEARCOUNT_LIMIT; 
    //     // increase_wearcount_threshold += LIST_WEARCOUNT_LIMIT;
    // }
    
    return (void *)(location + sizeof(header));
}

int wfree(void *addr) 
{
    char *location = ((char *)addr) - sizeof(header);
    uint32_t size;
    if (((header *)location)->size <= 0) {
        fprintf(stderr, "Header corruption detected!\n");
        exit(-1);
    }

    size = (uint32_t)(((header *)location)->size);

    extendFreeLocation(&location, &size);
#ifdef RESTRICT_ON_COALESCE_SIZE
    splitAndInsertFreeLocation(location, size);
#else
    insertFreeLocation(location, size); 
#endif

    // if (free_lists_size_sum > 0 && lists_wearcount_sum / free_lists_size_sum > increase_wearcount_threshold) {
    //     list_wearcount_limit = 2 * LIST_WEARCOUNT_LIMIT; 
    //     // increase_wearcount_threshold += LIST_WEARCOUNT_LIMIT;
    // }
}

void walloc_print(void) 
{
    unsigned i;

    for (i = 0; i < (end_address - start_address) / BASE_SIZE; i++) {
        if (!(i % (PAGE_SIZE / BASE_SIZE))) {
            printf("\n");
        }
        if (i >= (free_zone - start_address) / BASE_SIZE) {
            printf(" f ");
        } else if (volatile_metadata_list[i]->state) {
            printf(" o ");
        } else {
            printf(" _ ");
        }
    }
    printf("\n");

    for (i = 0; i < NR_SIZES; i++) {
        list_head *lh;
        printf("List %u: ", i);
        lh = free_lists[i];
        while (lh != NULL) {
            printf("%llu -> ", lh->position / BASE_SIZE);
            lh = lh->next;
        }
        printf("NULL\n");
    }
}