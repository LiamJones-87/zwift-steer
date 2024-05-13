#ifndef BLE_CUS_H__
#define BLE_CUS_H__

#include <stdint.h>
#include <BLECharacteristic.h>

typedef enum {
    BLE_CUS_EVT_NOTIFICATION_ENABLED,
    BLE_CUS_EVT_NOTIFICATION_DISABLED,
    BLE_CUS_EVT_CONNECTED,
    BLE_CUS_EVT_DISCONNECTED
} ble_cus_evt_type_t;

typedef struct {
    ble_cus_evt_type_t evt_type;
} ble_cus_evt_t;

typedef struct ble_cus_s {
    uint16_t service_handle;
    uint16_t conn_handle;
    BLECharacteristic *pCharSteerer;
    BLECharacteristic *pCharRX;
    BLECharacteristic *pCharTX;
} ble_cus_t;

typedef struct {
    void (*evt_handler)(ble_cus_evt_t *p_evt);
} ble_cus_init_t;

uint32_t ble_cus_init(ble_cus_t *p_cus, const ble_cus_init_t *p_cus_init);
uint32_t ble_cus_steering_value_update(ble_cus_t *p_cus, float angle);

#endif // BLE_CUS_H__