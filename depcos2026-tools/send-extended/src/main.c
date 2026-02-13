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

#include "app-version.h"

#include <hal/nrf_power.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/hci_vs.h>

#include <zephyr/bluetooth/chan_idx.h>

#define STACK_SIZE 512
#define PRIORITY 1

/* 1000 msec = 1 sec */

/* The devicetree node identifier for the "led0" alias. */
#define __unused__ [[maybe_unused]]

#define LED0_NODE DT_ALIAS(led0)
//#define LED31_NODE DT_ALIAS(led31)
//#define LED29_NODE DT_ALIAS(led29)
//#define LED2_NODE DT_ALIAS(led2)
//#define LED9_NODE DT_ALIAS(led9)

//#define CONFIG_BT_DEVICE_NAME "Ze2"

BUILD_ASSERT(IS_ENABLED(CONFIG_BT_HAS_HCI_VS),
	     "This app requires Zephyr-specific HCI vendor extensions");
    
const int8_t txpower_levels[] = { +99, +8, +7, +6, +5, +4, +3, +2, +0, -4, -8, -12, -16, -20, -40 };
//const int8_t txpower_levels[] __unused__ = { -40, -20, -16, };

K_MUTEX_DEFINE(mutex1);

K_THREAD_STACK_DEFINE(thread0_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_uart_stack, STACK_SIZE);
K_THREAD_STACK_DEFINE(thread_boot1200_stack, STACK_SIZE);

struct k_thread thread0_data;
struct k_thread thread_uart_data;
struct k_thread thread_boot1200_data;


void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl);

#define UNUSED __attribute__((unused))

#include <zephyr/drivers/uart.h>

#define BOOTLOADER_MAGIC_ADDR 0x20007FFCUL
#define BOOTLOADER_MAGIC_VALUE 0xf01669efUL
#define DOUBLE_TAP_MAGIC 0x07738135UL

static void enter_bootloader(void)
{
    //*(volatile uint32_t *)BOOTLOADER_MAGIC_ADDR = BOOTLOADER_MAGIC_VALUE;
    //
    *(volatile uint32_t *)BOOTLOADER_MAGIC_ADDR = DOUBLE_TAP_MAGIC;
    

    printk("Entering bootloader...\n");
    k_sleep(K_MSEC(200));

    NRF_POWER->GPREGRET = 0x57;
    k_sleep(K_MSEC(10));
    NVIC_SystemReset();
}

static void reset_only(void)
{
    printk("Resetting...\n");
    k_sleep(K_MSEC(200));
    NVIC_SystemReset();
}

uint8_t force_channel_index1;
uint8_t last_channel_index_set1;
uint8_t force_channel_index_std1;
uint8_t last_channel_index_std_set1;
uint8_t last_channel_index1;
uint8_t last_channel_index2;
uint8_t standard0_or_extended1;
uint8_t manual0_or_auto1;

uint8_t codedS2;
uint8_t codedS8;

/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */
static const struct gpio_dt_spec led0 = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
//static const struct gpio_dt_spec led31 = GPIO_DT_SPEC_GET(LED31_NODE, gpios);
//static const struct gpio_dt_spec led29 = GPIO_DT_SPEC_GET(LED29_NODE, gpios);
//static const struct gpio_dt_spec led2 = GPIO_DT_SPEC_GET(LED2_NODE, gpios);
//static const struct gpio_dt_spec led9 = GPIO_DT_SPEC_GET(LED9_NODE, gpios);
	
void thread_fn(void *led_ptr, void *t1_ptr, void *a3) {
	struct gpio_dt_spec *led = led_ptr;

	k_mutex_lock(&mutex1, K_FOREVER);
	gpio_pin_configure_dt(led, GPIO_OUTPUT_ACTIVE);
	k_mutex_unlock(&mutex1);

	bool led_state = true;
	int t1 = *(int*)t1_ptr;
	while (1) {		
		
		k_mutex_lock(&mutex1, K_FOREVER);
		gpio_pin_toggle_dt(led);
		k_mutex_unlock(&mutex1);
		
		led_state = !led_state;
		//printf("LED0 state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(t1);
	}
}

const struct device *uart = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));

static void thread_boot1200_fn(void *a, void *b, void *c)
{
    uint32_t dtr = 0;
    uint32_t baud = 0;
    bool saw_1200 = false;

	printk("started bootloader thread\n");

    /* Wait for host to open the port */
    while (1) {
        uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);
        if (dtr) {
            break;
        }
        k_sleep(K_MSEC(100));
    }

    while (1) {
        if (uart_line_ctrl_get(uart,
                               UART_LINE_CTRL_BAUD_RATE,
                               &baud) == 0) {
            if (baud == 1200) {
                saw_1200 = true;
            }
        }

        uart_line_ctrl_get(uart, UART_LINE_CTRL_DTR, &dtr);

        /* 1200 baud + port closed */
        if (saw_1200 && dtr == 0) {
            enter_bootloader();
        }

        k_sleep(K_MSEC(20));
    }
}

int current_power_level_index = 0;
int current_power_level = txpower_levels[0];

int keep_wait_loop = 1;

void thread_uart_fn(void *led_ptr, void *t1_ptr, void *a3) {


    uint8_t c;
    while (1) {
        //if (uart_fifo_read(uart, &c, 1)) {
        int status = uart_poll_in(uart, &c);
        if ( status == 0 ) {
            if (c == 'R') {  // Example: type "R" from picocom
                enter_bootloader();
            } 
            else if (c == 'r') {  // Example: type "R" from picocom
                reset_only();
            }
            else if (c == ' ' ) {
                last_channel_index_set1 = (last_channel_index_set1+1)%37;
                
                printk("forced secondary channel index: %i\n", (uint32_t)last_channel_index_set1 );
            }
            else if (c == '2' ) {
                codedS2 = !codedS2;                
                printk("S=2 coding scheme: %i\n", (uint32_t)codedS2 );
            }
            else if (c == '8' ) {
                codedS8 = !codedS8;                
                printk("S=8 coding scheme: %i\n", (uint32_t)codedS8 );
            }
            else if (c == ',' ) {
                last_channel_index_std_set1 = (last_channel_index_std_set1+1)%3;
                
                printk("forced primary channel index: %i\n", (uint32_t)last_channel_index_std_set1 );
            }
            else if (c== '=' ) {
                force_channel_index1 = !force_channel_index1;
	
                printk("force secondary channel index toggle status: %i\n", (uint32_t)force_channel_index1 );
            }
            else if (c== '+' ) {
                force_channel_index_std1 = !force_channel_index_std1;
	
                printk("force primary channel index toggle status: %i\n", (uint32_t)force_channel_index_std1 );
            }
            if (c== 'a' ) {
                manual0_or_auto1 = !manual0_or_auto1;
	
                printk("auto channel selection: %i\n", (uint32_t)manual0_or_auto1 );
            }
            else if (c== 'x' ) {
                standard0_or_extended1 = !standard0_or_extended1;


	
                printk("primary (0) or secondary (1): %i\n", (uint32_t)standard0_or_extended1 );

                if(standard0_or_extended1) {
                    force_channel_index_std1 = 0;
                    printk("force primary channel index status: %i\n", (uint32_t)force_channel_index_std1 );
                }

                keep_wait_loop = 0;
            }
            else if (c== 's' ) {
                printk("    x => %i (transmits in 0 - primary, 1 - secondary adv. channel)\n", standard0_or_extended1 );
                printk("    = => %i (uses 0 - random , 1 - single secondary channel for extended adv. transmission)\n", force_channel_index1 );
                printk("space => %i is a current secondary channel set for extended adv.\n", last_channel_index_set1 );
                printk("    + => %i (uses 0 - all, 1 - single primary channel for legacy adv. transmission)\n", force_channel_index_std1 );
                printk("    , => %i is a current primary channel set for legacy adv.\n", last_channel_index_std_set1 );
                printk("    p => %i is a current TX power\n", current_power_level);
                printk("    a => %i (mode 0 - manual, 1 - auto - channel selection)\n", manual0_or_auto1);
                if(codedS2)
                    printk("    2 => %i (1 -> S=2 coding scheme selected)\n", codedS2);
                else if(codedS8)
                    printk("    8 => %i (1 -> S=8 coding scheme selected)\n", codedS8);
                else
                    printk("      => (no coding scheme selected)\n");
                // printk("\n", );
            }
            else if (c== 'p' ) {

                int32_t t = (sizeof(txpower_levels) );
                current_power_level_index = (current_power_level_index+1)%t;
                current_power_level = txpower_levels[current_power_level_index];
                printk("current power level index: %i current power level: %i\n", 
                    current_power_level_index,
                    current_power_level                    
                );
                set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, current_power_level );
            }
            else if (c== 'h' || c=='?' ) {
                printk("help:\n");
                printk("\n");
                printk("R - reboot into bootloader\n");
                printk("r - reboot\n");
                printk("x - toggle primary/secondary adv. channel\n");
                printk("= - toggle static/random secondary adv. channel\n");
                printk("+ - toggle static/random primary adv. channel\n");
                printk("space - increment static secondary adv. channel\n");
                printk(", - increment static primary adv. channel\n");
                printk("p - set power\n");
                printk("s - print current settings\n");
                printk("2 - set S=2 coding scheme\n");
                printk("8 - set S=8 coding scheme\n");
                printk("    if both S=2 and S=8 coding schemes selected, S=2 takes precedence\n");
                printk("\n");
                printk("h or ? - this help\n");
                //printk(" - force extended channel status");
            }
        }

        k_sleep(K_MSEC(100));
    }
}

#define BT_HCI_OP_VS_WRITE_TX_POWER BT_OP(BT_OGF_VS, 0x0C)

void set_tx_power(uint8_t handle_type, uint16_t handle, int8_t tx_pwr_lvl)
{
	struct bt_hci_cp_vs_write_tx_power_level *cp;
	struct bt_hci_rp_vs_write_tx_power_level *rp;
	struct net_buf *buf, *rsp = NULL;
	int err;


	buf = bt_hci_cmd_create( BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL, sizeof(*cp) );
	if (!buf) {
		printk("Unable to allocate command buffer\n");
		return;
	}
    
	cp = net_buf_add(buf, sizeof(*cp));
	cp->handle = handle; // sys_cpu_to_le16(handle);
	cp->handle_type = handle_type;
	cp->tx_power_level = tx_pwr_lvl;

	err = bt_hci_cmd_send_sync(BT_HCI_OP_VS_WRITE_TX_POWER_LEVEL,
				   buf, &rsp);
	if (err) {
		printk("Set Tx power err: %d message: %s\n", err, strerror(-err) );
		return;
	}

	rp = (void *)rsp->data;
	printk("Actual Tx Power: %d\n", rp->selected_tx_power);

	net_buf_unref(rsp);
}

void adv_sent_callback_function(struct bt_le_ext_adv *adv, struct bt_le_ext_adv_sent_info *info)
{
    printk("Advertisement sent: event %u\n", info->num_sent);
}

int main_orig(void)
{
//	int t31_sleep = 1000;
	int t0_sleep = 400;
//	int t2_sleep = 2000;
//	int t9_sleep = 111;

    force_channel_index1 = 0;
    last_channel_index_set1 = 0;
    
    force_channel_index_std1 = 0;
    last_channel_index_std_set1 = 0;

    standard0_or_extended1 = 0;

    uart_irq_rx_enable(uart);
	

	k_thread_create(&thread0_data, thread0_stack, STACK_SIZE,
        	thread_fn, (void*)&led0, (void*)&t0_sleep, NULL,
		PRIORITY, 0, K_NO_WAIT);


//	k_thread_create(&thread29_data, thread29_stack, STACK_SIZE,
//        	thread_fn, (void*)&led29, (void*)&t29_sleep, NULL,
//		PRIORITY, 0, K_NO_WAIT);
//	k_thread_create(&thread2_data, thread2_stack, STACK_SIZE,
//        	thread_fn, (void*)&led2, (void*)&t2_sleep, NULL,
//		PRIORITY, 0, K_NO_WAIT);
//	k_thread_create(&thread9_data, thread9_stack, STACK_SIZE,
//        	thread_fn, (void*)&led9, (void*)&t9_sleep, NULL,
//		PRIORITY, 0, K_NO_WAIT);

	k_thread_create(&thread_uart_data, thread_uart_stack, STACK_SIZE,
        	thread_uart_fn, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

    static bt_addr_le_t my_addr = {
        .type = BT_ADDR_LE_RANDOM,
        .a = {{ 0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x01 }},
    };

    bt_id_create(&my_addr, NULL);  // Register custom address


	int err = bt_enable(NULL);
	if (err) {
	        printk("Bluetooth init failed (err %d)\n", err);
        	return 1;
    	}
	printk("Bluetooth initialized\n");
        	
    /*
	struct bt_data d1 = BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR));
    struct bt_data d2 = BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME);
    const struct bt_data ad1[] = { d1, d2 };
    */

	/*
	 * Set Advertisement data. Based on the Eddystone specification:
	 * https://github.com/google/eddystone/blob/master/protocol-specification.md
	 * https://github.com/google/eddystone/tree/master/eddystone-url
	 */
	
    __unused__ struct bt_data bt_data_data_flags = BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR);
    struct bt_data d4 UNUSED = BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe);

    /*
	struct bt_data d5 = BT_DATA_BYTES(BT_DATA_SVC_DATA16,
			      0xaa, 0xfe, // Eddystone UUID
			      0x10, // Eddystone-URL frame type
			      0x00, // Calibrated Tx power at 0m
			      0x00, // URL Scheme Prefix http://www.
			      'z', 'e', 'p', 'h', 'y', 'r',
			      'p', 'r', 'o', 'j', 'e', 'c', 't',
			      0x08); // .org 
    */


	//const struct bt_data ad2[] UNUSED = {
    //    bt_data_data_flags, d4
	//};

		

	/* Set Scan Response data */

	//err = bt_le_adv_start(BT_LE_ADV_CONN_NAME, ad, ARRAY_SIZE(ad), NULL, 0);
    printk("Advertising successfully started\n");


    printk("Application version: send-extended-" APP_GIT_HASH "\n");

    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t count = CONFIG_BT_ID_MAX;

    bt_id_get(addrs, &count);
    // Print the public/random address
    char addr_str[BT_ADDR_LE_STR_LEN];

    printk("count: %i\n", count);
    for(int i=0; i<count; i++) {
        bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
        printk("BLE address: %s\n", addr_str);
    }

    bt_id_get(NULL, &count);
    printk("available count: %i\n", count);
    
    
    //struct bt_le_ext_adv_cb adv_cb = {
    //    .sent = NULL,
    //    .started = NULL,
    //    .stopped = NULL,
    //};

    //err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);


    //if (err) {
    //    printk("Failed to create advertising set (err %d)\n", err);
    //    return;
    //}


	//const struct bt_data sd[] __attribute__((unused)) = { bt_data_device_name, bt_data_temp };
	//const struct bt_data sd0[] __attribute__((unused)) = { bt_data_data_flags };

    //err = bt_le_adv_start(BT_LE_ADV_NCONN, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    


    
    //bt_le_ext_adv_set_tx_power(adv, 8);

    //err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
    //printk("bt_le_adv_start status: (err %d)\n", err);

    //err = bt_le_ext_adv_start(BT_LE_EXT_ADV_CREATE_CONN_NAME, ad, ARRAY_SIZE(ad));
    //err = bt_le_ext_adv_start(BT_LE_EXT_ADV_CREATE_CONN_NAME, &adv_params, ad, ARRAY_SIZE(ad), NULL, 0);

    set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, current_power_level );
    //set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, 8 );
    //set_tx_power( BT_HCI_VS_LL_HANDLE_TYPE_CONN , 0, 0 );

            printk("count: %i\n", count);
        for(int i=0; i<count; i++) 
        {
            bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
            printk("BLE address: %s\n", addr_str);
        }
        


    printk("force_channel_index1: %i, last_channel_index_set1: %i\n", 
        (uint32_t)force_channel_index1, (uint32_t)last_channel_index_set1);
    printk("force_channel_index_std1: %i, last_channel_index_std_set1: %i\n", 
        (uint32_t)force_channel_index_std1, (uint32_t)last_channel_index_std_set1);

    const struct device *temp_dev = DEVICE_DT_GET_ONE(nordic_nrf_temp);
    struct sensor_value temp_val;

    struct bt_data bt_data_device_name = BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME);      

    struct bt_le_ext_adv_cb adv_cb_struct = {
        .sent = adv_sent_callback_function,
    };

    uint8_t mfg_data[4];
    mfg_data[0] = 0x1;
    mfg_data[1] = 0x0;


    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        bt_data_device_name,
        //BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME, strlen(CONFIG_BT_DEVICE_NAME)),
    };

     while (1) {
	    sensor_sample_fetch(temp_dev);
	    sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
	    int16_t temp_c = temp_val.val1 * 100 + temp_val.val2 / 10000;
	    // printk("Die temp: %d.%06d°C return: %d\n", temp_val.val1, temp_val.val2, err);

	    bt_id_get(addrs, &count);
	    // Print the public/random address
	    __unused__ char addr_str[BT_ADDR_LE_STR_LEN];

	    mfg_data[2] = temp_c & 0xFF;
	    mfg_data[3] = (temp_c >> 8) & 0xFF;

	    if (!standard0_or_extended1) { // standard

		    struct bt_data ad3[] = {
			    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
			    bt_data_device_name,
			    // BT_DATA(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME,
			    // strlen(CONFIG_BT_DEVICE_NAME)),
		    };

		    // struct bt_data bt_data_temp =
		    //     BT_DATA(BT_DATA_MANUFACTURER_DATA, mfg_data, sizeof(mfg_data));
		    // const struct bt_data sd2[] UNUSED = {bt_data_data_flags, bt_data_device_name,
		    //		      bt_data_temp};
		    // err = bt_le_adv_start(BT_LE_ADV_NCONN, sd2, ARRAY_SIZE(sd2), NULL, 0);

		    // struct bt_le_adv_param *param1 = BT_LE_ADV_NCONN;
		    // param1[0].options |= BT_LE_ADV_OPT_USE_IDENTITY;

		    struct bt_le_adv_param adv_params[] = {{
			    .options = BT_LE_ADV_OPT_USE_IDENTITY,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    }};

		    err = bt_le_adv_start(adv_params, ad3, ARRAY_SIZE(ad3), NULL, 0);
		    // err = bt_le_adv_start(param1, ad3, ARRAY_SIZE(ad3), NULL, 0);
		    // err = bt_le_adv_start(BT_LE_ADV_NCONN, ad3, ARRAY_SIZE(ad3), NULL, 0);
		    if (err) {
			    printk("bt_le_adv_start status: (err %d)\n", err);
		    }

            keep_wait_loop = 1;
            for(int i=0; i<50 && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }

		    err = bt_le_adv_stop();
		    if (err) {
			    printk("bt_le_adv_stop: (err %d)\n", err);
		    }
	    } else { // extended
		    // err = bt_le_ext_adv_set_data(adv, sd2, ARRAY_SIZE(sd2), NULL, 0);
		    // set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, current_power_level );

		    struct bt_le_ext_adv *adv;

		    struct bt_le_adv_param adv_params = {
			    .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    };

		    err = bt_le_ext_adv_create(&adv_params, &adv_cb_struct, &adv);
		    if (err) {
			    printk("bt_le_ext_adv_create status: (err %d) adv: %p\n", err, adv);
		    }

		    err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
		    if (err) {
			    printk("bt_le_ext_adv_set_data status: (err %d)\n", err);
		    }

		    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
		    if (err) {
			    printk("bt_le_ext_adv_start status: (err %d)\n", err);
		    }

            keep_wait_loop = 1;
            for(int i=0; i<50 && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }
		    err = bt_le_ext_adv_stop(adv);
		    if (err) {
			    printk("bt_le_adv_stop status: (err %d)\n", err);
		    }

		    bt_le_ext_adv_delete(adv);
	    }
    }
    
        


   
	/*
	while (1) {
		ret = gpio_pin_toggle_dt(&led);
		if (ret < 0) {
			return 0;
		}

		led_state = !led_state;
		printf("LED0 state: %s\n", led_state ? "ON" : "OFF");
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
	*/


}

int run_manual(void)
{
	int t0_sleep = 400;

    force_channel_index1 = 0;
    last_channel_index_set1 = 0;
    
    force_channel_index_std1 = 0;
    last_channel_index_std_set1 = 0;

    standard0_or_extended1 = 0;

    codedS2 = 0;
    codedS8 = 0;

    uart_irq_rx_enable(uart);
	

	k_thread_create(&thread0_data, thread0_stack, STACK_SIZE,
        	thread_fn, (void*)&led0, (void*)&t0_sleep, NULL,
		PRIORITY, 0, K_NO_WAIT);

	k_thread_create(&thread_uart_data, thread_uart_stack, STACK_SIZE,
        	thread_uart_fn, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

    static bt_addr_le_t my_addr = {
        .type = BT_ADDR_LE_RANDOM,
        .a = {{ 0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x01 }},
    };

    bt_id_create(&my_addr, NULL);  // Register custom address


	int err = bt_enable(NULL);
	if (err) {
	        printk("Bluetooth init failed (err %d)\n", err);
        	return 1;
    	}
	printk("Bluetooth initialized\n");
	
    __unused__ struct bt_data bt_data_data_flags = BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR);
    struct bt_data d4 UNUSED = BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe);

    printk("Advertising successfully started\n");
    printk("Application version: send-extended-" APP_GIT_HASH "\n");

    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t count = CONFIG_BT_ID_MAX;

    bt_id_get(addrs, &count);
    char addr_str[BT_ADDR_LE_STR_LEN];

    printk("count: %i\n", count);
    for(int i=0; i<count; i++) {
        bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
        printk("BLE address: %s\n", addr_str);
    }

    bt_id_get(NULL, &count);
    printk("available count: %i\n", count);

    set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, current_power_level );

    printk("count: %i\n", count);
    for(int i=0; i<count; i++) 
    {
        bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
        printk("BLE address: %s\n", addr_str);
    }
        
    printk("force_channel_index1: %i, last_channel_index_set1: %i\n", 
        (uint32_t)force_channel_index1, (uint32_t)last_channel_index_set1);
    printk("force_channel_index_std1: %i, last_channel_index_std_set1: %i\n", 
        (uint32_t)force_channel_index_std1, (uint32_t)last_channel_index_std_set1);

    const struct device *temp_dev = DEVICE_DT_GET_ONE(nordic_nrf_temp);
    struct sensor_value temp_val;

    struct bt_data bt_data_device_name = BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME);      

    struct bt_le_ext_adv_cb adv_cb_struct = {
        .sent = adv_sent_callback_function,
    };

    uint8_t mfg_data[4];
    mfg_data[0] = 0x1;
    mfg_data[1] = 0x0;


    struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        bt_data_device_name,
    };

    while (1) {
	    sensor_sample_fetch(temp_dev);
	    sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
	    int16_t temp_c = temp_val.val1 * 100 + temp_val.val2 / 10000;

	    bt_id_get(addrs, &count);
	    __unused__ char addr_str[BT_ADDR_LE_STR_LEN];

	    mfg_data[2] = temp_c & 0xFF;
	    mfg_data[3] = (temp_c >> 8) & 0xFF;

	    if (!standard0_or_extended1) { // standard

		    struct bt_data ad3[] = {
			    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
			    bt_data_device_name,
		    };

		    struct bt_le_adv_param adv_params[] = {{
			    .options = BT_LE_ADV_OPT_USE_IDENTITY,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    }};

		    err = bt_le_adv_start(adv_params, ad3, ARRAY_SIZE(ad3), NULL, 0);
		    if (err) {
			    printk("bt_le_adv_start status: (err %d)\n", err);
		    }

            keep_wait_loop = 1;
            for(int i=0; i<50 && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }

		    err = bt_le_adv_stop();
		    if (err) {
			    printk("bt_le_adv_stop: (err %d)\n", err);
		    }
	    } else { // extended

		    struct bt_le_ext_adv *adv;

		    struct bt_le_adv_param adv_params = {
			    .options = BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_USE_IDENTITY,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    };

		    err = bt_le_ext_adv_create(&adv_params, &adv_cb_struct, &adv);
		    if (err) {
			    printk("bt_le_ext_adv_create status: (err %d) adv: %p\n", err, adv);
		    }

		    err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
		    if (err) {
			    printk("bt_le_ext_adv_set_data status: (err %d)\n", err);
		    }

		    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
		    if (err) {
			    printk("bt_le_ext_adv_start status: (err %d)\n", err);
		    }

            keep_wait_loop = 1;
            for(int i=0; i<50 && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }
		    err = bt_le_ext_adv_stop(adv);
		    if (err) {
			    printk("bt_le_adv_stop status: (err %d)\n", err);
		    }

		    bt_le_ext_adv_delete(adv);
	    }
    }
}

void run1(const struct bt_data *bt_data_device_name, int std_duration, int ext_duration) {

    if (!standard0_or_extended1) { // standard

		    struct bt_data ad3[] = {
			    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
			    *bt_data_device_name,
		    };

		    struct bt_le_adv_param adv_params[] = {{
			    .options = BT_LE_ADV_OPT_USE_IDENTITY,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    }};

		    int err = bt_le_adv_start(adv_params, ad3, ARRAY_SIZE(ad3), NULL, 0);
		    if (err) {
			    printk("bt_le_adv_start status: (err %d)\n", err);
		    }
            
            for(int i=0; i<std_duration && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }

		    err = bt_le_adv_stop();
		    if (err) {
			    printk("bt_le_adv_stop: (err %d)\n", err);
		    }
	    } else { // extended

            struct bt_data ad[] = {
                BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
                *bt_data_device_name,
            };

            struct bt_le_ext_adv_cb adv_cb_struct = {
                .sent = adv_sent_callback_function,
            };

		    struct bt_le_ext_adv *adv;

            uint32_t options = BT_LE_ADV_OPT_EXT_ADV 
                | BT_LE_ADV_OPT_USE_IDENTITY
                | (( codedS2 || codedS8 ) ? BT_LE_ADV_OPT_CODED : 0)
                | (  codedS2 ? BT_LE_ADV_OPT_REQUIRE_S2_CODING : 
                    ( codedS8 ? BT_LE_ADV_OPT_REQUIRE_S8_CODING : 0 ))
                ;

		    struct bt_le_adv_param adv_params = {
			    .options = options,
			    .interval_min = BT_GAP_ADV_FAST_INT_MIN_2,
			    .interval_max = BT_GAP_ADV_FAST_INT_MAX_2,
			    .id = BT_ID_DEFAULT,
			    .sid = 0,
		    };

/* Extended advertising + LE Coded PHY (Long Range) */
/*
    struct bt_le_adv_param param = *BT_LE_ADV_PARAM(
        BT_LE_ADV_OPT_EXT_ADV | BT_LE_ADV_OPT_CODED,
        BT_GAP_ADV_FAST_INT_MIN_2,
        BT_GAP_ADV_FAST_INT_MAX_2,
        NULL
    );
*/

    /* Optional: choose coding scheme if your controller supports it
     * (requires BT_EXT_ADV_CODING_SELECTION Kconfig; see Zephyr docs)
     *
     * param.options |= BT_LE_ADV_OPT_REQUIRE_S8_CODING; // more range, slower
     * // or:
     * param.options |= BT_LE_ADV_OPT_REQUIRE_S2_CODING; // less range, faster
     */

/*
    err = bt_le_ext_adv_create(&param, NULL, &adv);
    if (err) {
        printk("bt_le_ext_adv_create failed (err %d)\n", err);
        return;
    }
*/


		    int err = bt_le_ext_adv_create(&adv_params, &adv_cb_struct, &adv);
		    if (err) {
			    printk("bt_le_ext_adv_create status: (err %d) adv: %p\n", err, adv);
		    }

		    err = bt_le_ext_adv_set_data(adv, ad, ARRAY_SIZE(ad), NULL, 0);
		    if (err) {
			    printk("bt_le_ext_adv_set_data status: (err %d)\n", err);
		    }

		    err = bt_le_ext_adv_start(adv, BT_LE_EXT_ADV_START_DEFAULT);
		    if (err) {
			    printk("bt_le_ext_adv_start status: (err %d)\n", err);
		    }

            for(int i=0; i<ext_duration && keep_wait_loop; i++)
            {
		        k_sleep(K_MSEC(100));
            }
		    err = bt_le_ext_adv_stop(adv);
		    if (err) {
			    printk("bt_le_adv_stop status: (err %d)\n", err);
		    }

		    bt_le_ext_adv_delete(adv);
	    }
}

int run_auto_manual(void) {
	int t0_sleep = 400;

    force_channel_index1 = 0;
    last_channel_index_set1 = 0;
    
    force_channel_index_std1 = 0;
    last_channel_index_std_set1 = 0;

    standard0_or_extended1 = 0;
    manual0_or_auto1 = 1;

    uart_irq_rx_enable(uart);
	

	k_thread_create(&thread0_data, thread0_stack, STACK_SIZE,
        	thread_fn, (void*)&led0, (void*)&t0_sleep, NULL,
		PRIORITY, 0, K_NO_WAIT);

	k_thread_create(&thread_uart_data, thread_uart_stack, STACK_SIZE,
        	thread_uart_fn, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);

	k_thread_create(&thread_boot1200_data, thread_boot1200_stack, STACK_SIZE,
        	thread_boot1200_fn, NULL, NULL, NULL,
		PRIORITY, 0, K_NO_WAIT);        

    static bt_addr_le_t my_addr = {
        .type = BT_ADDR_LE_RANDOM,
        .a = {{ 0xCA, 0xFE, 0xBA, 0xBE, 0x00, 0x01 }},
    };

    bt_id_create(&my_addr, NULL);  // Register custom address


	int err = bt_enable(NULL);
	if (err) {
	        printk("Bluetooth init failed (err %d)\n", err);
        	return 1;
    	}
	printk("Bluetooth initialized\n");
	
    //__unused__ struct bt_data bt_data_data_flags = BT_DATA_BYTES(BT_DATA_FLAGS, BT_LE_AD_NO_BREDR);
    //struct bt_data d4 UNUSED = BT_DATA_BYTES(BT_DATA_UUID16_ALL, 0xaa, 0xfe);

    printk("Advertising successfully started\n");
    printk("Application version: send-extended-" APP_GIT_HASH "\n");

    bt_addr_le_t addrs[CONFIG_BT_ID_MAX];
    size_t count = CONFIG_BT_ID_MAX;

    bt_id_get(addrs, &count);
    char addr_str[BT_ADDR_LE_STR_LEN];

    printk("count: %i\n", count);
    for(int i=0; i<count; i++) {
        bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
        printk("BLE address: %s\n", addr_str);
    }

    bt_id_get(NULL, &count);
    printk("available count: %i\n", count);

    set_tx_power(BT_HCI_VS_LL_HANDLE_TYPE_ADV, 0, current_power_level );

    printk("count: %i\n", count);
    for(int i=0; i<count; i++) 
    {
        bt_addr_le_to_str(&addrs[0], addr_str, sizeof(addr_str));
        printk("BLE address: %s\n", addr_str);
    }
        
    printk("force_channel_index1: %i, last_channel_index_set1: %i\n", 
        (uint32_t)force_channel_index1, (uint32_t)last_channel_index_set1);
    printk("force_channel_index_std1: %i, last_channel_index_std_set1: %i\n", 
        (uint32_t)force_channel_index_std1, (uint32_t)last_channel_index_std_set1);

    //const struct device *temp_dev = DEVICE_DT_GET_ONE(nordic_nrf_temp);
    //struct sensor_value temp_val;

    struct bt_data bt_data_device_name = BT_DATA_BYTES(BT_DATA_NAME_COMPLETE, CONFIG_BT_DEVICE_NAME);      

    //__unused__ struct bt_le_ext_adv_cb adv_cb_struct = {
    //    .sent = adv_sent_callback_function,
    //};

    uint8_t mfg_data[4];
    mfg_data[0] = 0x1;
    mfg_data[1] = 0x0;


    //struct bt_data ad[] = {
    //    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    //    bt_data_device_name,
    //};

    while (1) {

        keep_wait_loop = 1;

        if(manual0_or_auto1) {
            standard0_or_extended1 = 0;
            run1(&bt_data_device_name, 60, 540);
            standard0_or_extended1 = 1;
            run1(&bt_data_device_name, 60, 540);
        }
        else {
            run1(&bt_data_device_name, 50, 50);
        }

        /*
	    sensor_sample_fetch(temp_dev);
	    sensor_channel_get(temp_dev, SENSOR_CHAN_DIE_TEMP, &temp_val);
	    int16_t temp_c = temp_val.val1 * 100 + temp_val.val2 / 10000;

	    bt_id_get(addrs, &count);
	    __unused__ char addr_str[BT_ADDR_LE_STR_LEN];

	    mfg_data[2] = temp_c & 0xFF;
	    mfg_data[3] = (temp_c >> 8) & 0xFF;
        */


	    
    }
}

int main() {

    run_auto_manual();

}
