/* vim:ts=4 sw=4 expandtab:
 * Copyright (c) 2016 Intel Corporation
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/uart.h>
#include <ctype.h>

#include "app-version.h"

#include <hal/nrf_power.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>

#include <zephyr/bluetooth/chan_idx.h>

#define STACK_SIZE 512
#define PRIORITY 1

/* 1000 msec = 1 sec */

/* The devicetree node identifier for the "led0" alias. */

#define LED0_NODE DT_ALIAS(led0)
#define LED31_NODE DT_ALIAS(led31)
#define LED29_NODE DT_ALIAS(led29)
#define LED2_NODE DT_ALIAS(led2)
#define LED9_NODE DT_ALIAS(led9)

//#define CONFIG_BT_DEVICE_NAME "Ze2"

uint8_t last_channel_index_set1 = 0;
uint8_t force_channel_index1 = 0;
uint8_t force_channel_index_std1 = 0;
uint8_t last_channel_index_std_set1 = 0;
uint8_t last_channel_index1;
uint8_t last_channel_index2;


K_MUTEX_DEFINE(mutex1);

K_THREAD_STACK_DEFINE(thread31_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread29_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread2_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread9_stack, STACK_SIZE);

K_THREAD_STACK_DEFINE(thread_uart_stack, STACK_SIZE);

struct k_thread thread31_data;
struct k_thread thread29_data;
struct k_thread thread2_data;
struct k_thread thread9_data;
struct k_thread thread_uart_data;

#define UNUSED __attribute__((unused))

#include <zephyr/drivers/uart.h>

#define BOOTLOADER_MAGIC_ADDR 0x20007FFCUL
#define BOOTLOADER_MAGIC_VALUE 0xf01669efUL
#define DOUBLE_TAP_MAGIC 0x07738135UL

static void enter_bootloader(void)
{
    //*(volatile uint32_t *)BOOTLOADER_MAGIC_ADDR = BOOTLOADER_MAGIC_VALUE;
    //
	printk("Resetting to bootloader...\n");
    k_sleep(K_MSEC(400));
    *(volatile uint32_t *)BOOTLOADER_MAGIC_ADDR = DOUBLE_TAP_MAGIC;

    NRF_POWER->GPREGRET = 0x57;
    k_sleep(K_MSEC(10));
    NVIC_SystemReset();
}

static void reset_only(void)
{
	printk("Resetting\n");
    k_sleep(K_MSEC(400));
    NVIC_SystemReset();
}

void hexdump(const void *start, size_t size) {
    const uint8_t *ptr = (const uint8_t *)start;
    for (size_t i = 0; i < size; i += 16) {
        printk("%08lx  ", (unsigned long)(uintptr_t)(ptr + i));
        for (size_t j = 0; j < 16; ++j) {
            if (i + j < size)
                printk("%02x ", ptr[i + j]);
            else
                printf("   ");
            if (j == 7) printf(" ");
        }
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < size; ++j) {
            char c = ptr[i + j];
            printk("%c", isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}

void thread_uart_fn(void *led_ptr, void *t1_ptr, void *a3) {

    const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

    uint8_t c;
    while (1) {
        if (uart_fifo_read(uart, &c, 1)) {
            if (c == 'R') {  // Example: type "R" from picocom
                enter_bootloader();
            } else
            if (c == 'r') {  // Example: type "R" from picocom
                reset_only();
            } else
            { //if (c == 'd') {  // Example: type "R" from picocom
                printk("Read char: %u\n", (uint32_t)c);
    //const uint8_t *base = (uint8_t *)0x0;
    //hexdump(base, 0x26000);
            }
        }

        k_sleep(K_MSEC(100));
    }
}

static void scan_recv(const struct bt_le_scan_recv_info *info, struct net_buf_simple *buf)
{

    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(info->addr, addr_str, sizeof(addr_str));

    static uint32_t counter = 0;

    //printk("counter: %i\n", counter++);

	if (info->adv_type == BT_GAP_ADV_TYPE_EXT_ADV 
        //&&
	    //info->adv_props & BT_GAP_ADV_PROP_EXT_ADV 
        //&&
	    //info->adv_props & BT_GAP_ADV_PROP_CONNECTABLE
        ) {
		/* Attempt connection request for device with extended advertisements */
		//memcpy(&ext_addr, info->addr, sizeof(ext_addr));
		//raise_evt(BT_SAMPLE_EVT_EXT_ADV_FOUND);
        //printk("Received extended advertisement\n");
        //printk("PHY: 0x%x, Channel: %d\n", info->phy, info->chan);
        
        //printk("Primary PHY: 0x%x, Secondary PHY: 0x%x\n",
        //   info->primary_phy, info->secondary_phy);

        //info->

        // TODO: dorobic filtrowanie
         //   if( buf->len == 9 && strncmp(buf->data+4, "PWr", 3))            
        printk("chan_idx: %2i rssi: %3i addr: %s tx_power: %4i primary phy: %3i secondary phy: %3i\n", 
            last_channel_index1, 
            info->rssi, 
            addr_str,
            (int)info->tx_power,
            (int)info->primary_phy,
            (int)info->secondary_phy
        );


	}
    else {
        //printk("Received non-extended advertisement\n");

        //info->adv_props;
        //info;

        if(1)

            /*
            printk("chan_idx: %2i rssi: %3i addr: %s (standard), len: %d ", 
                last_channel_index2, info->rssi, addr_str, buf->len
            );

            for(int i=0; i<buf->len; i++)
                printk("%02x ", buf->data[i]
            );
            printk("\n");
            */

            if( buf->len == 9 && strncmp(buf->data+4, "PWr", 3))            
            {
                printk("chan_idx: %2i rssi: %3i addr: %s (standard)\n", 
                    last_channel_index2, info->rssi, addr_str
                );  
            }

    }
}

static struct bt_le_scan_cb scan_callbacks = {
	.recv = scan_recv,
};

int main(void)
{
    const uint32_t *softdevice_base = (uint32_t *)0x1000;

    uint32_t sp = softdevice_base[0];
    uint32_t reset = softdevice_base[1];
	printk("sp: %08x reset: %08x\n", sp, reset);

    //k_sleep(K_MSEC(3000));
    //for(int i=0; i< 0x26000; i++) {
	//    printk("%02x ", base[i]);
    //}
	//printk("\n");


	k_thread_create(&thread_uart_data, thread_uart_stack, STACK_SIZE,
        	thread_uart_fn, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

	printk("Starting Extended Advertising Demo [Scanner]\n");
    printk("Application version: recv-extended-" APP_GIT_HASH "\n");

	/* Initialize the Bluetooth Subsystem */
	int err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return 0;
	}

	bt_le_scan_cb_register(&scan_callbacks);


    struct bt_le_scan_param scan_param = {
        .type       = BT_HCI_LE_SCAN_PASSIVE,
        //.options    = BT_LE_SCAN_OPT_CODED | BT_LE_SCAN_OPT_NO_1M,
        .options    = BT_LE_SCAN_OPT_CODED,
        //.options    = BT_LE_SCAN_OPT_NONE,
        //.options    = BT_LE_SCAN_OPT_CODED /*| BT_LE_SCAN_OPT_FILTER_DUPLICATE*/,
        .interval   = 0x0010,
        .window     = 0x0010,
    };

    //err = bt_le_scan_start(BT_LE_SCAN_PASSIVE, NULL);
    err = bt_le_scan_start(&scan_param, NULL);
	if (err) {
		printk("failed (err %d)\n", err);
	}

	while (true) {
	    //printk("Tick\n");
        k_sleep(K_MSEC(5000));
	}




}
