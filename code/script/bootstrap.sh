#!/bin/bash

file="./.env"

# Changes file if given as flag
for arg in "$@"; do
    case "$arg" in
        --env-file=*)
            file="${arg#*=}"
            ;;
    esac
done


# Checks that the file is present
if [ ! -f "$file" ]; then
    echo ".env does not exist"
    exit 1
fi

# Checks that the file is readable
if [ ! -r "$file" ]; then
    echo "-env is not readable"
    exit 1
fi

var_names_numeric=("NUM_COOKS" "NUM_WAITERS" "MAX_CUSTOMERS" "TOTAL_CUSTOMERS" "GAME_SPEED" "RANDOM_SEED")
var_names_strings=("MENU_FILE" "RESOURCES_FILE")

# Checks that all variables are present
missing=()
for var_name in "${var_names_numeric[@]}" "${var_names_strings[@]}"; do
    flag="false"
    while IFS='=' read -r name value; do
        if [ "$name" = "$var_name" ]; then
            flag="true"
            break
        fi
    done < "$file"
    if [ "$flag" = "false" ]; then
        missing+=("$var_name")
    fi
done

# If missing, prints them and exits
if [ ! ${#missing[@]} -eq 0 ]; then
    echo "Missing env variables in .env:
    ${missing[@]}"
    exit 1
fi

# Checks the validity of numeric variables
while IFS='=' read -r name value; do
    for var_name in "${var_names_numeric[@]}"; do
        if [ "$var_name" = "$name" ]; then
            if ! [[ "$value" =~ ^[0-9]+$ ]]; then
                echo "$name has an invalid value"
                exit 1
            fi
            break
        fi
    done
done < "$file"


# Checks if the file path specified in the env variables exist
while IFS='=' read -r name value; do
    for var_name in "${var_names_strings[@]}"; do
        if [ "$var_name" = "$name" ]; then
            if [[ ! -f "$value" ]]; then
                echo "$name does not contain a valid path"
                exit 1
            fi
            break
        fi
    done
done < "$file"

# Loads variables to the environment
set -a
source "$file"
set +a

# Runs the binary
./restaurant
# Returns the binary exit status
exit $?
