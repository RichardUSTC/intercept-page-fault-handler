#ifndef MY_PAGE_FAULT_H
#define MY_PAGE_FAULT_H
extern int register_my_page_fault_handler(void);
extern void unregister_my_page_fault_handler(void);
#endif
