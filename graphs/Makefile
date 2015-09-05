#dot comes from the graphviz package
#convert comes from the imagemagick package

all:	wiring play qtone trill

wiring:	wiring1.dot
	dot -Tpng -o ../images/wiring1.png wiring1.dot
play:	play.ly
	lilypond -f png play.ly
	convert play.png -trim play_cropped.png
	mv play_cropped.png ../images
qtone:	qtone.ly
	lilypond -f png qtone.ly
	convert qtone.png -trim qtone_cropped.png
	mv qtone_cropped.png ../images
trill:	trill.ly
	lilypond -f png trill.ly
	convert trill.png -trim trill_cropped.png
	mv trill_cropped.png ../images
