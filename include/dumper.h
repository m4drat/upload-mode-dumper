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

#define MAX_PROBE_ENTRIES 0x40
#define MAX_DEVICE_NAME 0x10

typedef struct ProbeTableEntry
{
    uint32_t type;
    char name[20];
    uint64_t start;
    uint64_t end;
} ProbeTableEntry_t;

typedef enum Mode
{
    MODE_32,
    MODE_64,
} Mode_t;

typedef struct ProbeTable
{
    ProbeTableEntry_t entries[MAX_PROBE_ENTRIES];
    char device_name[MAX_DEVICE_NAME];
    uint32_t count;
    Mode_t mode;
} ProbeTable_t;

typedef struct State
{
    ProbeTable_t* probe_table;
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

typedef enum DumpMode
{
    DUMP_MODE_ALL,
    DUMP_MODE_INDEX,
    DUMP_MODE_RANGE,
} DumpMode_t;

typedef struct Options
{
    const char* output_file_name;
    char* output_path;
    DumpMode_t dump_mode;

    union
    {
        struct Range
        {
            uint64_t start_address;
            uint64_t end_address;
        } range;
        uint32_t index;
    };
} Options_t;

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#endif // DUMPER_H