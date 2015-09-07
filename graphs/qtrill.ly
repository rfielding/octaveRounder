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
  d'4
  r16 e' r e'
  f'4
  r16 e' r e'
}

scalelo = {
  r16 d' r d'
  e'4
  r16 f' r f'
  e'4
}

scaleboth = {
  d'16 d' d' d'
  eeh'16 eeh' eeh' eeh'
  f'16 f' f' f'
  eeh'16 eeh' eeh' eeh'
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

