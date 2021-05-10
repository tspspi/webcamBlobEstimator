# Simple single blob detector

## Introduction

This is a really simple blob detector that locates a single
bright cluster on an webcam image. It has been used during electron
beam diagnosis. This application __might not be useful for anything
else than the task it has been designed (or better call it hacked) for__.
This application is based on one of my [blog posts about webcam
access from C](https://www.tspi.at/2021/02/06/webcamcapture.html)

## External dependencies

| Package                                                        | License                        | Comment                                                           |
| -------------------------------------------------------------- | ------------------------------ | ----------------------------------------------------------------- |
| [libjpeg](https://github.com/mozilla/mozjpeg)                  | IJG license - BSD like license |                                                                   |
| [rawsockscpitools](https://github.com/tspspi/rawsockscpitools) | 4 clause BSD license           | In case Siglent SSG3021X should be controlled for frequency sweep |

## Description

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

## Building

The application is simply built by using GNU make:

```
gmake
```

Note that include paths and library paths have to include ```libjpeg``` and
if required one has to add the ```rawsockscpitools``` library to the Makefile.

```
-CCOBJ=clang -I/usr/local/include/ -Wall -ansi -std=c99 -pedantic -c
+CCOBJ=clang -I/usr/local/include/ -Wall -ansi -std=c99 -pedantic -c -DSSG_ENABLE -I/usr/home/tsp/githubRepos/rawsockscpitools/include
-CCLINKSUFFIX=-L/usr/local/lib -ljpeg
+CCLINKSUFFIX=-L/usr/local/lib /usr/home/tsp/githubRepos/rawsockscpitools/bin/librawsockscpitools.a -ljpeg
```
