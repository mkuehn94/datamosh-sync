import sys

# concatenated ppms -> one raw rgb stream, the loader wants no headers
def main():
    data = open(sys.argv[1], "rb").read()
    out = []
    i = 0
    while i < len(data):
        # P6 <w> <h> 255
        f = data[i:i + 30].split()
        w = int(f[1])
        h = int(f[2])
        start = data.index(b"255", i) + 3
        n = w * h * 3
        out.append(data[start:start + n])
        i = start + n
    sys.stdout.buffer.write(b"".join(out))

main()
