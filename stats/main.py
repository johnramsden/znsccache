import argparse
import pandas as pd
from scipy.stats import gmean, tmean

def parse_arguments():
    parser = argparse.ArgumentParser(description='Calculate latency statistics from a CSV file.')
    parser.add_argument('file_path', type=str, help='Path to the CSV file containing the data.')
    return parser.parse_args()

def calculate_statistics(file_path):
    # Read data from CSV
    df = pd.read_csv(file_path)

    # Calculating the tail latency, e.g., 95th percentile
    tail_latency = df['timems'].quantile(0.99)

    # Calculating the standard deviation of latency
    std_deviation = df['timems'].std()

    # Calculating the geometric mean of latency
    geo_mean = gmean(df['timems'])
    amean = tmean(df['timems'])
    sum_data = sum(df['timems'])

    # Print results
    print("Tail Latency (99th Percentile): {:.2f} ms".format(tail_latency))
    print("Standard Deviation: {:.2f} ms".format(std_deviation))
    print("Geometric Mean: {:.2f} ms".format(geo_mean))
    print("Mean: {:.2f} ms".format(amean))
    print("Sum: {:.2f} ms".format(sum_data))
    print("Sum: {:.2f} s".format(sum_data/1000))

def main():
    args = parse_arguments()
    calculate_statistics(args.file_path)

if __name__ == '__main__':
    main()
