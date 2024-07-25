#define _CRT_SECURE_NO_WARNINGS

#include "dumper.h"
#include "hexdump.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

int32_t fill_probetable(ProbeTable_t* probetable);

int create_directory(const char* name)
{
#ifdef __linux__
    return mkdir(name, 777);
#else
    return _mkdir(name);
#endif
}

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

    // Receive probe table
    state->probe_table = malloc(sizeof(ProbeTable_t));
    state->probe_table->count = 0;

    return fill_probetable(g_usb_state_ptr->probe_table);
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

    if (state->probe_table)
        free(state->probe_table);
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

int32_t parse_one(const Mode_t mode, const uint8_t* curr_ptr, ProbeTableEntry_t* entry)
{
    entry->type = *(uint32_t*)curr_ptr;
    curr_ptr += sizeof(uint32_t);

    strncpy(entry->name, (const char*)curr_ptr, sizeof(entry->name));
    const uint32_t name_len = (mode == MODE_64) ? 20 : 16;
    curr_ptr += name_len;

    if (mode == MODE_32) {
        entry->start = *(uint32_t*)curr_ptr;
        curr_ptr += sizeof(uint32_t);

        entry->end = *(uint32_t*)curr_ptr;
        curr_ptr += sizeof(uint32_t);
    } else {
        entry->start = *(uint64_t*)curr_ptr;
        curr_ptr += sizeof(uint64_t);

        entry->end = *(uint64_t*)curr_ptr;
        curr_ptr += sizeof(uint64_t);
    }

    return 0;
}

int32_t fill_probetable_parse(const uint8_t* curr_data_ptr, ProbeTable_t* probetable)
{
    const Mode_t mode = (*curr_data_ptr == '+') ? MODE_64 : MODE_32;
    probetable->mode = mode;

    const uint32_t entry_size = (mode == MODE_64) ? 40 : 28;

    strncpy(
        probetable->device_name, (const char*)curr_data_ptr + 1, sizeof(probetable->device_name));
    curr_data_ptr += MAX_DEVICE_NAME;

    ProbeTableEntry_t* curr_entry_ptr = &probetable->entries[0];
    while (probetable->count < MAX_PROBE_ENTRIES) {
        if (parse_one(mode, curr_data_ptr, curr_entry_ptr) < 0) {
            printf("Failed to parse entry\n");
            return -1;
        }

        if (curr_entry_ptr->start == 0 && curr_entry_ptr->end == 0)
            break;

        curr_data_ptr += entry_size;
        curr_entry_ptr++;
        probetable->count++;
    }

    return 0;
}

int32_t fill_probetable(ProbeTable_t* probetable)
{
    uint8_t send_buf[1024] = { 0 };
    // 1. Send preamble packet
    memset(send_buf, 0, sizeof(send_buf));
    memcpy(send_buf, c_preamble, sizeof(c_preamble));
    if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
        printf("Failed to send preamble packet (fill_probetable)\n");
        return -1;
    }

    if (receive_ack(g_usb_state_ptr,
                    "Failed to receive ack for preamble packet (fill_probetable)") < 0) {
        return -1;
    }

    memset(send_buf, 0, sizeof(send_buf));
    memcpy(send_buf, c_probe, sizeof(c_probe));
    if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
        printf("Failed to send probe packet\n");
        return -1;
    }

    uint8_t recv_buf[BLOCK_SIZE] = { 0 };
    if (receive_packet(g_usb_state_ptr, recv_buf, sizeof(recv_buf)) < 0) {
        printf("Failed to receive probetable packet\n");
        return -1;
    }

    // hexdump(recv_buf, sizeof(recv_buf), 0);

    return fill_probetable_parse(recv_buf, probetable);
}

void print_probetable(const ProbeTable_t* probetable)
{
    if (!probetable) {
        printf("Probe table is NULL\n");
        return;
    }

    if (!probetable->count) {
        printf("Probe table is empty\n");
        return;
    }

    printf("Probe table:\n");
    for (uint32_t i = 0; i < probetable->count; i++) {
        printf("Entry %d: %s [0x%llX, 0x%llX]\n",
               i,
               probetable->entries[i].name,
               probetable->entries[i].start,
               probetable->entries[i].end);
    }
}

int32_t dump_memory_range_to_file(const char* output_path,
                                  const uint64_t start_address,
                                  const uint64_t end_address)
{
    uint8_t send_buf[1024] = { 0 };
    FILE* output_file = fopen(output_path, "wb+");

    for (uint64_t addr_low = start_address; addr_low < end_address; addr_low += BLOCK_SIZE) {
        // Print progress once in a while
        if ((addr_low - start_address) % (BLOCK_SIZE * 0x30) == 0)
            printf("%s : %f%% complete\n",
                   output_path,
                   (double)(addr_low - start_address) / (end_address - start_address) * 100);

        // 1. Send preamble packet
        memset(send_buf, 0, sizeof(send_buf));
        memcpy(send_buf, c_preamble, sizeof(c_preamble));
        if (send_packet(g_usb_state_ptr, send_buf, sizeof(send_buf)) < 0) {
            printf("Failed to send preamble packet\n");
            return -1;
        }

        if (receive_ack(g_usb_state_ptr,
                        "Failed to receive ack for preamble packet (dump_memory)") < 0) {
            return -1;
        }

        const uint64_t high_addr = (uint64_t)(min(addr_low + BLOCK_SIZE, end_address));
        // printf("Dumping block: [0x%llX, 0x%llX)\n", addr_low, high_addr);

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
        // printf("Receiving %llu bytes\n", recv_size);
        if (receive_packet(g_usb_state_ptr, recv_buf, recv_size + 1) < 0) {
            printf("Failed to receive data packet\n");
            return -1;
        }

        fwrite(recv_buf, 1, recv_size, output_file);
        fflush(output_file);
    }

    fclose(output_file);

    return 0;
}

int32_t dump_memory(const Options_t* options)
{
    switch (options->dump_mode) {
        case DUMP_MODE_ALL:
            // create directory if it doesn't exist
            if (create_directory(options->output_path) < 0) {
                printf("Failed to create directory\n");
                return -1;
            }

            for (uint32_t i = 0; i < g_usb_state_ptr->probe_table->count; i++) {
                const uint64_t start_address = g_usb_state_ptr->probe_table->entries[i].start;
                // +1 to include the end address
                const uint64_t end_address = g_usb_state_ptr->probe_table->entries[i].end;

                if (start_address == 0 && end_address == 0) {
                    printf("Invalid index\n");
                    return -1;
                }

                char output_path[0x200] = { 0 };
                snprintf(output_path,
                         sizeof(output_path),
                         "%s/%s-%d.bin",
                         options->output_path,
                         g_usb_state_ptr->probe_table->entries[i].name,
                         i);

                printf("Saving %s [0x%llx, 0x%llx] to %s\n",
                       g_usb_state_ptr->probe_table->entries[i].name,
                       start_address,
                       end_address,
                       output_path);

                if (dump_memory_range_to_file(output_path, start_address, end_address) < 0)
                    return -1;
            }
        case DUMP_MODE_INDEX:
            const uint64_t start_address =
                g_usb_state_ptr->probe_table->entries[options->index].start;
            const uint64_t end_address = g_usb_state_ptr->probe_table->entries[options->index].end;

            if (start_address == 0 && end_address == 0) {
                printf("Invalid index\n");
                return -1;
            }

            return dump_memory_range_to_file(options->output_path, start_address, end_address);
        case DUMP_MODE_RANGE:
            return dump_memory_range_to_file(
                options->output_path, options->range.start_address, options->range.end_address);
        default:
            printf("Invalid dump mode\n");
            return -1;
    }
}

Options_t parse_options(int argc, char* argv[])
{
    Options_t options = { 0 };
    const char hex_prefix[] = "0x";

    if (!strcmp(argv[1], "dump_all")) {
        if (argc != 3) {
            printf("Usage: %s dump_all <output_directory>\n", argv[0]);
            exit(-1);
        }

        options.dump_mode = DUMP_MODE_ALL;
        options.output_path = argv[2];
    } else if (!strcmp(argv[1], "dump_index")) {
        if (argc != 4) {
            printf("Usage: %s dump_index <output_file> <index>\n", argv[0]);
            exit(-1);
        }

        options.dump_mode = DUMP_MODE_INDEX;
        options.output_path = argv[2];
        options.index = atoi(argv[3]);
    } else if (!strcmp(argv[1], "dump_range")) {
        if (argc != 5) {
            printf("Usage: %s dump_range <output_file> <start_address> <end_address>\n", argv[0]);
            exit(-1);
        }

        options.dump_mode = DUMP_MODE_RANGE;
        options.output_path = argv[2];

        if (strlen(argv[3]) > 2) {
            options.range.start_address =
                strtoull(!strcmp(hex_prefix, argv[3]) ? argv[3] + 2 : argv[3], NULL, 16);
        }
        if (strlen(argv[4]) > 2) {
            // +1 to include the end address
            options.range.end_address =
                strtoull(!strcmp(hex_prefix, argv[4]) ? argv[4] + 2 : argv[4], NULL, 16) + 1;
        }

        if (!options.range.end_address || options.range.start_address > options.range.end_address) {
            printf("Invalid address range: 0x%llX, 0x%llX\n",
                   options.range.start_address,
                   options.range.end_address);
            exit(-1);
        }
    } else {
        printf("Invalid dump mode\n");
        exit(-1);
    }

    return options;
}

int main(int argc, char* argv[])
{
    if (argc < 3) {
        printf("Usage: %s dump_all <output_directory>\n", argv[0]);
        printf("Usage: %s dump_index <output_file> <index>\n", argv[0]);
        printf("Usage: %s dump_range <output_file> <start_address> <end_address>\n", argv[0]);
        return -1;
    }

    Options_t options = parse_options(argc, argv);
    if (options.dump_mode == DUMP_MODE_RANGE) {
        printf("Dumping a total of %llu (0x%llx) bytes from 0x%llX to 0x%llX\n",
               options.range.end_address - options.range.start_address,
               options.range.end_address - options.range.start_address,
               options.range.start_address,
               options.range.end_address);
    }

    if (init_device(g_usb_state_ptr) < 0)
        return -1;

    print_probetable(g_usb_state_ptr->probe_table);

    if (dump_memory(&options) < 0)
        return -1;

    printf("Dumped memory to %s\n", options.output_file_name);

    close_state(g_usb_state_ptr);

    return 0;
}