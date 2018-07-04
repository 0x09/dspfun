scan - Progressively reconstruct images using various frequency space scans.

![Flower image part way through a radial scan|http://0x09.net/i/g/scan/flower_radial.mp4](http://0x09.net/i/g/scan/flower_radial.png "Flower image part way through a radial scan")

Flower image part way through a radial scan, showing intermediates. [Link to full sequence](http://0x09.net/i/g/scan/flower_radial.mp4).

# Usage
	usage: scan <options> input output
	options:
	   -h|--help
	   -H|--fullhelp
	   -m|--method <name>                scan method
	   -o|--options <optstring>          scan-specific options
	   -v|--visualize                    show scan in frequency-space
	   -s|--spectrogram                  show scan over image spectrogram (implies -v)
	   -i|--intermediates                show transform intermediates for current index (stacks with -v/-s)
	   -M|--max-intermediates            use full range for transform intermediates (implies -i)
	   -S|--step <int>                   number of scan iterations per frame of output
	   -I|--invert                       invert scan order
	   -n|--frames <int>                 limit the number of frames of output
	   -O|--offset <int>                 offset into scan to start at
	      --skip                         don't fill previous scan indexes when jumping to an offset with --offset
	   -g|--linear                       operate in linear light
	   -p|--pruned-idct <bool>           use built-in pruned idct instead of fftw, faster on small scan intervals (default: auto based on scan interval)
	   -f|--serialization-file <path>    serialize scan to file
	   -t|--serialization-format <fmt>   scan format to serialize (with -f)

	ffmpeg options:
	   --ff-format <avformat>   output format
	   --ff-encoder <avcodec>   output codec
	   --ff-rate <rate>         output framerate
	   --ff-opts <optstring>    output av options string (k=v:...)
	   --ff-loglevel <-8..64>   av loglevel

	spec options:
	   --spec-gain <float>       spectrogram log multiplier (with -s)
	   --spec-opts <optstring>   spectrogram options string (k=v:...) (with -s)

	scan methods   - options
	   horizontal
	   vertical   
	   zigzag     
	   random      - optional seed (int)
	   row        
	   column     
	   diagonal   
	   mirror     
	   box        
	   ibox       
	   radial      - optional rounding mode (tonearest, upward, downward, system)
	   iradial     - optional rounding mode (tonearest, upward, downward, system)
	   magnitude   - optional quantization factor (float)
	   evalxy      - expression satisfying index = f(x,y)
	   evali       - expressions satisfying x = f(i,width,height); y = f(i,width,height)
	   file        - filename
	   precomputed - method:method options

	serialization formats:
	   index
	   coordinate

	spectrogram option string keys and values:
	   preset = abs, shift, flat, signmap
	   scale = linear, log
	   sign = abs, shift, saturate

# Scans
Labels below link to examples (capped to 2400 frames) with the exception of the eval and file methods.

 | | | 
-|-|-|-
[horizontal](http://0x09.net/i/g/scan/horizontal.mp4)|[vertical](http://0x09.net/i/g/scan/vertical.mp4)|[zigzag](http://0x09.net/i/g/scan/zigzag.mp4)|[diagonal](http://0x09.net/i/g/scan/diagonal.mp4)
![horizontal](http://0x09.net/i/g/scan/horiz.png "horizontal")|![vertical](http://0x09.net/i/g/scan/vert.png "vertical")|![zigzag](http://0x09.net/i/g/scan/zigzag.png "zigzag")|![diag](http://0x09.net/i/g/scan/diag.png "diag")
[row](http://0x09.net/i/g/scan/row.mp4)|[column](http://0x09.net/i/g/scan/column.mp4)|[box](http://0x09.net/i/g/scan/box.mp4)|[ibox](http://0x09.net/i/g/scan/ibox.mp4)
![row](http://0x09.net/i/g/scan/row.png "row")|![column](http://0x09.net/i/g/scan/col.png "column")|![box](http://0x09.net/i/g/scan/box.png "box")|![ibox](http://0x09.net/i/g/scan/ibox.png "ibox")
[radial](http://0x09.net/i/g/scan/radial.mp4)|[iradial](http://0x09.net/i/g/scan/iradial.mp4)|[mirror](http://0x09.net/i/g/scan/mirror.mp4)|[magnitude](http://0x09.net/i/g/scan/magnitude.mp4)
![radial](http://0x09.net/i/g/scan/radial.png "radial")|![iradial](http://0x09.net/i/g/scan/iradial.png "iradial")|![mirror](http://0x09.net/i/g/scan/mirror.png "mirror")|![magnitude](http://0x09.net/i/g/scan/magnitude.png "magnitude")
[random](http://0x09.net/i/g/scan/random.mp4)|evali|evalxy|file
![random](http://0x09.net/i/g/scan/random.png "random")|![evali](http://0x09.net/i/g/scan/evali.png "evali")|![evalxy](http://0x09.net/i/g/scan/evalxy.png "evalxy")|![file](http://0x09.net/i/g/scan/file.png "file")

# Examples
Perform a classic zigzag scan and display the output with `ffplay`:

`scan --method zigzag flower.png ffplay:`

Generate the radial sequence shown above with spectrogram and intermediates:

`scan --method radial --spectrogram --intermediates flower.png flower.avi`

Do the same using a shifted spectrogram for visualization:

`scan --method radial --spectrogram --intermediates --spec-opts preset=shift flower.png flower.avi`

Scan by coefficient magnitude, grouping values by rounding to 1/100000:

`scan --method magnitude --options 100000 flower.png flower.avi`

Use ffmpeg's expression evaluator with coordinate input to create a custom scan function:

`scan --method evalxy --options 'bitand(x,y)' flower.png flower.avi`

Use ffmpeg's expression evaluator with index input to reproduce the "horizontal" scan method:

`scan --method evali --options 'mod(i,height); floor(i/height)' flower.png flower.avi`

# Serialization
Scans may be serialized to plaintext in one of two self-describing formats, which may then be read back using the `file` scan method.

Example of each format using an 8x8 diagonal scan:

## index
Values are indexes, coordinates are encoded positionally (row x col).

`scan -m diag --serialization-format index --serialization-file /dev/stdout 8x8.png`
```
0  1  2  3  4  5  6  7
1  2  3  4  5  6  7  8
2  3  4  5  6  7  8  9
3  4  5  6  7  8  9 10
4  5  6  7  8  9 10 11
5  6  7  8  9 10 11 12
6  7  8  9 10 11 12 13
7  8  9 10 11 12 13 14
```

## coordinate
Values are coordinates, indexes are encoded positionally (interval x index).

`scan -m diag --serialization-format coordinate --serialization-file /dev/stdout 8x8.png`
```
0,0
0,1 1,0
0,2 1,1 2,0
0,3 1,2 2,1 3,0
0,4 1,3 2,2 3,1 4,0
0,5 1,4 2,3 3,2 4,1 5,0
0,6 1,5 2,4 3,3 4,2 5,1 6,0
0,7 1,6 2,5 3,4 4,3 5,2 6,1 7,0
1,7 2,6 3,5 4,4 5,3 6,2 7,1
2,7 3,6 4,5 5,4 6,3 7,2
3,7 4,6 5,5 6,4 7,3
4,7 5,6 6,5 7,4
5,7 6,6 7,5
6,7 7,6
7,7
```
