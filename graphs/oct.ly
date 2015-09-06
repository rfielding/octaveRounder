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

scalelo = {
  \time 4/4
  d'8 d'''_\downo d'_\upo c'''_\downo d'_\upo d'''_\downo d'_\upo c'''_\downo 
}

scaleboth = {
  \time 4/4
  d'8 d''8 d'8 c''8
  d'8 d''8 d'8 c''8
}

\new Staff \with {
  instrumentName = #"input"
}
\scalelo

\new Staff \with {
  instrumentName = #"output"
}
\scaleboth


