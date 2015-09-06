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
  \time 4/4
  d'1
}

scalelo = {
  \time 4/4
  r8 d''8_\downo r8 ees''8 r8 d''8 r8 ees''8 
}

scaleboth = {
  \time 4/4
  d'8 d'8 d'8 ees'8
  d'8 d'8 d'8 ees'8
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

