import sys
import os

def main():
    if len(sys.argv) != 3:
        print("Usage: bin2c.py <input_file> <output_file>")
        sys.exit(1)

    input_file = sys.argv[1]
    output_file = sys.argv[2]

    try:
        with open(input_file, 'rb') as f:
            data = f.read()
    except FileNotFoundError:
        print(f"Error: Input file '{input_file}' not found.")
        sys.exit(1)

    hex_data = ', '.join(f'0x{b:02x}' for b in data)
    hex_data += ', 0x00'

    with open(output_file, 'w') as f:
        f.write(hex_data)

if __name__ == '__main__':
    main()
