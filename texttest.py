#!/usr/bin/python

import sys, time
from rgbmatrix import Adafruit_RGBmatrix

font_width  = 4
font_height = 6
lines = ["Hi Mom!", "I'm cool."]

matrix = Adafruit_RGBmatrix(16, 1)
bdf_font_file = "fonts/{0}x{1}.bdf".format(font_width, font_height)
matrix.DrawText(bdf_font_file, 1, 1, 0xFF, 0xFF, 0, lines[0])
time.sleep(2.0)
matrix.DrawText(bdf_font_file, 1, 1 + font_height, 0xFF, 0, 0xFF, lines[1])
time.sleep(10.0)

matrix.Clear()
