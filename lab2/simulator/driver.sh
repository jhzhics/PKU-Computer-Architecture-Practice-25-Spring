#!/usr/bin/bash
IFS=$'\n'
DIR="test/src"
packages=()
perf_mode=""
filter_regex=""

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --perf)
            [[ -z "$2" ]] && { echo "Missing argument for --perf"; exit 1; }
            perf_mode="$2"
            shift 2
            ;;
        -E)
            [[ -z "$2" ]] && { echo "Missing argument for -E"; exit 1; }
            filter_regex="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

# Validate performance mode
if [[ -n "$perf_mode" && ! "$perf_mode" =~ ^(multicycle|pipeline)$ ]]; then
    echo "Invalid perf mode: $perf_mode. Use multicycle or pipeline"
    exit 1
fi

# Check Prerequisites
echo "Checking Dependencies..."
dpkg -l libcapstone-dev &> /dev/null
for pkg in "${packages[@]}"; do
    dpkg -l "$pkg" &> /dev/null || {
        echo "Failed: Package '$pkg' not installed. Run install_dependencies.sh first."
        exit 1
    }
done

# Running Tests
targets=$(find "$DIR" -type f)
targets_count=$(echo "$targets" | wc -l)
success_count=0

# Build simulator with performance mode
build_cmd="make -C sim"
[[ -n "$perf_mode" ]] && build_cmd+=" PERF=$perf_mode"
echo "Build Simulator..."; eval $build_cmd &> /dev/null
echo "Simulator building finished."

processed_count=0
for line in $targets; do
    target=$(echo "$line" | sed -E 's|.+/||; s/\.c$//')
    
    # Apply regex filter
    if [[ -n "$filter_regex" ]] && [[ ! "$target" =~ $filter_regex ]]; then
        continue
    fi
    ((processed_count++))
    
    echo "Processing: $target"
    
    # Build test command
    make_cmd="make T=$target"
    [[ -n "$perf_mode" ]] && make_cmd+=" PERF=$perf_mode"
    
    output=$(eval $make_cmd 2>&1)
    echo "$output" | grep "HIT GOOD TRAP" > /dev/null
    
    if [ $? -eq 0 ]; then
        echo -e "\033[1;32mSuccess\033[0m"
        success_count=$((success_count + 1))
        
        # Show immediate performance metrics
        if [[ -n "$perf_mode" ]]; then
            echo -e "\033[1;34mPerformance Metrics:\033[0m"
            echo "$output" | awk '
                /Performance Profiler:/ {printf "  %s\n", $0}
                /Dynamic instructions:/ {printf "  %s\n", $0}
                /Dynamic cycles:/ {printf "  %s\n", $0}
                /CPI:/ {printf "  %s\n", $0}
                /Control Hazard Stall Cycles:/ {printf "  %s\n", $0}
                /Data Hazard Stall Cycles:/ {printf "  %s\n\n", $0}'
        fi

    else
        echo -e "\033[1;31mFailed\033[0m"
        echo "$output"
    fi
done

# Final report
printf "\nScore: %d/%d" $success_count $processed_count
[[ -n "$filter_regex" ]] && printf " (filtered: '%s')" "$filter_regex"
printf "\n"