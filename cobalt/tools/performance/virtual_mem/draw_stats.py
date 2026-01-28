import matplotlib.pyplot as plt

def plot_memory_stats(input_file, output_image):
    seconds = []
    vsize_list = []
    rss_list = []
    
    # Standard Android page size is usually 4KB
    PAGE_SIZE = 4096 

    print(f"Reading {input_file}...")
    
    with open(input_file, 'r') as f:
        for i, line in enumerate(f):
            line = line.strip()
            if not line: continue
            
            # Handle potential spaces in process name: "123 (app name) S..."
            parts = line.rpartition(') ')
            if len(parts) < 3: continue
            
            # Get the values after the name
            values = parts[2].split()
            
            try:
                # Field 23 (index 20) = Virtual Memory (bytes)
                # Field 24 (index 21) = RSS (pages)
                vsize_bytes = int(values[20])
                rss_pages = int(values[21])
                
                # Convert to MB
                vsize_mb = vsize_bytes / (1024 * 1024)
                rss_mb = (rss_pages * PAGE_SIZE) / (1024 * 1024)
                
                seconds.append(i) # Assuming 1 line = 1 second
                vsize_list.append(vsize_mb)
                rss_list.append(rss_mb)
                
            except (IndexError, ValueError):
                continue

    # Plotting
    plt.figure(figsize=(12, 6))
    
    # Virtual Memory (Red Line)
    plt.plot(seconds, vsize_list, label='Virtual Memory (VSS)', color='#d62728', linewidth=2)
    
    # RSS (Blue Line)
    plt.plot(seconds, rss_list, label='Resident Set Size (RSS)', color='#1f77b4', alpha=0.7)
    
    plt.title('Memory Usage Stress Test', fontsize=16)
    plt.xlabel('Time (seconds)', fontsize=12)
    plt.ylabel('Memory Usage (MB)', fontsize=12)
    plt.legend(fontsize=12)
    plt.grid(True, linestyle='--', alpha=0.5)
    
    # Save file
    plt.savefig(output_image)
    print(f"Plot saved to {output_image}")

if __name__ == "__main__":
    plot_memory_stats('stats.txt', 'memory_plot.png')
