# Samsung Upload Mode Dumper

This is a tool that can be used to dump the memory of a Samsung device in __upload mode__.

There are a few other tools that can do this, but I couldn't make them work (or they were way too slow), so I decided to write my own.

## 🔎 Acquiring Dependencies

1. Download prebuilt [libusb](https://github.com/libusb/)

    ```bash
    curl -L https://github.com/libusb/libusb/releases/download/v1.0.26/libusb-1.0.26-binaries.7z --output libusb-1.0.26-binaries.7z
    ```

2. Extract the binaries

    ```bash
    7z x libusb-1.0.26-binaries.7z
    ```

3. If you are on Windows you might need to install a driver for your device. You can do that by running `Zadig` and installing the driver for your device: [Zadig](https://zadig.akeo.ie/).

## 💻 Adding your device's USB ID

If your device's USB ID is not in the `c_supported_devs` array in `dumper.h` you will need to add it.

1. Run `lsusb` and find your device's USB ID.

2. Add your device's USB ID to the `c_supported_devs` array in `dumper.h`.

## 🔨 Building

1. Run `build.bat` or `build.sh` depending on your platform.
2. The binary will be at `build/upload_dumper.exe` or `build/upload_dumper`.

## 🧪 Usage

1. Run `upload_dumper` with the following arguments to dump all memory (ranges are determined using the partition table extracted from the device):

    ```bash
    ./upload_dumper dump_all <output_directory>
    ```

2. Run `upload_dumper` with the following arguments to dump a specific partition:

    ```bash
    ./upload_dumper dump_index <output_file> <index>
    ```

3. Run `upload_dumper` with the following arguments to dump a specific range of memory:

    ```bash
    ./upload_dumper dump_range <output_file> <start_address> <end_address>
    ```

Examples:

The following command will dump all memory to the `dump` directory.

```bash
./upload_dumper dump_all ./dump
```

The following command will dump the memory from `0x8F000000` to `0x8F010000` to `dump.bin`.

```bash
./upload_dumper dump_range dump.bin 0x8F000000 0x8F010000
```

This one will dump the partition at index `13` to `dump.bin`.

```bash
./upload_dumper dump_index dump.bin 13
```

## References

There are a few projects that I used as a reference (and to copy some code snippets :) for this project:

1. [alex-segura/s9-sboot-emu](https://github.com/alex-segura/s9-sboot-emu)
2. [nitayart/sboot_dump](https://github.com/nitayart/sboot_dump)
3. [bkerler/sboot_dump](https://github.com/bkerler/sboot_dump/)
