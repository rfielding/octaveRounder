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

arpd = {
  d8 f a  d f a  d a f  d a f  d
}
arpdr = {
  d8 f a  d,_\upo f a  d,_\upo a'_\downo f  d a'_\downo f  d
}

result = \new PianoStaff {
  \time 6/8
  \autochange \relative c {
    \arpd
  }
}

\relative c' {
  \time 6/8
  \arpdr
}
\result
