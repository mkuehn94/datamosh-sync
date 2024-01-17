import numpy
import cv2

def main():
    frame = cv2.imread("in.ppm")
    print(frame.shape)

main()
