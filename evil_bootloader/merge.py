def overwrite_bin(target_file, source_file, offset):
    with open(target_file, "r+b") as target, open(source_file, "rb") as source:
        target.seek(offset)
        data = source.read()
        target.write(data)

overwrite_bin("base/kernel.img", "start/start.bin", 0x0000)
overwrite_bin("base/kernel.img", "extra_fns/handler.bin", 0x4000)