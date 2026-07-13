#!/bin/bash

file="./.env"

# Changes env file or env variable if given through a flag
for arg in "$@"; do
    case "$arg" in
        --env-file=*)
            file="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --num-cooks=*)
            num_cooks="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --num-waiters=*)
            num_waiters="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --max-customers=*)
            max_customers="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --total-customers=*)
            total_customers="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --menu-file=*)
            menu_file="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --resources-file=*)
            resources_file="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --game-speed=*)
            game_speed="${arg#*=}"
            ;;
    esac
    case "$arg" in
        --random-seed=*)
            random_seed="${arg#*=}"
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


# Checks if the file paths specified in the env variables exist
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

# Loads custom variables, if present as flags
if [[ -v num_cooks ]]; then
    if [[ "$num_cooks" =~ ^[0-9]+$ ]]; then
        declare "NUM_COOKS=$num_cooks"
    else
        echo "Custom num_cooks not a valid number, ignoring it"
    fi
fi
if [[ -v num_waiters ]]; then
    if [[ "$num_waiters" =~ ^[0-9]+$ ]]; then
        declare "NUM_WAITERS=$num_waiters"
    else
        echo "Custom num_waiters not a valid number, ignoring it"
    fi
fi
if [[ -v max_customers ]]; then
    if [[ "$max_customers" =~ ^[0-9]+$ ]]; then
        declare "MAX_CUSTOMERS=$max_customers"
    else
        echo "Custom max_customers not a valid number, ignoring it"
    fi
fi
if [[ -v total_customers ]]; then
    if [[ "$total_customers" =~ ^[0-9]+$ ]]; then
        declare "TOTAL_CUSTOMERS=$total_customers"
    else
        echo "Custom total_customers not a valid number, ignoring it"
    fi
fi
if [[ -v menu_file ]]; then
    if [[ -f "$menu_file" ]]; then
        declare "MENU_FILE=$menu_file"
    else
        echo "Invalid custom value on menu_file, ignoring it"
    fi
fi
if [[ -v resources_file ]]; then
    if [[ -f "$resources_file" ]]; then
        declare "RESOURCES_FILE=$resources_file"
    else
        echo "Invalid custom value on resources_file, ignoring it"
    fi
fi
if [[ -v game_speed ]]; then
    if [[ "$game_speed" =~ ^[0-9]+$ ]]; then
        declare "GAME_SPEED=$game_speed"
    else
        echo "Custom game_speed not a valid number, ignoring it"
    fi
fi
if [[ -v random_seed ]]; then
    if [[ "$random_seed" =~ ^[0-9]+$ ]]; then
        declare "RANDOM_SEED=$random_seed"
    else
        echo "Custom random_seed not a valid number, ignoring it"
    fi
fi


# Runs the binary
./restaurant
# Returns the binary exit status
exit $?
