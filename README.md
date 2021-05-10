# Simple single blob detector

This is a really simple blob detector that locates a single
bright cluster on an webcam image. It has been used during electron
beam diagnosis.

It works on a really simple principle (no DoG pyramids, scale spaces, etc.)
by simply:

* Optionally setting the frequency on an Siglent frequency generator. This
  requires the [raw socks SCPI library](https://github.com/tspspi/rawsockscpitools)
* Capturing a frame
* Doing a grayscale transformation
* Locating the pixel with maximum brightness (seed pixel)
* Associating each pixel in the neighborhood of a cluster pixel with
  the cluster if it's intensity is above a given threashold
* Determining the spatial size (in pixel) of the cluster
* Outputting the size and sum of intensity values to a measurement file

All data is stored into specific files when doing multiple measurements
using the signal generator.

![Example capture](./doc/testoutput/measurement43000000-raw.jpg)

![Example cluster](./doc/testoutput/measurement43000000-cluster.jpg)
