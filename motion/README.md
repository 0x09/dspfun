 motion - apply various 2- or 3-dimensional frequency-domain operations to an image or video.

# Usage

	motion -i infile [-o outfile]
	[-s|--size WxHxD] [-b|--blocksize WxHxD] [-p|--bandpass X1xY1xZ1-X2xY2xZ2]
	[-B|--boost float] [-D|--damp float]  [--spectrogram type] [-q|--quant quant] [-d|--dither]
	[--keep-rate] [--samesize-chroma] [--frames lim] [--offset pos] [--csp|c colorspace] [--iformat|--format fmt] [--codec codec] [--encopts|--decopts opts]

	-b|--blocksize - 3D size of blocks to operate on. (full input dimensions)
	-s|--size - 3D size of output blocks, if scaling. (blocksize)
	-p|--bandpass - Beginning and end coordinates of brick-wall bandpass. (blocksize)
	-B|--boost - Multiplier for the pass band. (1)
	-D|--damp - Multiplier for the stop band. (0)
	--spectrogram <type> - 0/none = absolute value spectrum, 1 = shifted spectrum, -1 = invert a shifted spectrum
	-q|--quant - Quantize the frequency coefficients by multiplying by this qfactor and rounding.
	-d|--dither - Apply 2D Floyd-Steinberg dithering to the high-precision transform products.

	--keep-rate - If scaling in time, maintain same framerate.
	--samesize-chroma - Subsampled chroma planes will use the same block size as the Y plane.

3D coordinates can take 0 to represent their parent's size.

Beware that the default block size will be the dimensions of the input. For all but the smallest videos this will consume a massive amount of memory. To operate on a full-frame basis instead, use -b0x0x1

Refer to the FFmpeg documentation for colorspace, format, encoder, and enc/decopts.

Includes a script `motionplay` to pipe `motion` output and view it in `ffplay`.
	
	Usage: motionplay -i infile <non-output motion args>

# Examples

Perform a 3-dimensional analog to JPEG-style compression:
	
	motion -b8x8x8 -q 20 --samesize-chroma -i ... -o ...

Watch a 2D spectrum of a video

	motionplay -b0x0x1 --spectrogram -i ...
	
