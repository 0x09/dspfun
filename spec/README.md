spec - Generate invertible DCT frequency spectrums for viewing and editing.

# Usage
	spec -g -c csp -t (abs|shift|flat|sign) -R (one|dc|dcs) -T (linear|log) -S (abs|shift|saturate) -G (native|lenna|custom(float)) <infile> <outfile>
	ispec -g -c csp -t (abs|shift|flat|sign) -R (one|dc|dcs) -T (linear|log) -S (abs|shift|saturate) -G (native|lenna|custom(float)) -p -m <signmap> <infile> <outfile>

## Spectrogram types

Presets (-t):

* abs - Log-scaled absolute value spectrum, common for viewing. Invertible with a corresponding sign spectrum. (`-Rdc -Tlog -Sabs -Gnative`)
* shift - Log-scaled spectrum shifted into the 0-1 range, suitable for editing and inverting even at low bitdepths. (`-Rone -Tlog -Sshift -Gnative`)
* flat - Linear-scaled shifted spectrum for editing with higher-bitdepth editors. (`-Rone -Tlinear -Sshift -G1`)
* sign - Sign map. Can be provided to ispec to invert an `abs` spectrum. (`-Rone -Tlinear -Ssat -G1`)

## Other options
-c: Color planes to operate on (R/G/B)

-g: Operate in linear RGB colorspace.

# Examples

Generate a traditional Lenna spectrogram:

	spec Lenna.png Lenna.spec.png

![Lenna spectrogram](http://0x09.net/i/g/Lenna.spec.png)

`spec` can be used as a filter, particularly when piping with imagemagick. This allows you to perform e.g. sinc resizes using crop/expand filters:

	spec -tflat image.png | convert - -crop 32x32+0+0 - | ispec -tflat | display
	
Or a smooth low-pass filter using the gradient tool of an image editor:
	
	spec -tshift kodim23.png | convert - -depth 8 /tmp/kodim23.png
	<edit>
	ispec -tshift /tmp/kodim23.png | display

![Gradient lowpass](http://0x09.net/i/g/gradlp.png) ![Cover image](http://0x09.net/i/g/smoothpass.png)

Draw into the absolute value frequency spectrum of an image:

	spec -tabs kodim23.png | convert - -depth 8 /tmp/kodim23.png
	<edit>
	ispec -tabs /tmp/kodim23.png -m<(spec -tsign kodim23.png) | display

![Hidden message](http://0x09.net/i/g/hidden.png) ![Cover image](http://0x09.net/i/g/cover.png)
