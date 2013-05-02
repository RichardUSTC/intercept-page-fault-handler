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

//PGFAULT_NR is the interrupt number of page fault. It is platform specific.
#if defined(CONFIG_X86_64)
#define PGFAULT_NR X86_TRAP_PF
#else
#error This module is only for X86_64 kernel
#endif

static unsigned long new_idt_table_page;
static struct desc_ptr default_idtr;

//addresses of some symbols
static unsigned long addr_dft_page_fault = 0UL; //address of default 'page_fault'
static unsigned long addr_dft_do_page_fault = 0UL; //address of default 'do_page_fault'
static unsigned long addr_pv_irq_ops = 0UL; //address of 'pv_irq_ops'
static unsigned long addr_adjust_exception_frame; //content of pv_irq_ops.adjust_exception_frame, it's a function
static unsigned long addr_error_entry = 0UL;
static unsigned long addr_error_exit = 0UL;

module_param(addr_dft_page_fault, ulong, S_IRUGO);
module_param(addr_dft_do_page_fault, ulong, S_IRUGO);
module_param(addr_pv_irq_ops, ulong, S_IRUGO);
module_param(addr_error_entry, ulong, S_IRUGO);
module_param(addr_error_exit, ulong, S_IRUGO);

#define CHECK_PARAM(x) do{\
    if(!x){\
        printk(KERN_INFO "my_virt_drv: Error: need to set '%s'\n", #x);\
        is_any_unset = 1;\
    }\
    printk(KERN_INFO "my_virt_drv: %s=0x%lx\n", #x, x);\
}while(0)

static int check_parameters(void){
    int is_any_unset = 0;
    CHECK_PARAM(addr_dft_page_fault);
    CHECK_PARAM(addr_dft_do_page_fault);
    CHECK_PARAM(addr_pv_irq_ops);
    CHECK_PARAM(addr_error_entry);
    CHECK_PARAM(addr_error_exit);
    return is_any_unset;
}

typedef void (*do_page_fault_t)(struct pt_regs*, unsigned long);

void my_do_page_fault(struct pt_regs* regs, unsigned long error_code){
    struct task_struct * task = current;
    printk(KERN_INFO "my_virt_drv: page fault detected in process %lu.\n", (unsigned long)task->pid);
    ((do_page_fault_t)addr_dft_do_page_fault)(regs, error_code);
}

asmlinkage void my_page_fault(void);
asm("   .text");
asm("   .type my_page_fault,@function");
asm("my_page_fault:");
asm("   .byte 0x66");
asm("   xchg %ax, %ax");
asm("   callq *addr_adjust_exception_frame");
asm("   sub $0x78, %rsp");
asm("   callq *addr_error_entry");
asm("   mov %rsp, %rdi");
asm("   mov 0x78(%rsp), %rsi");
asm("   movq $0xffffffffffffffff, 0x78(%rsp)");
asm("   callq my_do_page_fault");
asm("   jmpq *addr_error_exit");
asm("   nopl (%rax)");

//this function is copied from kernel source
static inline void pack_gate(gate_desc *gate, unsigned type, unsigned long func,
                         unsigned dpl, unsigned ist, unsigned seg){
    gate->offset_low    = PTR_LOW(func);
    gate->segment       = __KERNEL_CS;
    gate->ist       = ist;
    gate->p         = 1;
    gate->dpl       = dpl;
    gate->zero0     = 0;
    gate->zero1     = 0;
    gate->type      = type;
    gate->offset_middle = PTR_MIDDLE(func);
    gate->offset_high   = PTR_HIGH(func);
}

static void my_load_idt(void *info){
    struct desc_ptr *idtr_ptr = (struct desc_ptr *)info;
    load_idt(idtr_ptr);
}

static int my_fault_init(void){
    //check all the module_parameters are set properly
    if(check_parameters())
        return -1;
    //get the address of 'adjust_exception_frame' from pv_irq_ops struct
    addr_adjust_exception_frame = *(unsigned long *)(addr_pv_irq_ops + 0x30);
    return 0;
}

int register_my_page_fault_handler(void){
    struct desc_ptr idtr;
    gate_desc *old_idt, *new_idt;
    int retval;

    //first, do some initialization work.
    retval = my_fault_init();
    if(retval)
        return retval;

    //record the default idtr
    store_idt(&default_idtr);

    //read the content of idtr register and get the address of old IDT table
    old_idt = (gate_desc *)default_idtr.address; //'default_idtr' is initialized in 'my_virt_drv_init'

    //allocate a page to store the new IDT table
    printk(KERN_INFO "my_virt_drv: alloc a page to store new idt table.\n");
    new_idt_table_page = __get_free_page(GFP_KERNEL);
    if(!new_idt_table_page)
        return -ENOMEM;

    idtr.address = new_idt_table_page;
    idtr.size = default_idtr.size;
    
    //copy the old idt table to the new one
    new_idt = (gate_desc *)idtr.address;
    memcpy(new_idt, old_idt, idtr.size);
    pack_gate(&new_idt[PGFAULT_NR], GATE_INTERRUPT, (unsigned long)my_page_fault, 0, 0, __KERNEL_CS);
    
    //load idt for all the processors
    printk(KERN_INFO "my_virt_drv: load the new idt table.\n");
    load_idt(&idtr);
    printk(KERN_INFO "my_virt_drv: new idt table loaded.\n");
    smp_call_function(my_load_idt, (void *)&idtr, 1); //wait till all are finished
    printk(KERN_INFO "my_virt_drv: all CPUs have loaded the new idt table.\n");
    return 0;
}

void unregister_my_page_fault_handler(void){
    struct desc_ptr idtr;
    store_idt(&idtr);
    //if the current idt is not the default one, restore the default one
    if(idtr.address != default_idtr.address || idtr.size != default_idtr.size){
        load_idt(&default_idtr);
        smp_call_function(my_load_idt, (void *)&default_idtr, 1);
        free_page(new_idt_table_page);
    }
}

MODULE_LICENSE("Dual BSD/GPL");
