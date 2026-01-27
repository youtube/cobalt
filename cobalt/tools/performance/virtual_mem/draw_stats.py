import matplotlib.pyplot as plt
import pandas as pd

def plot_memory_stats(input_file, output_image):

    print(f"Reading {input_file}...")
    
    # Load the data
    try:
        df = pd.read_csv(input_file)
    except FileNotFoundError:
        print("Error: file not found. Run the bash script first.")
        exit()

    df['Timestamp'] = pd.to_datetime(df['Timestamp'], format='%H:%M:%S')
    # Calculate Elapsed Time in seconds
    df['Elapsed'] = (df['Timestamp'] - df['Timestamp'].iloc[0]).dt.total_seconds()

    # Convert KB to MB for easier reading
    df['PSS_MB'] = df['PSS'] / 1024
    df['RSS_MB'] = df['RSS'] / 1024
    df['GPU_MB'] = df['Graphics'] / 1024

    # Create the plot
    plt.figure(figsize=(12, 6))

    plt.plot(df['Elapsed'], df['PSS_MB'], label='Total PSS (MB)', color='blue', linewidth=2)
    plt.plot(df['Elapsed'], df['RSS_MB'], label='Total RSS (MB)', color='green', linestyle='--')
    plt.plot(df['Elapsed'], df['GPU_MB'], label='GPU Memory (MB)', color='red')

    # Formatting
    plt.title('Memory Profile: dev.cobalt.coat', fontsize=14)
    plt.xlabel('Time (Seconds)', fontsize=12)
    plt.ylabel('Memory Usage (MB)', fontsize=12)
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.xticks(rotation=45)
    plt.tight_layout()

    # Save and Show
    plt.savefig(output_image)
    print(f"Plot saved to {output_image}")


if __name__ == "__main__":
    plot_memory_stats('stats.txt', 'memory_plot.png')