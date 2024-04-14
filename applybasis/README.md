Tools for generating basis functions for various 2D transforms, and applying them to images.

# genbasis
Generate basis function plots in the style of [this one](https://upload.wikimedia.org/wikipedia/commons/archive/2/24/20121105172942%21DCT-8x8.png) from Wikipedia.

## Usage
    Usage: genbasis --size <WxH> [options] <outfile>
    
    Options:
      -h, --help             This help text.
      -f, --function <type>  Type of basis to generate. [default: DFT]
                             Types: DFT, iDFT, DCT[1-4], DST[1-4], WHT, DHT.
      -I, --inverse          Transpose the output.
      -n, --natural          Center the output around the DC. Commonly in DFT visualizations.
      -P, --plane <type>     How to represent complex values in the output image. [default: real]
                             Types: real, imaginary, magnitude, phase
                             Note: types other than "real" are only meaningful for the DFT.
      -s, --size <WxH>       Size of the basis functions.
      -t, --terms <WxH>      Number of basis functions to generate in each dimension. [default: equal to --size]
      -O, --offset <XxY>     Offset the terms by this amount [default: 0x0]
      -p, --padding <p>      Amount of padding to add in between terms. [default: 1]
      -S, --scale <int>      Integer point upscaling factor for basis functions. [default: 1]
      -g, --linear           Generate the basis functions in linear light and scale to sRGB for output.

## Example
16x16 complex DFT basis:

	genbasis --function DFT --size 16x16 --padding 2 --natural --plane complex dftbasis.png

![16x16 complex DFT basis](https://0x09.net/i/g/dftbasis.png "16x16 complex DFT basis")

8x8 DCT basis scaled up:

	genbasis --function DCT2 --size 8x8 --padding 4 --scale 4 dctbasis.png

![8x8 DCT basis](https://0x09.net/i/g/dctbasis.png "8x8 DCT basis")

# applybasis
Apply basis functions from various 2D transforms to an image file, progressively sum the result, or invert a generated set of coefficients. Allows for visualization of each stage in a multidimensional transform.

## Usage
    Usage: applybasis [options] <infile> <outfile>
    
    Options:
      -h, --help             This help text.
      -f, --function <type>  Type of basis to generate. [default: DFT]
                             Types: DFT, iDFT, DCT[1-4], DST[1-4], WHT, DHT.
      -I, --inverse          Transpose the output.
      -n, --natural          Center the output around the DC. Commonly in DFT visualizations.
      -P, --plane <type>     How to represent complex values in the output image. [default: real]
                             Types: real, imaginary, magnitude, phase
                             Note: types other than "real" are only meaningful for the DFT.
      -u, --sum <NxM>        Sum this many terms after applying the basis functions. [default: 1x1 (no summing)]
                             When NxM is the full input dimensions, the output is a fully transformed spectrum of the type specified with -f.
      -t, --terms <WxH>      Number of basis functions to generate in each dimension. [default: equal to the input image dimensions]
      -O, --offset <XxY>     Offset the terms by this amount [default: 0x0]
      -p, --padding <p>      Amount of padding to add in between terms. [default: 1]
      -S, --scale <int>      Integer point upscaling factor for basis functions. [default: 1]
      -g, --linear           Apply the basis functions in linear light and scale to sRGB for output.
      -R, --rescale <type>   How to scale summed values. [default: linear]
                             Types: linear, log, gain, level
                             Two types may be provided, e.g. linear-log. applybasis will interpolate between these as the number of summed terms increases.
      -N, --range <type>     How to visualize negative values. [default: shift2]
                             Types:
                               shift  - shift into 0,1 range (brightens output)
                               shift2 - shift into -1,1 range prior to applying basis
                               abs    - take the absolute value
                               invert - wrap around
                               hue    - apply a color rotation
      -d <file.coeff>        Optional file to store transformed coefficients. May be used later as an input together with the --inverse flag to invert the original transform.


Note: all applybasis options that pertain to basis generation function the same as the corresponding genbasis option, with the small exception that `-P complex` is only available in genbasis as it makes use of the otherwise unused color planes in genbasis' output.

## Example

Progressively-summed 16x16 DCT/iDCT of [an example image](https://0x09.net/i/g/flower.png).
	
	for i in 1 4 16; do applybasis -f DCT2 -u ${i}x${i} -S $i flower.png fdct_$i.png; done
	applybasis -fDCT2 -s16x16 -S16 -d out.coeff flower.png /dev/null
	for i in 1 4 16; do applybasis -f DCT3 -I -u ${i}x${i} -S $i out.coeff idct_$i.png; done

Forward on top, inverse on bottom:

![Progressively-summed example image DCT](https://0x09.net/i/g/flower_sums.png "Progressively-summed example image DCT")

# draw
Draw images in frequency space by setting coordinates and coefficient value.

## Usage
    Usage: draw -b <WxH> [-f <XxY:strength> ...] <outfile>
    
        Options:
      -b <WxH>           Size of the output image.
      -f <XxY:strength>  Frequency component position and value. My repeat.

## Example
Drawing with several coefficients

	draw -b 256x256 -f 3x3:0.4 -f 2x5:0.2 -f 4x6:0.2 -f 145x132:0.05 draw.png

![Drawing with several coefficients](https://0x09.net/i/g/draw.png "Drawing with several coefficients")
