zoom - Interpolate images with a cosine basis at arbitrary scales/offsets.

This tool is not suitable for processing large images wholesale but is geared toward extracting moderately sized segments at potentially very high zoom levels.

# Usage
	zoom -s scale -p pos -v viewport --basis=interpolated,centered,native --showsamples=1(point),2(grid) -cgP input output

	-s WxH - Rational or floating point scale factor.
	-p XxY - Floating point position in *scaled* image to start the viewport (unless -P).
	-v WxH - Size of view into *scaled* image / output size.
	-c - Anchor viewport to center of image
	-P - Translate position coordinates into scaled space
	-g - Scale and output in linear RGB
	--showsamples - Draw a grid on integer samples as if looking at a graphing calculator.
	--basis - Set the boundaries of the interpolated basis functions
	          interpolated: even around the scaled half sample (default)
	          native: even around the unscaled half sample
	          centered: first and last samples of input correspond to first and last samples output

# Example
Interpolate [Kodak parrot's](https://r0k.us/graphics/kodak/kodak/kodim23.png) eye 20x

	zoom -s20x20 -p 10000x3200 -v 512x512 kodim23.png zoomed.png
	
![Zoom](https://0x09.net/i/g/zoom.png)