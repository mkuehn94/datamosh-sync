import datamosh
import numpy

def frames4():
    return numpy.arange(4 * 8 * 8 * 3, dtype=numpy.uint8).reshape(4, 8, 8, 3)

def one_vector():
    return [(0, 0, 0, 2, 2, 1, 1, "hold")]

def test_shift_lands_down_right():
    f = frames4()
    out = datamosh.mosh(f, one_vector(), 8, 8)
    assert (out[1][1:3, 1:3] == f[0][0:2, 0:2]).all()

def test_the_rest_is_untouched():
    f = frames4()
    out = datamosh.mosh(f, one_vector(), 8, 8)
    assert (out[3] == f[3]).all()

def test_out_of_range_vector_is_skipped():
    f = frames4()
    out = datamosh.mosh(f, [(0, 0, 0, 4, 4, 9, 0, "hold")], 8, 8)
    assert (out[1] == f[1]).all()

def test_one_vector_per_beat():
    vecs = [(0, 0, 0, 2, 2, 1, 1, "hold"), (3, 4, 4, 2, 2, 1, 0, "hold")]
    q = datamosh.quantize(vecs, [0, 42, 83], 24)
    assert [v[0] for v in q] == [0, 2]

test_shift_lands_down_right()
test_the_rest_is_untouched()
test_out_of_range_vector_is_skipped()
test_one_vector_per_beat()
print("ok")
