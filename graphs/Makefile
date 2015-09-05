all:	wiring play

wiring:	wiring1.dot
	dot -Tpng -o ../images/wiring1.png wiring1.dot
play:	play.ly
	lilypond -f png play.ly
	convert play.png -trim play_cropped.png
	mv play_cropped.png ../images
