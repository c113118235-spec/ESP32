#include <zephyr/types.h>
#include <stddef.h>
#include <zephyr/sys/printk.h>
#include <zephyr/sys/util.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/kernel.h>

#ifndef IBEACON_RSSI
#define IBEACON_RSSI 0xc8
#endif

/* ---------- 第一組：普通 Beacon (UUID: 1234...) ---------- */
static const struct bt_data ad_normal[] = {
    { .type = BT_DATA_FLAGS,
      .data_len = 1,
      .data = (uint8_t[]){ BT_LE_AD_NO_BREDR } },
    { .type = BT_DATA_MANUFACTURER_DATA,
      .data_len = 25,
      .data = (uint8_t[]){
          0x4c, 0x00, 0x02, 0x15,     // Apple iBeacon header
          0x31, 0x32, 0x33, 0x34,     // UUID "1234"
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,
          0x00, 0x00, 0x00, 0x00,
          0xAA, 0xAA,                 // Major
          0xBB, 0xBB,                 // Minor
          IBEACON_RSSI
      } }
};

/* ---------- 第二組：加密 IAN Beacon ---------- */
/* 原始 UUID: "IAN" + 補 0 */
static uint8_t uuid_ian_plain[16] = {
    0x49, 0x41, 0x4E, 0x00, // 'I','A','N',0
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00
};

/* XOR key (16 bytes) */
static uint8_t key[16] = {
    0xAA, 0xBB, 0xCC, 0xDD,
    0x11, 0x22, 0x33, 0x44,
    0x55, 0x66, 0x77, 0x88,
    0x99, 0x00, 0xFF, 0xEE
};

/* 加密後 UUID */
static uint8_t uuid_ian_enc[16];

/* 廣播資料陣列 */
static struct bt_data ad_ian[2];
static uint8_t ad_ian_data[25];

static void encrypt_ian_uuid(void)
{
    for (int i = 0; i < 16; i++) {
        uuid_ian_enc[i] = uuid_ian_plain[i] ^ key[i];
    }

    /* Flags */
    ad_ian[0].type = BT_DATA_FLAGS;
    ad_ian[0].data_len = 1;
    static uint8_t flags[1] = { BT_LE_AD_NO_BREDR };
    ad_ian[0].data = flags;

    /* Manufacturer data */
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
    printk("Starting iBeacon Rotation Demo\n");

    encrypt_ian_uuid();

    err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    printk("Bluetooth initialized\n");

    while (1) {
        /* 廣播普通 1234 */
        err = bt_le_adv_start(BT_LE_ADV_NCONN, ad_normal, ARRAY_SIZE(ad_normal), NULL, 0);
        if (!err) {
            printk(">>> Advertising: UUID = 1234\n");
        }
        k_sleep(K_SECONDS(3));
        bt_le_adv_stop();

        /* 廣播加密 IAN */
        err = bt_le_adv_start(BT_LE_ADV_NCONN, ad_ian, ARRAY_SIZE(ad_ian), NULL, 0);
        if (!err) {
            printk(">>> Advertising: UUID = Encrypted IAN\n");
        }
        k_sleep(K_SECONDS(3));
        bt_le_adv_stop();
    }
}
