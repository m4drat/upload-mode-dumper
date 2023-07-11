#ifndef DUMPER_H
#define DUMPER_H

#include <libusb-1.0/libusb.h>
#include <stdio.h>

#define DEBUG_PRINT 0
#define BLOCK_SIZE 0x40000
#define ACK_PACKET_SIZE 0x400
#define USB_CLASS_CDC_DATA 0x0A

typedef struct Device
{
    int vendor_id;
    int product_id;
} Device_t;

// Supported devices, add your own here
const Device_t c_supported_devs[] = { { 0x04e8, 0x6601 },
                                      { 0x04e8, 0x685d },
                                      { 0x04e8, 0x68c3 },
                                      { 0x04e8, 0x6860 } };

const char c_preamble[] = "PrEaMbLe\0";
const char c_acknowledgment[] = "AcKnOwLeDgMeNt\0";
const char c_postamble[] = "PoStAmBlE\0";
const char c_powerdown[] = "PoWeRdOwN\0";
const char c_dataxfer[] = "DaTaXfEr\0";
const char c_probe[] = "PrObE\0";

typedef struct State
{
    libusb_context* ctx;
    libusb_device* device;
    libusb_device_handle* handle;
    int interface_index;
    int alt_setting_index;
    int in_endpoint;
    int out_endpoint;
    int interface_claimed;
} State_t;

State_t g_usb_state;
State_t* g_usb_state_ptr = &g_usb_state;

typedef struct Options
{
    const char* output_file_name;
    FILE* output_file;
    uint64_t start_address;
    uint64_t end_address;
    uint32_t print_hexdump;
} Options_t;

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#endif // DUMPER_H