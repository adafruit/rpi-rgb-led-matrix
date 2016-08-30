#!/usr/bin/python

import sys, time
from rgbmatrix import Adafruit_RGBmatrix

matrix = Adafruit_RGBmatrix(16, 1)

matrix.Fill(0xFF0000)
time.sleep(1.0)
matrix.Clear()

bdf_font_file = "fonts/5x7.bdf"
msg = u'Hi Mom!'
msg.encode('utf-8')
matrix.DrawText(bdf_font_file, 1, 1, 0xFF, 0xFF, 0, msg);
time.sleep(5.0)

matrix.Clear()
print "good night."
