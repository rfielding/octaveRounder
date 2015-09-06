\version "2.16.2"

\header {
  tagline = ""
}

upo = \markup {
  \arrow-head #Y #UP ##t
}
downo = \markup {
  \arrow-head #Y #DOWN ##t
}

\new Staff \with {
  instrumentName = #"Bayati on D"
}
\relative {
  d8 (eeh f) g f (eeh d) c d
}

\new Staff \with {
  instrumentName = #"Rast on D"
}
\relative {
  d8 e (fih g) a g (fih d16 ) c16 d8
}

\new Staff \with {
  instrumentName = #"A flat"
}
\relative {
  \time 3/4
  aes'4 (beseh ces) 
  deses8 
  ces16 deses
  ces16 deses
  ces16 deses
  ces16 deses
  ces16 deses
}

