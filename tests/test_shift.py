import datamosh
import numpy

def frames4():
    return numpy.arange(4 * 8 * 8 * 3, dtype=numpy.uint8).reshape(4, 8, 8, 3)

def one_vector():
    return [(0, 0, 0, 2, 2, 1, 1, "hold")]

def test_shift_lands_down_right():
    f = frames4()
    out = datamosh.mosh(f, one_vector(), 8, 8)
    assert (out[1][1:3, 1:3] == f[1][0:2, 0:2]).all()

def test_the_rest_is_untouched():
    f = frames4()
    out = datamosh.mosh(f, one_vector(), 8, 8)
    assert (out[3] == f[3]).all()

test_shift_lands_down_right()
test_the_rest_is_untouched()
print("ok")
