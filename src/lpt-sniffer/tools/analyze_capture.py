#!/usr/bin/env python3
"""
PARALAX Capture Analyzer
Analyzes LPT capture data to identify device type and characteristics
"""

import csv
import sys
import statistics
from collections import Counter

def analyze_capture(filename):
    """Analyze captured LPT data and identify device type"""
    
    samples = []
    
    # Read capture file
    print(f"Reading {filename}...")
    try:
        with open(filename, 'r') as f:
            reader = csv.reader(f)
            for row in reader:
                # Skip comments and headers
                if not row or row[0].startswith('#') or row[0].startswith('TIMESTAMP'):
                    continue
                
                try:
                    timestamp = int(row[0])
                    data = int(row[1], 16)
                    samples.append((timestamp, data))
                except (ValueError, IndexError):
                    continue
    except FileNotFoundError:
        print(f"Error: File '{filename}' not found")
        return
    
    if len(samples) < 2:
        print("Error: Not enough samples captured")
        return
    
    print(f"Captured {len(samples)} samples")
    print()
    
    # Calculate timing statistics
    deltas = [samples[i+1][0] - samples[i][0] for i in range(len(samples)-1)]
    
    avg_delta = statistics.mean(deltas)
    median_delta = statistics.median(deltas)
    stdev_delta = statistics.stdev(deltas) if len(deltas) > 1 else 0
    min_delta = min(deltas)
    max_delta = max(deltas)
    
    # Calculate sample rate
    sample_rate = 1_000_000 / avg_delta if avg_delta > 0 else 0
    
    print("=== Timing Analysis ===")
    print(f"Average period: {avg_delta:.1f} μs")
    print(f"Median period:  {median_delta:.1f} μs")
    print(f"Std deviation:  {stdev_delta:.1f} μs")
    print(f"Min period:     {min_delta} μs")
    print(f"Max period:     {max_delta} μs")
    print(f"Sample rate:    {sample_rate:.0f} Hz ({sample_rate/1000:.1f} kHz)")
    print()
    
    # Analyze data values
    data_values = [d for _, d in samples]
    
    print("=== Data Analysis ===")
    print(f"Min value: 0x{min(data_values):02X} ({min(data_values)})")
    print(f"Max value: 0x{max(data_values):02X} ({max(data_values)})")
    print(f"Mean value: {statistics.mean(data_values):.1f}")
    print(f"Unique values: {len(set(data_values))}")
    
    # Check for common patterns
    counter = Counter(data_values)
    most_common = counter.most_common(5)
    print(f"Most common values:")
    for value, count in most_common:
        percentage = (count / len(data_values)) * 100
        print(f"  0x{value:02X}: {count} times ({percentage:.1f}%)")
    print()
    
    # Device identification
    print("=== Device Identification ===")
    
    if 20_000 < sample_rate < 24_000:
        print("✓ COVOX SPEECH THING detected")
        print("  - Sample rate matches 22 kHz typical Covox output")
        print("  - Continuous streaming DAC")
        
    elif 6_000 < sample_rate < 8_000:
        print("✓ DISNEY SOUND SOURCE detected")
        print("  - Sample rate matches 7 kHz typical DSS output")
        print("  - FIFO-based DAC")
        
    elif avg_delta > 50 and max_delta > 500:
        print("? Possible OPL2LPT detected")
        print("  - Irregular timing suggests register writes")
        print("  - Need paired address/data analysis")
        
        # Check for OPL2 register patterns
        if check_opl2_pattern(samples):
            print("  ✓ OPL2 register write pattern detected")
    
    else:
        print("? UNKNOWN DEVICE")
        print("  - Timing doesn't match known patterns")
        print("  - May need additional analysis")
    
    print()
    
    # Generate recommendations
    print("=== Recommendations ===")
    
    if stdev_delta > avg_delta * 0.1:
        print("⚠ High timing variance detected")
        print("  - Check for system interrupts on DOS machine")
        print("  - Verify STROBE connection quality")
    
    if len(set(data_values)) < 16:
        print("⚠ Low data diversity")
        print("  - May indicate poor connection on data lines")
        print("  - Verify D0-D7 wiring")
    
    duration_sec = (samples[-1][0] - samples[0][0]) / 1_000_000
    print(f"✓ Capture duration: {duration_sec:.2f} seconds")
    
    if duration_sec < 1.0:
        print("  - Consider longer capture for better analysis")

def check_opl2_pattern(samples):
    """Check if data follows OPL2 register write pattern"""
    # OPL2 writes come in pairs: address (0x00-0xF5), then data
    # This is a simplified check
    
    addresses = [d for _, d in samples if d <= 0xF5]
    percentage = (len(addresses) / len(samples)) * 100
    
    return percentage > 50  # More than half are valid OPL2 addresses

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze_capture.py <capture.csv>")
        print()
        print("Analyzes PARALAX LPT capture data to identify device type")
        sys.exit(1)
    
    analyze_capture(sys.argv[1])

if __name__ == "__main__":
    main()
