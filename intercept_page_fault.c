#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/traps.h>
#include <asm/desc_defs.h>
#include <linux/sched.h>
#include <linux/moduleparam.h>
#include "my_page_fault.h"

static int my_virt_drv_init(void){
    int retval;
    printk(KERN_INFO "my_virt_drv: Init.\n");
    //register the new page fault handler
    retval = register_my_page_fault_handler();
    if(retval)
        return retval;
    return 0;
}

static void my_virt_drv_exit(void){
    //unregister our new page fault handler
    unregister_my_page_fault_handler();
    printk(KERN_INFO "my_virt_drv: Exit.\n");
}
module_init(my_virt_drv_init);
module_exit(my_virt_drv_exit);
MODULE_LICENSE("Dual BSD/GPL");
