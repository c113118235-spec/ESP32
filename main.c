#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#ifndef IBEACON_RSSI
#define IBEACON_RSSI 0xc8
#endif

/* ===== PIR 設定 ===== */
#define PIR_NODE DT_NODELABEL(gpio1)
#define PIR_PIN 3
static const struct device *pir_dev;

/* ---------- 普通 Beacon ---------- */
static const struct bt_data ad_normal[] = {
    { .type = BT_DATA_FLAGS,
      .data_len = 1,
      .data = (uint8_t[]){ BT_LE_AD_NO_BREDR } },
    { .type = BT_DATA_MANUFACTURER_DATA,
      .data_len = 25,
      .data = (uint8_t[]){
          0x4c, 0x00, 0x02, 0x15,
          0x31, 0x32, 0x33, 0x34,
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,
          0xAA, 0xAA,
          0xBB, 0xBB,
          IBEACON_RSSI
      } }
};

/* ---------- 加密 IAN Beacon ---------- */

static uint8_t uuid_ian_plain[16] = {
    0x49, 0x41, 0x4E, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

static uint8_t key[16] = {
    0xAA, 0xBB, 0xCC, 0xDD,
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
    0x99, 0x00, 0xFF, 0xEE
};

static uint8_t uuid_ian_enc[16];

static struct bt_data ad_ian[2];
static uint8_t ad_ian_data[25];

static void encrypt_ian_uuid(int pir_value)
{
    for (int i = 0; i < 16; i++) {
        uuid_ian_enc[i] = uuid_ian_plain[i] ^ key[i];
    }

    /* 把 PIR 狀態放在最後一個 byte */
    uuid_ian_enc[15] = pir_value;

    ad_ian[0].type = BT_DATA_FLAGS;
    ad_ian[0].data_len = 1;
    static uint8_t flags[1] = { BT_LE_AD_NO_BREDR };
    ad_ian[0].data = flags;

    ad_ian_data[0] = 0x4c;
    ad_ian_data[1] = 0x00;
    ad_ian_data[2] = 0x02;
    ad_ian_data[3] = 0x15;

    for (int i = 0; i < 16; i++) {
        ad_ian_data[4 + i] = uuid_ian_enc[i];
    }

    ad_ian_data[20] = 0xAA;
    ad_ian_data[21] = 0xAA;
    ad_ian_data[22] = 0xBB;
    ad_ian_data[23] = 0xBB;
    ad_ian_data[24] = IBEACON_RSSI;

    ad_ian[1].type = BT_DATA_MANUFACTURER_DATA;
    ad_ian[1].data_len = 25;
    ad_ian[1].data = ad_ian_data;
}

int main(void)
{
    int err;
    printk("Starting iBeacon Rotation + PIR\n");

    pir_dev = DEVICE_DT_GET(PIR_NODE);
    if (!device_is_ready(pir_dev)) {
        printk("GPIO1 not ready\n");
        return 0;
    }

    gpio_pin_configure(pir_dev, PIR_PIN, GPIO_INPUT);

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }

    printk("Bluetooth initialized\n");

    while (1) {

        int pir_value = gpio_pin_get(pir_dev, PIR_PIN);
        printk("PIR: %d\n", pir_value);

        encrypt_ian_uuid(pir_value);

        /* 普通 Beacon */
        bt_le_adv_start(BT_LE_ADV_NCONN, ad_normal, ARRAY_SIZE(ad_normal), NULL, 0);
        k_sleep(K_SECONDS(3));
        bt_le_adv_stop();

        /* 加密 + PIR Beacon */
        bt_le_adv_start(BT_LE_ADV_NCONN, ad_ian, ARRAY_SIZE(ad_ian), NULL, 0);
        k_sleep(K_SECONDS(3));
        bt_le_adv_stop();
    }
}
