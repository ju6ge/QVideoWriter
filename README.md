QVideoWriter
============

This is a small project to enable writing a Video file from a series of
QImages. It was created because using using QtFFmpegWrapper did not 
produce satisfying results.

It is intended to be easily to use it other project and builds as a static
library. And is based on the ffmpeg encode_video example.

# Building

Using cmake:
``` code
mkdir build
cd build
cmake ..
make
```

