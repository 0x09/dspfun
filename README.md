DSP Funbox - Set of *nix utilities for experimentation and learning about spectral analysis of images.

![dspfun](https://0x09.net/i/g/dspfun.png "8x8 DFT basis")  
<sub>8x8 DFT basis created with [genbasis](applybasis#genbasis)</sub>
# Overview

dspfun is a loosely related collection of commandline tools built to help visualize or test various aspects of 2- and 3-dimensional signal processing in the frequency domain.  
Each set of tools aims to cover a specific aspect of this &mdash; such as visualizing the basis functions of transforms or generating spectrograms &mdash; and including things that are not necessarily practical but may be interesting to experiment with.

# Toolsets

* [spec](spec) - Generate DCT spectrums for viewing or invertible spectrums for editing in a regular image editor.
* [motion](motion) - 3-dimensional frequency-space editor for video.
* [applybasis](applybasis) - Tools for working with basis functions of a variety of 2D transforms.
* [zoom](zoom) - Interpolate images against a cosine basis.
* [scan](scan) - Progressively reconstruct images using various frequency space scans.

See each toolset for additional info about it.

# Dependencies
Other than a Unix-like build environment, most tools in this framework depend on libraries from one or more of these projects:

* [FFTW](https://www.fftw.org) for performing transforms.
* [ImageMagick](https://www.imagemagick.org) (MagickWand) for image I/O.
* [FFmpeg](https://ffmpeg.org) for video I/O.

# Building
Running

	make

From the top level will build each project. Alternatively each project may be built independently with the Makefile in its directory.

# Common conventions
Usage for each set of tools is described in its own README, but these follow some common conventions and often share functionality, especially with respect to input and output.

## Image and video I/O
ImageMagick is used for image input and output, so any formats and specifiers supported by it (e.g. `png:-` for png pipe output) may be used.
All tools which produce image output default to Sixel graphics for inline display if the output is a terminal.  

FFmpeg's libraries are used for video input and output and so likewise formats supported by it are accepted. Tools which output video also accept a special argument `ffplay:` instead of a file or pipe to display raw video output using the ffplay binary. This will configure ffplay with the correct color properties for the output so may be preferable to e.g. piping yuv4mpeg.  

## Configurable floating point precision
The internal floating point precision for all tools may be configured at compile time by setting make vars `COEFF_PRECISION` and `INTERMEDIATE_PRECISION` with a value of F, D, or L for float, double, or long double.
This is intended to allow for some simple configurability of tools' speed and memory use vs precision, or simply to see how different levels of precision affect results.  
These define the precision used for FFTW transforms and storage buffers (coeff precision), and for intermediate calculations (intermediate precision.)
Specifying these is optional and each project that can be configured this way has defaults appropriate for it.

## Linear light processing
Most tools process in the input's colorspace by default, but offer the option to process in linear RGB. For tools that support this it can be enabled with the `-g` (for gamma correct) short option or `--linear` long option.
