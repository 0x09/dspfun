Tools for working with image/video as a 3-dimensional time series

# motion
motion - apply various 2- or 3-dimensional frequency-domain operations to an image or video.

## Usage

	motion -i infile [-o outfile]
	[-s|--size WxHxD] [-b|--blocksize WxHxD] [-p|--bandpass X1xY1xZ1-X2xY2xZ2]
	[-B|--boost float] [-D|--damp float]  [--spectrogram type] [-q|--quant quant] [-d|--dither]
	[--keep-rate] [--samesize-chroma] [--frames lim] [--offset pos] [--csp|c colorspace options] [--iformat|--format fmt] [--codec codec] [--encopts|--decopts opts]

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

## Examples

Perform a 3-dimensional analog to JPEG-style compression:
	
	motion -b8x8x8 -q 20 --samesize-chroma -i ... -o ...

Watch a 2D spectrum of a video

	motionplay -b0x0x1 --spectrogram -i ...

# Rotate
rotate - rotate video by right angles on a 3-dimensional axis.

## Usage

	usage: rot <ffapi args> -r framerate -s start:frames [-]xyz in out
	[-]xyz: new dimensional arrangement, with -/+ to indicate direction

	ffapi args: -o/O   input/output dictionary options
	            -f/F   input/output format
	            -c     intermediate colorspace options
	            -e     encoder
	            -l     loglevel

## Examples
Rotate a short video such that the time axis is now facing the viewer, with frames stacked as "slices" from left to right. If the dimensions of our input.avi are 1280x720, and it is 3000 frames long, the output will be 3000x720, and 1280 frames long. We can limit the number of frames, thus limiting the output width to 1920, so that it is fully visible on most monitors:

	rotate -s 0:1920 zy-x input.avi timeline.avi

Rotate the output of the previous example back to normal, perhaps after applying some filter to it to interesting effect

	rotate zyx timeline.avi original.avi

# Transcode
transcode - barebones transcoder / FFmpeg API wrapper test client. Simply decodes and re-encodes frames in the given format at a constant framerate. All functionality is shared with the above tools.

## Usage

	usage: transcode <ffapi args> -r framerate -s start:frames input output

## Examples
Just convert an mp4 to y4m

	transcode input.mp4 output.y4m

Convert an avi to mp4, specifying encoding options

	transcode -e libx264 -O pixel_format=yuv444p:crf=16:preset=veryslow input.avi output.mp4