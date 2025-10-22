Tools for working with image/video as a 3-dimensional time series

# motion
Apply various 2- or 3-dimensional frequency-domain operations to an image or video.

This tool takes an input image or video and, treating width × height × time as a 3D volume, performs a 3 dimensional discrete cosine transform in the block size specified.  
These transformed blocks may be filtered in frequency space via some built-in operations (band pass, quantization, scaling) or with arbitrary formulas via FFmpeg's expression evaluator.  
The result is then either transformed back into the space/time domain for output, or a spectrogram visualization of the 3D frequency domain may be produced instead.

**Photosensitivity note:** this tool enables operations affecting motion in the frequency domain and so is capable of producing artifacts like the [Gibbs phenomenon](https://en.wikipedia.org/wiki/Gibbs_phenomenon) in time, which can manifest as rapid flashing depending on the input and options used.  
As a precaution the output rate can be limited to a very low or even fractional value with the `--framerate` option described below.

## Usage

    Usage: motion [options] <infile> [outfile]
    
      <outfile>               Output file or pipe, or "ffplay:" for ffplay output. If no output file is given motion prints the input dimensions and exits.
    
      -h, --help              This help text.
      -Q, --quiet             Silence progress and other non-error output.
    
      -b, --blocksize <dims>  3D size of blocks to operate on in the form WxHxD. [default: 0x0x1 (the full input frame dimensions)]
      -s, --size <dims>       3D size of output blocks in the form WxHxD for scaling. [default: 0x0x0 (the blocksize)]
      -p, --bandpass <range>  Beginning and end coordinates of brick-wall bandpass in the form X1xY1xZ1-X2xY2xZ2. [default: 0x0x0 through blocksize]
      -B, --boost <float>     Multiplier for the pass band. [default: 1]
      -D, --damp <float>      Multiplier for the stop band. [default: 0]
    
      --spectrogram[=<type>]   Output a spectrogram visualization, optionally specifying the type. [default: abs]
                               Type: abs, shift, flat, copy.
      --ispectrogram[=<type>]  Invert an input spectrogram. [default: shift]
                               Type: shift, flat, copy.
    
      -q, --quant <float>     Quantize the frequency coefficients by multiplying by this qfactor and rounding.
      --threshold <min-max>   Set frequency coefficients outside of this absolute value range to zero. [default: 0-1]
      --coeff-limit <limit>   Limit output to only the top N frequency coefficients per block.
      -d, --dither            Apply 2D Floyd-Steinberg dithering to the high-precision transform products.
      --preserve-dc[=<type>]  Preserve the DC coefficient when applying a band pass filter with -p. [default: dc]
                              Type: dc, grey.
      --eval <expression>     Apply a formula to coefficients using FFmpeg's expression evaluator.
                              Provided arguments are coefficient "c" in a uniform range 0-1, indexes as "x", "y", "z", and "i" (color component), and dimensions "width", "height", "depth", and "components".
    
      --fftw-planning-method <m>  How thoroughly to plan the transform: estimate (default), measure, patient, exhaustive. Higher values trade startup time for transform time.
      --fftw-wisdom-file <file>   File to read accumulated FFTW plan wisdom from and save new wisdom to. Can be used to save startup time for higher planning methods for repeat block sizes.
      --fftw-threads <num>        Maximum number of threads to use for FFTW. [default: 1]
    
      -r, --framerate <rate>  Set the output framerate to this number or fraction (default: the input framerate).
      --keep-rate             If scaling in time with -s, retain the input framerate instead of scaling the framerate to retain the total duration. Ignored if --framerate is set.
      --samesize-chroma       If processing in a pixel format with subsampled chroma planes like yuv420p, chroma planes will use the same block size as the Y plane.
    
      --frames <limit>        Limit the number of output frames.
      --offset <pos>          Seek to this frame number in the input before processing.
    
      -c, --csp <optstring>   Option string specifying the pixel format and color properties to convert to for processing.
                              e.g. pixel_format=rgb24 converts the decoded input to rgb24 before processing.
      --iformat <fmt>         FFmpeg input format name (e.g. for pipe input).
      --format <fmt>          FFmpeg output format name. [default: selected by FFmpeg based on output file extension]
      --codec <enc>           FFmpeg output encoder name. [default: FFV1 or selected by FFmpeg based on output format]
      --encopts <optstring>   Option string containing FFmpeg encoder options for the output file.
      --decopts <optstring>   Option string containing FFmpeg decoder options for the input file.
      --loglevel <int>        Integer FFmpeg log level. [default: 16 (AV_LOG_ERROR)]

### Blocks
Processing takes place in terms of 3 dimensional blocks of arbitrary size, up to the full dimensions of the input. Individual dimensions for the blocksize and size arguments may be 0 to represent their parent coordinate – for example, a blocksize argument of 0x0x1 specifies a depth of 1 with the width and height of the input (this is the default.)  
To transform the entire input as a 3D volume use `-b 0x0x0`, but note that the full dimensions of the input must fit into memory as floating point values.  
If the input dimensions are not an integer multiple of the block size along any given axis, a warning will be printed and the input will be cropped/truncated to the next largest multiple.

### Output
If the output argument is a pipe (`-` or `pipe:`) and no format is specified with `--format`, motion defaults to yuv4mpeg output, which can be piped directly to mplayer/mpv or other tools.  
For non-pipe output, motion defaults to FFV1 as the output codec. If a format that doesn't support FFV1 is selected FFmpeg's default encoder for that format will be used instead.  
To view raw video output without potential colorspace or other color property conversions, `ffplay:` may be used to launch FFmpeg's ffplay utility and stream output to it.

## Examples

Perform a 3-dimensional analog to JPEG-style compression:
	
	motion --blocksize 8x8x8 --quant 20 --samesize-chroma <input> <output>

Watch a 2D spectrum of a video

	motion --spectrogram <input> ffplay:

Perform a linear 3D smoothing filter using FFmpeg's expression evaluator

	motion --blocksize 0x0x120 --eval 'c * ((width-x)/width) * ((height-y)/height) * ((depth-z)/depth)' <input> <output>

Perform a low pass filter along the time axis.

	motion --blocksize 1x1x0 --bandpass 0x0x0-0x0x20

# Rotate
Rotate video by right angles on a 3-dimensional axis.

This tool takes an input image or video and swaps its axes in 3 dimensions based on the mapping given.

## Usage

    Usage: rotate [options] [-]xyz <infile> <outfile>
    
      [-]xyz  How to rearrange the input dimensions, with -/+ to indicate direction.
    	       e.g. "zyx" swaps the x and z axis while "x-yz" results in a vertical flip.
    
      -h                  This help text.
      -s <start:nframes>  Starting frame number and total number of frames of input to use.
      -r <rational>       Output framerate or "same" to match input duration. [default: input rate]
      -q                  Don't print progress.
    
      -o <optstring>  Option string containing FFmpeg decoder options for the input file.
      -O <optstring>  Option string containing FFmpeg encoder options for the output file.
      -f <fmt>        FFmpeg input format name (e.g. for pipe input).
      -F <fmt>        FFmpeg output format name. [default: selected by FFmpeg based on output file extension]
      -c <optstring>  Option string specifying the pixel format and color properties to convert to for processing.
      -e <enc>        FFmpeg output encoder name. [default: FFV1 or selected by FFmpeg based on output format]
      -l <int>        Integer FFmpeg log level. [default: 16 (AV_LOG_ERROR)]


## Examples
Rotate a short video such that the time axis is now facing the viewer, with frames stacked as "slices" from left to right. If the dimensions of our input.avi are 1280x720, and it is 3000 frames long, the output will be 3000x720, and 1280 frames long. We can limit the number of frames, thus limiting the output width to 1920, so that it is fully visible on most monitors:

	rotate -s 0:1920 zy-x input.avi timeline.avi

Rotate the output of the previous example back to normal, perhaps after applying some filter to it to interesting effect

	rotate zyx timeline.avi original.avi

# Transcode
Basic transcoder meant for testing dspfun's FFmpeg API wrapper. Simply decodes and re-encodes frames in the given format at a constant framerate. It has no unique functionality over the above tools.

## Usage

	Usage: transcode <ffapi args> -r framerate -s start:frames input output

## Examples
Just convert an mp4 to y4m

	transcode input.mp4 output.y4m

Convert an avi to mp4, specifying encoding options

	transcode -e libx264 -O pixel_format=yuv444p:crf=16:preset=veryslow input.avi output.mp4