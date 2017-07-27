zoom - Interpolate images with a cosine basis at arbitrary scales/offsets.

This tool is not suitable for processing large images wholesale but is geared toward extracting moderately sized segments at potentially very high zoom levels.

# Usage
	zoom -s scale -p pos -v viewport --basis=interpolated,centered,native -c --showsamples=1(point),2(grid) input output

	-s WxH - Rational or floating point scale factor.
	-p XxY - Floating point position in *scaled* image to start the viewport.
	-v WxH - Size of view into *scaled* image / output size.
	--showsamples - Draw a grid on integer samples as if looking at a graphing calculator.
	--basis - Set the boundaries of the interpolated basis functions
	          interpolated: even around the scaled half sample (default)
	          native: even around the unscaled half sample
	          centered: first and last samples of input correspond to first and last samples output

# Example
Interpolate [Kodak parrot's](http://r0k.us/graphics/kodak/kodak/kodim23.png) eye 20x

	zoom -s20x20 -p 885x850 -v 512x512 kodim23.png zoomed.png
	
![Zoom](http://0x09.net/i/g/zoom.png)