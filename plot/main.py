import argparse
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def parse_arguments():
    parser = argparse.ArgumentParser(description='Plot data from a CSV file and save to PDF and PNG.')
    parser.add_argument('file_path', type=str, help='Path to the CSV file containing the data.')
    parser.add_argument('title', type=str, help='Title of the plot.')
    parser.add_argument('y_label', type=str, help='Label for the Y-axis.')
    parser.add_argument('output', type=str, help='Filename for outputs (without ext).')
    return parser.parse_args()

def clean_data(data):
    # Set a baseline datetime (e.g., start of the current day or another fixed time)
    baseline = pd.Timestamp('today').normalize()  # This sets the time to 00:00:00 today
    # Convert elapsed milliseconds to seconds and add to baseline
    data['timestamp'] = baseline + pd.to_timedelta(data['timestamp'], unit='ms')
    data['timems'] = pd.to_numeric(data['timems'], errors='coerce')
    # Drop any rows where conversion failed
    data.dropna(subset=['timestamp', 'timems'], inplace=True)
    return data


def aggregate_data(data, bin_size=1000):  # bin_size in milliseconds
    """
    Manually bin the data by 'timestamp' and average 'timems' within each bin.
    """
    # Define bins based on the maximum timestamp and bin size
    max_time = data['timestamp'].max()
    bins = range(0, int(max_time + bin_size), bin_size)

    # Group data by bins and calculate the mean
    data['bin_index'] = np.digitize(data['timestamp'], bins) - 1
    aggregated_data = data.groupby('bin_index')['timems'].mean().reset_index()
    aggregated_data['bin_time'] = aggregated_data['bin_index'] * (bin_size / 1000)  # Time in seconds

    return aggregated_data

def detect_outliers(data):
    Q1 = data['timems'].quantile(0.25)
    Q3 = data['timems'].quantile(0.75)
    IQR = Q3 - Q1
    outlier_threshold_upper = Q3 + 3.0 * IQR  # Using 3 times the IQR for stricter detection
    outlier_threshold_lower = Q1 - 3.0 * IQR
    outliers = data[(data['timems'] > outlier_threshold_upper) | (data['timems'] < outlier_threshold_lower)]
    return outliers


def plot_data(file_path, title, y_label, output):
    # Load the data into a DataFrame
    data = pd.read_csv(file_path)

    # Detect outliers before aggregating
    outliers = detect_outliers(data)

    # Aggregate data
    aggregated_data = aggregate_data(data)

    # Plotting
    fig, ax = plt.subplots(figsize=(12, 7))
    ax.plot(aggregated_data['bin_time'], aggregated_data['timems'], linestyle='-', label='Aggregated Data')
    ax.scatter(outliers['timestamp'] / 1000, outliers['timems'], color='red', s=10, label='Outliers')  # Smaller dot for outliers

    # Customizing the plot
    ax.set_title(title)
    ax.set_xlabel('Time (s)')
    ax.set_ylabel(y_label)
    ax.grid(True)
    ax.legend()
    # Save the plot to files
    # plt.savefig(f"{output}.pdf", format='pdf', dpi=300)
    plt.savefig(f"{output}.png", format='png', dpi=300)

    # Display the plot
    plt.show()


def main():
    args = parse_arguments()
    plot_data(args.file_path, args.title, args.y_label, args.output)


if __name__ == '__main__':
    main()
