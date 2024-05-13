// ble_cus.cpp

#include "ble_cus.h"
#include <cstring>

uint32_t ble_cus_init(ble_cus_t *p_cus, const ble_cus_init_t *p_cus_init) {
  // Initialize the ble_cus_t structure
  p_cus->service_handle = 0;
  p_cus->conn_handle = 0;
  p_cus->pCharSteerer = nullptr;
  p_cus->pCharRX = nullptr;
  p_cus->pCharTX = nullptr;

  // Set the event handler if provided
  if (p_cus_init->evt_handler != nullptr) {
    // Set the event handler in the ble_cus_t structure
    // You can store the event handler pointer or implement your own mechanism
    // to handle events based on your specific requirements
  }

  return 0; // Return success
}

uint32_t ble_cus_steering_value_update(ble_cus_t *p_cus, float angle) {
  if (p_cus->pCharSteerer != nullptr) {
    // Update the steering angle characteristic value
    uint8_t value[4];
    memcpy(value, &angle, sizeof(float));
    p_cus->pCharSteerer->setValue(value, sizeof(float));
    p_cus->pCharSteerer->notify();
    return 0; // Return success
  }
  return 1; // Return error
}