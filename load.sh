#!/bin/bash

LINUX_VERSION=`uname -r`
SYSTEM_MAP=/boot/System.map-${LINUX_VERSION}

if [ ! -f ${SYSTEM_MAP} ]
then
    echo "Error: file ${SYSTEM_MAP} cannot be found."
    exit -1
fi

function get_addr {
    cat ${SYSTEM_MAP} | grep "\<$1\>" | awk '{print "0x"$1}'
}

function check_addr {
    if [ "x"$2 = "x" ]
    then
        echo "Error: $1 does not exist in ${SYSTEM_MAP}"
        exit -1
    fi
}

ADDR_PAGE_FAULT=`get_addr "page_fault"`
check_addr page_fault $ADDR_PAGE_FAULT 

ADDR_DO_PAGE_FAULT=`get_addr "do_page_fault"`
check_addr do_page_fault $ADDR_DO_PAGE_FAULT

ADDR_PV_IRQ_OPS=`get_addr "pv_irq_ops"`
check_addr pv_irq_ops $ADDR_PV_IRQ_OPS

ADDR_ERROR_ENTRY=`get_addr "error_entry"`
check_addr error_entry $ADDR_ERROR_ENTRY

ADDR_ERROR_EXIT=`get_addr "error_exit"`
check_addr error_exit $ADDR_ERROR_EXIT

sudo insmod intercept.ko\
    addr_dft_page_fault=$ADDR_PAGE_FAULT\
    addr_dft_do_page_fault=$ADDR_DO_PAGE_FAULT\
    addr_pv_irq_ops=$ADDR_PV_IRQ_OPS\
    addr_error_entry=$ADDR_ERROR_ENTRY\
    addr_error_exit=$ADDR_ERROR_EXIT
