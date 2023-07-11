#define _CRT_SECURE_NO_WARNINGS

#include "dumper.h"
#include "hexdump.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int init_device(State_t* state)
{
    libusb_device** devices;
    struct libusb_device_descriptor desc;
    struct libusb_config_descriptor* config;

    int result = libusb_init(&state->ctx);
    if (result != LIBUSB_SUCCESS) {
        printf("Failed to initialize libusb. libusb error: %d\n", result);
        return -1;
    }

    ssize_t total_devices = libusb_get_device_list(state->ctx, &devices);
    if (total_devices < 0) {
        printf("Failed to retrieve device list\n");
        return -1;
    }

    for (uint32_t i = 0; i < total_devices; i++) {
        struct libusb_device_descriptor device_desc;
        if (libusb_get_device_descriptor(devices[i], &device_desc) != LIBUSB_SUCCESS) {
            printf("Failed to retrieve device descriptor for device %d\n", i);
            continue;
        }

        for (uint32_t j = 0; j < sizeof(c_supported_devs); j++) {
            if (device_desc.idVendor != c_supported_devs[j].vendor_id)
                continue;
            if (device_desc.idProduct != c_supported_devs[j].product_id)
                continue;

            state->device = devices[i];
            libusb_ref_device(state->device);
            break;
        }
        if (state->device)
            break;
    }

    libusb_free_device_list(devices, (int)total_devices);

    if (!state->device) {
        printf("Device detection failed\n");
        return -1;
    }

    result = libusb_open(state->device, &state->handle);
    if (result != LIBUSB_SUCCESS) {
        printf("Failed to access device: %s\n", libusb_strerror(result));
        return -1;
    }

    result = libusb_get_device_descriptor(state->device, &desc);
    if (result != LIBUSB_SUCCESS) {
        printf("Failed to retrieve device descriptor: %s\n", libusb_strerror(result));
        return -1;
    }

    printf("Found device %04x:%04x\n", desc.idVendor, desc.idProduct);

    result = libusb_get_config_descriptor(state->device, 0, &config);
    if (result != LIBUSB_SUCCESS || !config) {
        printf("Failed to retrieve config descriptor: %s\n", libusb_strerror(result));
        return -1;
    }

    state->interface_index = -1;
    state->alt_setting_index = -1;
    for (uint32_t iface_idx = 0; iface_idx < config->bNumInterfaces; iface_idx++) {
        for (uint32_t altsetting_idx = 0;
             altsetting_idx < (uint32_t)config->interface[iface_idx].num_altsetting;
             altsetting_idx++) {
            int in_endpoint_addr = -1;
            int out_endpoint_addr = -1;
            for (uint32_t endpoint_idx = 0;
                 endpoint_idx <
                 config->interface[iface_idx].altsetting[altsetting_idx].bNumEndpoints;
                 endpoint_idx++) {
                const struct libusb_endpoint_descriptor* endpoint;
                endpoint =
                    &config->interface[iface_idx].altsetting[altsetting_idx].endpoint[endpoint_idx];
                if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    in_endpoint_addr = endpoint->bEndpointAddress;
                else
                    out_endpoint_addr = endpoint->bEndpointAddress;
            }
            if (state->interface_index >= 0)
                continue;
            if (config->interface[iface_idx].altsetting[altsetting_idx].bNumEndpoints != 2)
                continue;
            if (config->interface[iface_idx].altsetting[altsetting_idx].bInterfaceClass !=
                USB_CLASS_CDC_DATA)
                continue;
            if (in_endpoint_addr == -1)
                continue;
            if (out_endpoint_addr == -1)
                continue;
            state->interface_index = iface_idx;
            state->alt_setting_index = altsetting_idx;
            state->in_endpoint = in_endpoint_addr;
            state->out_endpoint = out_endpoint_addr;
        }
    }

    libusb_free_config_descriptor(config);

    if (state->interface_index < 0) {
        printf("Failed to find correct interface configuration\n");
        return -1;
    }

    printf("Claiming interface...\n");
    result = libusb_claim_interface(state->handle, state->interface_index);
    if (result != LIBUSB_SUCCESS) {
        printf("Claiming interface failed: %s\n", libusb_strerror(result));
        return -1;
    }
    state->interface_claimed = 1;

    printf("Setting up interface...\n");
    result = libusb_set_interface_alt_setting(
        state->handle, state->interface_index, state->alt_setting_index);
    if (result != LIBUSB_SUCCESS) {
        printf("Setting up interface failed: %s\n", libusb_strerror(result));
        return -1;
    }

    return 0;
}

void close_state(State_t* state)
{
    if (state->interface_claimed && state->handle) {
        libusb_release_interface(state->handle, state->interface_index);
        state->interface_claimed = 0;
    }

    if (state->handle)
        libusb_close(state->handle);

    if (state->device)
        libusb_unref_device(state->device);

    if (state->ctx)
        libusb_exit(state->ctx);
}

int32_t send_packet(State_t* state, uint8_t* packet, uint32_t packet_size)
{
    int32_t result;
    int32_t transferred;

#if DEBUG_PRINT == 1
    printf("[send_packet]\n");
    hexdump(packet, packet_size, 0);
#endif

    result = libusb_bulk_transfer(
        state->handle, state->out_endpoint, packet, packet_size, &transferred, 1000);
    if (result != LIBUSB_SUCCESS) {
        printf("Failed to send packet: %s\n", libusb_strerror(result));
        return -1;
    }

    return 0;
}

int32_t receive_packet(State_t* state, uint8_t* packet, uint32_t packet_size)
{
    int32_t result;
    int32_t transferred;

    result = libusb_bulk_transfer(
        state->handle, state->in_endpoint, packet, packet_size, &transferred, 1000);
    if (result != LIBUSB_SUCCESS) {
        printf("Failed to receive packet: %s\n", libusb_strerror(result));
        return -1;
    }

#if DEBUG_PRINT == 1
    printf("[receive_packet]\n");
    hexdump(packet, packet_size, 0);
#endif

    return 0;
}

int32_t receive_ack(State_t* state, char* message)
{
    uint8_t recv_buf[ACK_PACKET_SIZE] = { 0 };

    if (receive_packet(state, recv_buf, ACK_PACKET_SIZE) < 0 ||
        memcmp(recv_buf, c_acknowledgment, sizeof(c_acknowledgment)) != 0) {
        printf("%s\n", message);
        return -1;
    }

    return 0;
}

int32_t dump_memory(Options_t* options)
{
    uint8_t send_buf[1024] = { 0 };

    for (uint64_t addr_low = options->start_address; addr_low < options->end_address;
         addr_low += BLOCK_SIZE) {
        // 1. Send preamble packet
        memset(send_buf, 0, sizeof(send_buf));
        memcpy(send_buf, c_preamble, sizeof(c_preamble));
        if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
            printf("Failed to send preamble packet\n");
            return -1;
        }

        if (receive_ack(g_usb_state_ptr, "Failed to receive ack for preamble packet") < 0) {
            return -1;
        }

        const uint64_t high_addr = (uint64_t)(min(addr_low + BLOCK_SIZE, options->end_address));
        printf("Dumping block: [0x%llX, 0x%llX)\n", addr_low, high_addr);

        // 2. Send low address
        memset(send_buf, 0, sizeof(send_buf));
        sprintf((char*)send_buf, "%09llX", addr_low);
        if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
            printf("Failed to send low address packet\n");
            return -1;
        }

        if (receive_ack(g_usb_state_ptr, "Failed to receive ack for low address packet") < 0) {
            return -1;
        }

        // 3. Send high address
        memset(send_buf, 0, sizeof(send_buf));
        sprintf((char*)send_buf, "%09llX", high_addr);
        if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
            printf("Failed to send high address packet\n");
            return -1;
        }

        if (receive_ack(g_usb_state_ptr, "Failed to receive ack for high address packet") < 0) {
            return -1;
        }

        // 4. Receive data
        uint8_t recv_buf[BLOCK_SIZE + 0x10] = { 0 };

        memset(send_buf, 0, sizeof(send_buf));
        memcpy(send_buf, c_dataxfer, sizeof(c_dataxfer));
        if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
            printf("Failed to send data xfer packet\n");
            return -1;
        }

        const uint64_t recv_size = high_addr - addr_low;
        printf("Receiving %llu bytes\n", recv_size);
        if (receive_packet(g_usb_state_ptr, recv_buf, recv_size + 1) < 0) {
            printf("Failed to receive data packet\n");
            return -1;
        }

        if (options->print_hexdump) {
            hexdump(recv_buf, recv_size, addr_low);
        }

        fwrite(recv_buf, 1, recv_size, options->output_file);
        fflush(options->output_file);
    }

    fclose(options->output_file);

    return 0;
}

Options_t parse_options(int argc, char* argv[])
{
    Options_t options = { 0 };
    const char hex_prefix[] = "0x";

    options.output_file_name = argv[1];
    options.output_file = fopen(options.output_file_name, "wb+");
    if (strlen(argv[2]) > 2) {
        options.start_address =
            strtoull(!strcmp(hex_prefix, argv[2]) ? argv[2] + 2 : argv[2], NULL, 16);
    }
    if (strlen(argv[3]) > 2) {
        options.end_address =
            strtoull(!strcmp(hex_prefix, argv[3]) ? argv[3] + 2 : argv[3], NULL, 16);
    }
    options.print_hexdump = (argc > 4) ? !strcmp(argv[4], "print_hexdump") : 0;

    return options;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: %s <output_file> <start_address> <end_address> <print_hexdump>\n", argv[0]);
        return -1;
    }

    Options_t options = parse_options(argc, argv);

    if (!options.start_address || !options.end_address ||
        options.start_address > options.end_address) {
        printf(
            "Invalid address range: 0x%llX, 0x%llX\n", options.start_address, options.end_address);
        return -1;
    }

    printf("Dumping a total of %llu (0x%llx) bytes from 0x%llX to 0x%llX\n",
           options.end_address - options.start_address,
           options.end_address - options.start_address,
           options.start_address,
           options.end_address);

    if (init_device(g_usb_state_ptr) < 0)
        return -1;

    if (dump_memory(&options) < 0)
        return -1;

    printf("Dumped memory to %s\n", options.output_file_name);

    close_state(g_usb_state_ptr);

    return 0;
}