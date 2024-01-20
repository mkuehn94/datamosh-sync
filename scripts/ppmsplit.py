import sys

# concatenated ppms -> one raw rgb stream, the loader wants no headers.
# the 255 of a maxval must not be searched for, pixel bytes are full of
# them, the header is read token by token instead.
def main():
    data = open(sys.argv[1], "rb").read()
    out = []
    i = 0
    while i < len(data):
        if data[i:i + 2] != b"P6":
            break
        i += 2
        nums = []
        while len(nums) < 3:
            while data[i] in (32, 10, 9):
                i += 1
            s = i
            while data[i] not in (32, 10, 9):
                i += 1
            nums.append(int(data[s:i]))
        i += 1
        w, h, _max = nums
        n = w * h * 3
        out.append(data[i:i + n])
        i += n
    sys.stdout.buffer.write(b"".join(out))

main()
