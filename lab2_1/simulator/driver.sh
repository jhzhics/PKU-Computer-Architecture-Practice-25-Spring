#!/usr/bin/bash
IFS=$'\n'

DIR="test/src"

targets=$(find "$DIR" -type f)
targets_count=$(echo "$targets" | wc -l)
success_count=0
echo "Build Simulator..."; make -C sim &> /dev/null
echo "Simulator building finished."
for line in $targets; do
    target=$(echo $line | sed -E "s|.+\/([^\/]+)\.c|\1|")
    echo "Processing: $target"
    output=$(make T=$target 2>&1)
    echo $output | grep "HIT GOOD TRAP" > /dev/null
    if [ $? -eq 0 ]; then
        echo -e "\033[1;32mSuccess\033[0m"
        success_count=$(($success_count + 1))
    else
        echo -e "\033[1;31mFailed\033[0m"
        echo "$output"
    fi
done

printf "Score: %d/%d\n" $success_count $targets_count