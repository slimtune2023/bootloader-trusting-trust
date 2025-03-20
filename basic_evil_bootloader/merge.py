#!/usr/bin/env python3
import sys
import os

def main():
    file1_path = "base/kernel.img"
    file2_path = "extra_fns/handler.bin"
    try:
        buffer_size = 0x200000 - 0x8000 + 0x4000
    except ValueError:
        print("Error: buffer_size must be an integer.")
        sys.exit(1)
    output_path = "kernel.img"
    if (os.path.exists(output_path)):
        os.remove(output_path)

    # Read the first binary file
    if not os.path.exists(file1_path):
        print(f"Error: {file1_path} does not exist.")
        sys.exit(1)

    with open(file1_path, "rb") as f:
        file1_data = f.read()

    # Read the second binary file
    if not os.path.exists(file2_path):
        print(f"Error: {file2_path} does not exist.")
        sys.exit(1)

    with open(file2_path, "rb") as f:
        file2_data = f.read()

    # Check if file1 is larger than the buffer size
    if len(file1_data) > buffer_size:
        print("Error: The size of the first file is larger than the buffer size.")
        sys.exit(1)

    # Calculate how many zero bytes to pad
    pad_length = buffer_size - len(file1_data)

    # Create the output binary file
    with open(output_path, "wb") as out:
        out.write(file1_data)
        out.write(b'\x00' * pad_length)
        out.write(file2_data)

    print(f"Output written to {output_path}.")

if __name__ == '__main__':
    main()
