import pandas as pd
from tqdm import tqdm

CACHE_SIZE = [1024, 2048, 4096, 8192, 16384, 32768, 65536]
ASSOCIATIVITY = [1, 2, 4, 8, 16, 32]
BLOCK_SIZE = [16, 32, 64, 128, 256, 512]
WRITE_POLICY = ['--WriteBack', '--WriteThrough']
WRITE_MISS_POLICY = ['--WriteAllocate', '--NoWriteAllocate']
EVICT_POLICY = ['--LRU']

if __name__ == "__main__":
    import sys
    from pathlib import Path
    import argparse
    import subprocess

    # Add the parent directory to the system path
    sys.path.append(str(Path(__file__).resolve().parent.parent))

    # Set up argument parser
    parser = argparse.ArgumentParser(description='Cache Simulator')
    parser.add_argument('-f', '--file', type=str, required=True, help='Input trace file')
    parser.add_argument('-s', '--sim', type=str, help='Cache Sim Executable', default='sim/build/CacheSimulator')

    args = parser.parse_args()
    input_file = args.file
    sim_executable = args.sim
    
    # Create empty DataFrame to store results
    columns = ['CacheSize', 'BlockSize', 'Associativity', 'WritePolicy', 'WriteMissPolicy', 
               'EvictPolicy', 'TotalReads', 'ReadMisses', 'TotalWrites', 'WriteMisses', 'TotalLatency']
    results = pd.DataFrame(columns=columns)

    # Calculate total iterations for progress bar
    total_iterations = len(CACHE_SIZE) * len(BLOCK_SIZE) * len(ASSOCIATIVITY) * \
                      len(WRITE_POLICY) * len(WRITE_MISS_POLICY) * len(EVICT_POLICY)

    # Perform grid search with progress bar
    with tqdm(total=total_iterations, desc="Simulating cache configurations") as pbar:
        for cache_size in CACHE_SIZE:
            for block_size in BLOCK_SIZE:
                for assoc in ASSOCIATIVITY:
                    for write_policy in WRITE_POLICY:
                        for write_miss in WRITE_MISS_POLICY:
                            for evict_policy in EVICT_POLICY:
                                # Skip if associativity * block_size > cache_size
                                if assoc * block_size > cache_size:
                                    pbar.update(1)
                                    continue
                                
                                # Construct command
                                cmd = f"{sim_executable} {write_policy} {write_miss} {evict_policy} {cache_size} {block_size} {assoc} {input_file}"
                                
                                try:
                                    # Execute command and capture output
                                    output = subprocess.check_output(cmd, shell=True).decode()
                                    
                                    # Parse output
                                    lines = output.strip().split('\n')
                                    total_reads = int(lines[0].split(': ')[1])
                                    read_misses = int(lines[1].split(': ')[1])
                                    total_writes = int(lines[2].split(': ')[1])
                                    write_misses = int(lines[3].split(': ')[1])
                                    total_latency = int(lines[4].split(': ')[1])

                                    # Add results to DataFrame
                                    results.loc[len(results)] = [cache_size, block_size, assoc, write_policy, 
                                                               write_miss, evict_policy, total_reads, read_misses, 
                                                               total_writes, write_misses, total_latency]
                                except Exception as e:
                                    print(f"Error executing command: {cmd}")
                                    print(f"Error message: {str(e)}")
                                
                                pbar.update(1)

    # Save results to CSV
    output_file = Path(input_file).stem + '.csv'
    results.to_csv(output_file, index=False)
