import re
import sys

def parse_smaps_from_log(log_file):
    """
    Parses a log file to extract and print smaps information.

    Args:
        log_file (str): The path to the log file.
    """
    try:
        with open(log_file, 'r') as f:
            lines = f.readlines()
    except FileNotFoundError:
        print(f"Error: Log file not found at '{log_file}'")
        sys.exit(1)

    # Regex to find the header line and extract the timestamp and content
    header_regex = re.compile(r"(\d{2}-\d{2}\s\d{2}:\d{2}:\d{2}\.\d{3}).*?(name\|.*)")
    # Regex to find and extract the content of a data line
    data_regex = re.compile(r".*?I starboard:\s*(.*\|.*)")

    in_smap_block = False
    
    for line in lines:
        if not in_smap_block:
            header_match = header_regex.search(line)
            if header_match:
                timestamp = header_match.group(1)
                header_content = header_match.group(2)
                
                print(f"At {timestamp}")
                print(header_content.rstrip())
                in_smap_block = True
        else:
            # We are inside a smap block
            data_match = data_regex.search(line)
            if data_match:
                data_content = data_match.group(1)
                print(data_content.rstrip())
            else:
                # The block has ended, print a newline for separation
                in_smap_block = False
                print("")

def main():
    """Main function."""
    log_file_path = 'log.starboard.txt'
    if len(sys.argv) > 1:
        log_file_path = sys.argv[1]
    
    parse_smaps_from_log(log_file_path)

if __name__ == "__main__":
    main()
