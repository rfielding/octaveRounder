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

scalehi = {
  \time 6/8
  d'8 r f' g' f' r d'
}

scalelo = {
  \time 6/8
  r8 e' r r r e' r
}

scaleboth = {
  \time 6/8
  d'8 eeh' f' g' f' eeh' d'
}

\new PianoStaff \with {
  instrumentName = #"input"
}
<<
  \new Staff = "scalehi" \scalehi
  \new Staff = "scalelo" \scalelo
>>

\new Staff \with {
  instrumentName = #"output"
}
\scaleboth

