#include <hidapi.h>

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SONY_VENDOR_ID 0x054c
#define DS4_V1_PRODUCT_ID 0x05c4
#define DS4_V2_PRODUCT_ID 0x09cc
#define GET_PAIRING_REPORT_ID 0x12
#define GET_PAIRING_REPORT_SIZE 16
#define SET_PAIRING_REPORT_ID 0x13
#define SET_PAIRING_REPORT_SIZE 23

static int parse_mac(const char *text, uint8_t mac[6]) {
  unsigned int bytes[6];
  char extra;
  if (strlen(text) != 17 ||
      sscanf(text, "%2x:%2x:%2x:%2x:%2x:%2x%c",
             &bytes[0], &bytes[1], &bytes[2],
             &bytes[3], &bytes[4], &bytes[5], &extra) != 6) {
    return 0;
  }

  for (size_t i = 0; i < 6; i++) {
    if (bytes[i] > 0xff) {
      return 0;
    }
    mac[i] = (uint8_t)bytes[i];
  }

  return (mac[0] & 1) == 0;
}

static hid_device *open_ds4(void) {
  const unsigned short product_ids[] = {
    DS4_V1_PRODUCT_ID,
    DS4_V2_PRODUCT_ID
  };

  for (size_t i = 0; i < sizeof(product_ids) / sizeof(product_ids[0]); i++) {
    hid_device *device = hid_open(SONY_VENDOR_ID, product_ids[i], NULL);
    if (device != NULL) {
      return device;
    }
  }

  return NULL;
}

static int read_pairing_report(hid_device *device,
                               unsigned char report[GET_PAIRING_REPORT_SIZE]) {
  memset(report, 0, GET_PAIRING_REPORT_SIZE);
  report[0] = GET_PAIRING_REPORT_ID;
  int result = hid_get_feature_report(device, report, GET_PAIRING_REPORT_SIZE);
  if (result < GET_PAIRING_REPORT_SIZE) {
    const wchar_t *error = hid_error(device);
    fprintf(stderr, "Could not read DS4 pairing report");
    if (error != NULL) {
      fprintf(stderr, ": %ls", error);
    }
    fputc('\n', stderr);
    return 0;
  }

  return 1;
}

static void print_mac(const char *label, const unsigned char *reversed_bytes) {
  printf("%s%02X:%02X:%02X:%02X:%02X:%02X\n",
         label,
         reversed_bytes[5], reversed_bytes[4], reversed_bytes[3],
         reversed_bytes[2], reversed_bytes[1], reversed_bytes[0]);
}

int main(int argc, char **argv) {
  uint8_t target_mac[6];
  int write_requested = argc == 3 && strcmp(argv[1], "--write") == 0;

  if (argc != 1 && !write_requested) {
    fprintf(stderr, "Usage: %s [--write ESP32_BLUETOOTH_MAC]\n", argv[0]);
    return 2;
  }

  if (write_requested && !parse_mac(argv[2], target_mac)) {
    fprintf(stderr, "Invalid unicast MAC address: %s\n", argv[2]);
    return 2;
  }

  if (hid_init() != 0) {
    fprintf(stderr, "HIDAPI initialization failed.\n");
    return 1;
  }

  hid_device *device = open_ds4();
  if (device == NULL) {
    fprintf(stderr,
            "No wired DualShock 4 found (expected Sony USB ID 054c:05c4 or 054c:09cc).\n"
            "Connect it with a data-capable USB cable and close other controller apps.\n");
    hid_exit();
    return 1;
  }

  unsigned char report[GET_PAIRING_REPORT_SIZE];
  if (!read_pairing_report(device, report)) {
    hid_close(device);
    hid_exit();
    return 1;
  }

  print_mac("Controller Bluetooth MAC: ", &report[1]);
  print_mac("Current paired host MAC:   ", &report[10]);

  if (write_requested) {
    // The DS4 write report carries a 16-byte link key after the host MAC.
    unsigned char set_report[SET_PAIRING_REPORT_SIZE] = {0};
    set_report[0] = SET_PAIRING_REPORT_ID;
    for (size_t i = 0; i < 6; i++) {
      set_report[1 + i] = target_mac[5 - i];
    }

    int result = hid_send_feature_report(
      device, set_report, SET_PAIRING_REPORT_SIZE
    );
    if (result < SET_PAIRING_REPORT_SIZE) {
      const wchar_t *error = hid_error(device);
      fprintf(stderr, "Could not write DS4 pairing report");
      if (error != NULL) {
        fprintf(stderr, ": %ls", error);
      }
      fputc('\n', stderr);
      hid_close(device);
      hid_exit();
      return 1;
    }

    if (!read_pairing_report(device, report)) {
      hid_close(device);
      hid_exit();
      return 1;
    }

    for (size_t i = 0; i < 6; i++) {
      if (report[10 + i] != target_mac[5 - i]) {
        fprintf(stderr, "Readback does not match the requested host MAC.\n");
        hid_close(device);
        hid_exit();
        return 1;
      }
    }

    print_mac("Verified paired host MAC:   ", &report[10]);
  }

  hid_close(device);
  hid_exit();
  return 0;
}
