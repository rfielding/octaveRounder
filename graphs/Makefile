#dot comes from the graphviz package
#convert comes from the imagemagick package
none:

%:	%.dot
	dot -Tpng -o ../images/$@.png $@.dot

%:	%.ly
	lilypond -f png -danti-alias-factor=10 $@.ly
	convert $@.png -trim $@_cropped.png
	rm $@.png
	mv $@_cropped.png ../images

