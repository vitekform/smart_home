#!/bin/bash
SDKCONFIG_FILE="sdkconfig"

if [ ! -f "$SDKCONFIG_FILE" ]; then
    echo "Error: sdkconfig file not found in the current directory!"
    exit 1
fi

echo "Configuring $SDKCONFIG_FILE for 4MB flash size and custom partition table..."

# Helper function to set or replace CONFIG options in sdkconfig
set_config_option() {
    local key="$1"
    local value="$2"
    
    # If the key exists (either set or commented out)
    if grep -q "^$key=" "$SDKCONFIG_FILE" || grep -q "^# $key is not set" "$SDKCONFIG_FILE"; then
        # If it's a "not set" comment, replace it
        sed -i "s|^# $key is not set|$key=$value|" "$SDKCONFIG_FILE"
        # If it's already set, update the value
        sed -i "s|^$key=.*|$key=$value|" "$SDKCONFIG_FILE"
    else
        # If it doesn't exist, append it
        echo "$key=$value" >> "$SDKCONFIG_FILE"
    fi
}

comment_config_option() {
    local key="$1"
    if grep -q "^$key=" "$SDKCONFIG_FILE"; then
        sed -i "s|^$key=.*|# $key is not set|" "$SDKCONFIG_FILE"
    fi
}

# 1. Enable custom partition table
comment_config_option "CONFIG_PARTITION_TABLE_SINGLE_APP"
comment_config_option "CONFIG_PARTITION_TABLE_SINGLE_APP_LARGE"
comment_config_option "CONFIG_PARTITION_TABLE_TWO_OTA"
comment_config_option "CONFIG_PARTITION_TABLE_TWO_OTA_LARGE"

set_config_option "CONFIG_PARTITION_TABLE_CUSTOM" "y"
set_config_option "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME" '"partitions.csv"'
set_config_option "CONFIG_PARTITION_TABLE_FILENAME" '"partitions.csv"'

# 2. Change flash size to 4MB
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_1MB"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_2MB"
set_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_4MB" "y"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_8MB"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_16MB"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_32MB"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_64MB"
comment_config_option "CONFIG_ESPTOOLPY_FLASHSIZE_128MB"

set_config_option "CONFIG_ESPTOOLPY_FLASHSIZE" '"4MB"'

echo "Done! sdkconfig updated successfully."
