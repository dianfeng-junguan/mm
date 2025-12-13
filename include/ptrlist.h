/*
链表数据结构
*/

#define PTRLIST_DEF(type) \
    type* next;           \
    type* prev;

#define PTRLIST_NEXT(node) ((node)->next)
#define PTRLIST_PREV(node) ((node)->prev)

// insert newnode after nodeptr
// reminder: nodeptr is a pointer to a pointer
#define PTRLIST_INSERT(nodeptr, newnode)                   \
    do {                                                   \
        if (*nodeptr) {                                    \
            PTRLIST_INSERT_UNCHECKED(*(nodeptr), newnode); \
        } else {                                           \
            newnode->next = 0;                             \
            newnode->prev = 0;                             \
            *(nodeptr) = (newnode);                        \
        }                                                  \
    } while (0)
#define PTRLIST_INSERT_UNCHECKED(node, newnode) \
    do {                                        \
        (newnode)->next = (node)->next;         \
        (newnode)->prev = (node);               \
        if ((node)->next) {                     \
            (node)->next->prev = (newnode);     \
        }                                       \
        (node)->next = (newnode);               \
    } while (0)

#define PTRLIST_DROP(node)                     \
    do {                                       \
        if ((node)->prev) {                    \
            (node)->prev->next = (node)->next; \
        }                                      \
        if ((node)->next) {                    \
            (node)->next->prev = (node)->prev; \
        }                                      \
        (node)->next = 0;                      \
        (node)->prev = 0;                      \
    } while (0)