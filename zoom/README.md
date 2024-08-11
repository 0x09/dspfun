zoom - Interpolate images with a cosine basis at arbitrary scales/offsets.

This tool uses the continuous nature of the (i)DCT to perform interpolation on an input image.
It's meant to offer a look at how these behave when sampled in between and outside of the integer grid.

# Usage
    Usage: zoom [options] <input> <output>
    
      -h, --help  This help text.
      -s <scale>  Rational or decimal scale factor. May be a single value or XxY to specify horizontal/veritcal scaling factors.
      -r <res>    Logical resolution in the form WxH. May be fractional. Takes precedence over -s.
      -p <pos>    Floating point offset in image, in the form XxY (e.g. 100.0x100.0). Coordinates are in terms of the scaled output unless -P is set
      -v <size>   Output view size in WxH.
      -c          Anchor view to center of image
      -P          Position coordinates with -p are relative to the input rather than the scaled output
      -%          Position coordinates with -p are a percent value rather than a number of samples
      -g          Scale in linear RGB
      -q          Don't output progress
    
      --showsamples[=<type>]  Show where integer coordinates in the input are located in the scaled image when upscaling.
                              type: point (default), grid.
    
      --basis <type>  Set the boundaries of the interpolated basis functions. [default: interpolated]
                      type:
                        interpolated: even around half of a sample of the scaled output
                        native: even around half of a sample of the input before scaling
                        centered: the first and last samples of the input correspond to the first and last samples of the output
    
    animation options:
      -n <frames>  Number of output frames [default: 1]
      -x <expr>    Expression animating the x coordinate
      -y <expr>    Expression animating the y coordinate
      -S <expr>    Expression animating the overall scale factor
      -X <expr>    Expression animating the horizontal scale factor (if different from -S)
      -Y <expr>    Expression animating the vertical scale factor (if different from -S)
    
    ffmpeg options:
       --ff-format <avformat>  output format
       --ff-encoder <avcodec>  output codec
       --ff-rate <rate>        output framerate
       --ff-opts <optstring>   output av options string (k=v:...)
       --ff-loglevel <-8..64>  av loglevel
    


## Animation
zoom produces still image output by default, but can animate output (e.g. pans, zooms) using expressions interpreted by [FFmpeg's expression evaluator](https://www.ffmpeg.org/ffmpeg-utils.html#Expression-Evaluation).  

Expressions are evaluated for each frame of output and determine that frame's coordinate offsets and scale factors.

Values available for use in expressions:

* `i`: The current frame index.
* `n`: The total number of frames (defined by the `-n` option).
* `w`, `h`: The original input image dimensions.
* `vw`, `vh`: The output view dimensions.
* `x`, `y`: The current coordinates (scaled). The initial value of these is set by the `-p` option if provided.
* `xs`, `ys`: The current vertical and horizontal scale factors. The initial value of these is set by `s` option if provided.

Scaling expressions are evaluated first, so when evaluating coordinates with `-x` and `-y` the values of `xs` and `ys` reflect the current frame's scale factor.

Note that unlike coordinates specified by `-p` where units may be modified by other options, coordinate values for expressions are always in terms of scaled logical pixels.

# Examples
Animate a zoom from 1/4x to 4x keeping coordinates centered:

    zoom -n 600 -S '0.25+3.75*(i/n)^2' -x '(w*xs-vw)/2' -y '(h*ys-vh)/2' flower.png zoom.avi

[![Flower 1/4 to 4x](https://0x09.net/i/g/flower_zoom.png)](https://0x09.net/i/g/flower_zoom.mp4)  
<sub>[(Link to motion output.)](https://0x09.net/i/g/flower_zoom.mp4)</sub>

Generate an animated subpixel pan across a single logical pixel:

    zoom -n 600 -x 'i/n' image.png pan.avi

The same pan but iteratively referencing the current x coordinate:

    zoom -n 600 -x 'x+1/n' image.png pan.avi
