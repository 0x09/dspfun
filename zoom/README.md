zoom - Interpolate images with a cosine basis at arbitrary scales/offsets.

This tool uses the continuous nature of the (i)DCT to perform interpolation on an input image.
It's meant to offer a look at how these behave when sampled in between and outside of the integer grid.

# Usage
	Usage: zoom [options] <input> <output>
    
      -h, --help  This help text.
      -s <scale>  Rational or decimal scale factor.
      -p <pos>    Floating point offset in image, in the form XxY (e.g. 100.0x100.0). Uses scaled output coordinates unless -P is set
      -v <size>   Output view size in WxH.
      -c          Anchor view to center of image
      -P          Translate position coordinates into scaled space
      -g          Scale and output in linear RGB
    
      --showsamples[=<type>]  Show where integer coordinates in the input are located in the scaled image when upscaling.
                              type: point (default), grid.
    
      --basis <type>  Set the boundaries of the interpolated basis functions. [default: interpolated]
                      type:
                        interpolated: even around half of a sample of the scaled output
                        native: even around half of a sample of the input before scaling
                        centered: the first and last samples of the input correspond to the first and last samples of the output

# Example
Interpolate [Kodak parrot's](https://r0k.us/graphics/kodak/kodak/kodim23.png) eye 20x

	zoom -s20x20 -p 10000x3200 -v 512x512 kodim23.png zoomed.png
	
![Zoom](https://0x09.net/i/g/zoom.png)