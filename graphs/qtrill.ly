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
  d'2
  r16 e' r e' r e' r e'
  f'2
  r16 e' r e' r e' r e'
}

scalelo = {
  r16 d' r d' r d' r d'
  e'2
  r16 f' r f' r f' r f'  
  e'2
}

scaleboth = {
  d'16 d' d' d' d' d' d' d' 
  eeh'16 eeh' eeh' eeh' eeh' eeh' eeh' eeh' 
  f'16 f' f' f' f' f' f' f' 
  eeh'16 eeh' eeh' eeh' eeh' eeh' eeh' eeh' 
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

