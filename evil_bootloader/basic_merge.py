def overwrite_bin(target_file, source_file, offset):
    with open(target_file, "r+b") as target, open(source_file, "rb") as source:
        target.seek(offset)
        data = source.read()
        target.write(data)

overwrite_bin("base/kernel.img", "start/basic.bin", 0x0000)