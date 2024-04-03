Tools for generating basis functions for various 2D transforms, and applying them to images.

# genbasis
Generate basis function plots in the style of [this one](https://upload.wikimedia.org/wikipedia/commons/archive/2/24/20121105172942%21DCT-8x8.png) from Wikipedia.

## Usage
    genbasis -o outfile -f|--function=(DFT),iDFT,DCT[1-4],DST[1-4],WHT [-I|--inverse] [-n|--natural] [-P|--plane=(real),imag,mag,phase,cplx]
             -s|--size WxH [-t|--terms WxH] [-O|--offset XxY] [-p|--padding p] [-S|--scale WxH]

	plane - How to represent complex values. Arguments other than `real` only meaningful for the DFT.
	inverse - Generate the transpose.
	natural - For DFT plots, put the DC at the center.
	size, terms, offset - By default a set with size NxM will generate NxM basis functions, each NxM in size. To output fewer tiles, set `terms` to the desired number of basis functions. `offset` can be used to set where in the space to start generating.

## Example
16x16 complex DFT basis:

	genbasis --function=DFT --size 16x16 --padding 2 --natural --plane=cplx -o dftbasis.png
	genbasis -s16x16 -p2 -n -Pcplx -o dftbasis.png

![16x16 complex DFT basis](https://0x09.net/i/g/dftbasis.png "16x16 complex DFT basis")

8x8 DCT basis scaled up:

	genbasis --function=DCT2 --size 8x8 --padding 4 --scale 4 -o dctbasis.png
	genbasis -fDCT -s8x8 -p4 -S4 -o dctbasis.png

![8x8 DCT basis](https://0x09.net/i/g/dctbasis.png "8x8 DCT basis")

# applybasis
Apply basis functions from various 2D transforms to an image file, progressively sum the result, or invert a generated set of coefficients. Allows for visualization of each stage in a multidimensional transform.

## Usage

    applybasis -i infile -o outfile [-d out.coeff]
               -f|--function=(DFT),iDFT,DCT[1-4],DST[1-4],WHT  [-I|--inverse]
               [-P|--plane=(real),imag,mag,phase]  [-R|--rescale=(linear),log,gain,level[-...]]  [-N|--range=shift,(shift2),abs,invert,hue]
               [-t|--terms WxH]  [-s|--sum NxM]  [-O|--offset XxY]  [-p|--padding p]  [-S|--scale scale]

	range - How to visualize negative values:
		shift - shift into 0-255 range (brightens image)
		shift2 - shift image into -255..255 range prior to applying basis (default)
		abs - take the absolute value
		invert - wrap around
		hue - apply a color rotation

	rescale - How to scale summed values. Two arguments can be used when summing, to smoothly transition between i.e. linear and log scaling when visualizing a frequency spectrum.

This tool shares several parameters with `genbasis`, documented above.

## Example

Progressively-summed 16x16 DCT/iDCT of [an example image](https://0x09.net/i/g/flower.png).
	
	for i in 1 4 16; do applybasis -i /tmp/flower.png -fDCT2 -s${i}x${i} -S$i -o fdct_$i.png; done
	applybasis -i /tmp/flower.png -fDCT2 -s16x16 -S16 -d/tmp/out.coeff -o /dev/null
	for i in 1 4 16; do applybasis -i /tmp/out.coeff -fDCT3 -I -s${i}x${i} -S$i -o idct_$i.png; done

Forward on top, inverse on bottom:

![Progressively-summed example image DCT](https://0x09.net/i/g/flower_sums.png "Progressively-summed example image DCT")

# draw
Draw images in frequency space by setting coordinates and coefficient value.

## Usage
	draw -b WxH [-f XxY:strength ...] outfile

## Example
Drawing with several coefficients

	draw -b 256x256 -f 3x3:0.4 -f 2x5:0.2 -f4x6:0.2 -f 145x132:0.05 draw.png

![Drawing with several coefficients](https://0x09.net/i/g/draw.png "Drawing with several coefficients")
