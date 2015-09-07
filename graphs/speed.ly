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

runraw = {
  g'16 b' c'' d''
  g'_\upo b' c'' d''
  g'_\upo d''_\downo c'' b'
  g' d''_\downo c'' b'
}

run = {
  g16 b c d
  g b c d
  g d c b
  g d c b
}

\new Staff \with {
  instrumentName = #"input"
} {
  \repeat volta 2
  \runraw
}

\new PianoStaff \with {
  instrumentName = #"output"
} {
  \repeat volta 2
  \autochange \relative {
    \run
  }
}


