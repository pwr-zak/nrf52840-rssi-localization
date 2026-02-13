
import re
from pathlib import Path
import pandas as pd
import matplotlib.pyplot as plt


def load_multichannel_rssi_from_file(file_path):
    
    with open(file_path, 'r') as file:
        data = file.read()

    entries = re.findall(r'chan_idx:\s*(\d+)\s*rssi:\s*(-?\d+)', data)
    df = pd.DataFrame(entries, columns=['chan_idx', 'rssi'])
    df['chan_idx'] = df['chan_idx'].astype(int)
    df['rssi'] = df['rssi'].astype(int)

    return df




def add_frequency_column(df):
    """
    Adds a Frequency column (in MHz) based on BLE channel index.
    
    BLE advertising channels: 37 (2402 MHz), 38 (2426 MHz), 39 (2480 MHz)
    BLE data channels 0-36: 2404 + chan_idx * 2 MHz (skipping 2426 MHz)
    """
    def chan_idx_to_frequency(chan_idx):
        if chan_idx == 37:
            return 2402
        elif chan_idx == 38:
            return 2426
        elif chan_idx == 39:
            return 2480
        elif 0 <= chan_idx <= 10:
            return 2404 + chan_idx * 2
        elif 11 <= chan_idx <= 36:
            return 2428 + (chan_idx - 11) * 2
        else:
            return None
        
    if "Frequency" not in df.columns:
        df['Frequency'] = df['chan_idx'].apply(chan_idx_to_frequency)
    return df




## WATERFALL - ANALIZATOR WIDMA

def load_waterfal_from_file(filepath):

    with open(filepath, 'r') as f:
        lines = [line.strip() for line in f if line.strip()]
    
    # Parse header
    frames = None
    values = None
    data_start = 0
    for i, line in enumerate(lines):
        if line.startswith("Frames"):
            frames = int(line.split(';')[1])
        elif line.startswith("Values"):
            values = int(line.split(',')[1])
        elif line.startswith("Frame"):
            data_start = i
            break

    # Parse lines
    records = []
    i = data_start
    while i < len(lines):
        if lines[i].startswith("Frame"):
            frame_num = int(lines[i].split(';')[1])
            timestamp = lines[i+1]
            for j in range(values):
                #print(lines[i+2+j].split(';')[0:-1])
                freq, strength = map(float, lines[i+2+j].split(';')[0:-1])
                records.append({
                    'Frame': frame_num,
                    'Timestamp': timestamp,
                    'Frequency': freq,
                    'SignalStrength': strength
                })
            i += 2 + values
        else:
            i += 1

    df = pd.DataFrame(records)
    return df

def average_waterfall_by_frequency(df, threshold=-40, first_frame=None, last_frame=None):
    """
    Averages signal strength by frequency over time - frequencies with transmission will have higher signal strength
    compared to frequencies without transmission. Threshold allows to filter out low signal strengths (where no transmission
    is present). 

    Args:
    :param df: DataFrame containing the waterfall data.
    :param threshold: Signal strength threshold for filtering.
    :param first_frame: Optional first frame from waterfall to include in the averaging. Note it is <0
    :param last_frame: Optional last frame from waterfall to include in the averaging. Note it is <0
    """

    # Filter by signal strength threshold
    filtered = df[df['SignalStrength'] >= threshold]
    # Filter by frame range if specified
    if first_frame is not None:
        filtered = filtered[filtered['Frame'] >= first_frame]
    if last_frame is not None:
        filtered = filtered[filtered['Frame'] <= last_frame]
    # Group by frequency and calculate mean signal strength
    avg_df = filtered.groupby('Frequency', as_index=False)['SignalStrength'].mean()
    avg_df.rename(columns={'SignalStrength': 'AvgSignalStrength'}, inplace=True)
    return avg_df

