# datamosh-sync

datamoshing where the artifacts follow the beat instead of whatever
the encoder thought was funny. input: a clip, a motion vector sidecar, a wav.
output: the moshed clip, gif if the clip is short.

the vectors come from my own block matcher, the mp4 decoder never went
beyond reading the boxes, the sidecar format is one line per block:

    f x y w h dx dy freeze

the repo is small, the clips are not in it.
