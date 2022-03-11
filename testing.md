- set DEBUG flag to 1
- make
- run `./pnt-lut pgm pnt /dev/stdout | hd`
- open another shell (or run previous command in background) and run `./test
  pnt`

will generate random tablet coordinates, feed to pnt-lut, and print info.
decent smoke test.

note: added delay between sending each coordinate to mimic issue we had with
ascii converter
