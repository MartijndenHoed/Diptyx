Import("env")
merge_cmd = (
    '"$PYTHONEXE" "$PROJECT_PACKAGES_DIR/tool-esptoolpy/esptool.py" '
    '--chip esp32s3 merge_bin -o "$BUILD_DIR/merged.bin" '
    '--flash_mode dio --flash_freq 80m --flash_size 16MB '
    '0x0 "$BUILD_DIR/bootloader.bin" '
    '0x8000 "$BUILD_DIR/partitions.bin" '
    '0x10000 "$BUILD_DIR/${PROGNAME}.bin" '
    '0x910000 "$BUILD_DIR/littlefs.bin"'
)
env.AddPostAction(
    "$BUILD_DIR/${PROGNAME}.bin",
    env.VerboseAction(merge_cmd, "Merging binaries into merged.bin")
)