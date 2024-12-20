## Vencode
### Convert any file to video.

![preview](assets/preview.gif)
### Dependencies to install
	* libpng
	* ffmpeg
	* zlib

### Introduction
Convert any file to video... You can upload it to YouTube and get infinite storage...

This program reads the bytes of any file and prints the binary string into PNG files, then groups them into a video using FFMPEG.

For convert a folder, just archive it (with zip, tar, 7z or anything else).

vencode has some options, you can choose the resolution, bit size, framerate and code for FFMPEG video encoding, and multi thread PNG writing for improved speed.

You can choose the number of writing threads for more speed, I recommend setting the number of your CPU cores as thread quantity. 

I strongly recommend compress the files before conversion.

Example:

`$ ./vencode -e files.zip -o files.zip.mp4 -t 4`

With above example, you get a video "files.zip.mp4" which contains all data of files.zip intact. (the -t 4 is the threads number, assuming you have a CPU with 4 cores)

The default resolution is 1920x720, with a bit size of 4 pixels and 30fps. With these settings you can upload your encoded files to YouTube.

You can read more about the options typing:

`$ ./vencode -h`

### Build
For any *NIX system:

`$ make`

`$ sudo make install`
 
### Multi thread writing
A fixed thread count is not efficient because many devices have a different CPU power and cores, so you can choose any value, in my case, setting the double of my CPU cores as thread count works fine. However, you can experiment with setting a higher value for improved speed, but be careful, the result may be a slower process if you set too many threads for your CPU.

### Process speed and output video file size
If you want to convert a file to video, you can set some options in this progam for improve speed:

- Set thread number to appropriate value.

- Set the '-c libx264' argument. You can set the video codec with -c  argument, video encoding with libx264 is faster than libx265.

- Set the '-z' option to enable "ultrafast" preset in FFMPEG.

If you use libx264 codec with "ultrafast" preset, the file size of video will be more larger (maybe 8-9 times larger than original file).

If you want to save space, use the '-c libx265' option without the '-z' argument, the file size will be more lightweight, (maybe 4-5 times more larger than original file). If you do that, the process will be slower.
