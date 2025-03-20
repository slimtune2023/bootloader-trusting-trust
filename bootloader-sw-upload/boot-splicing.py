import os

def splice_binary_files(output_file, file_offsets):
    """
    Splices multiple binary files together at specific offsets.
    
    :param output_file: The name of the output binary file.
    :param file_offsets: A list of tuples (file_path, offset), where file_path is the binary file to read
                         and offset is the position in the output file where the content should be placed.
    """
    # Determine the final file size
    final_size = 0
    for file_path, offset in file_offsets:
        file_size = os.path.getsize(file_path)
        final_size = max(final_size, offset + file_size)
    
    # Create and initialize the output file with null bytes
    with open(output_file, 'wb') as f:
        f.write(b'\x00' * final_size)
    
    # Write each file at its specified offset
    with open(output_file, 'r+b') as f:
        for file_path, offset in file_offsets:
            with open(file_path, 'rb') as src:
                f.seek(offset)
                f.write(src.read())
    
    print(f"Success!")

def extract_binary_section(input_file, output_file, offset, length):
    """
    Extracts a specific section from a binary file and saves it as a new file.
    
    :param input_file: The binary file to extract from.
    :param output_file: The file to save the extracted section.
    :param offset: The starting position in the binary file.
    :param length: The number of bytes to extract.
    """
    with open(input_file, 'rb') as f:
        f.seek(offset)
        data = f.read(length)
    
    # Insert the length of data as the first four bytes
    data_with_size = len(data).to_bytes(4, 'little') + data
    # print(data_with_size[0:4])
    # print(hex(len(data_with_size) - 4))
    
    with open(output_file, 'wb') as f:
        f.write(data_with_size)
    
    # print(f"Extracted section saved to '{output_file}'.")
    return len(data_with_size) - 4

# Example usage
if __name__ == "__main__":
    boot_name = input("bootloader name: ")
    cluster_length = extract_binary_section(f"../bootloaders/{boot_name}", "last_cluster", 63 * 512 * 64, 512 * 64 * 5)
    print(cluster_length / (512 * 64))

    prog_name = input("program name: ")
    file_offsets = [
        (prog_name, 0),   # Place file1.bin at offset 0
        ("last_cluster", 40960)  # 0xA000 (address 0x12000)
    ]

    combined_name = input("merged bin name: ")
    splice_binary_files(f"{combined_name}", file_offsets)    
