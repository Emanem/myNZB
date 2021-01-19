# myNZB

A lightweight NZB downloader (originally hosted [here](https://mynzb.youlink.org/))

## What's this myNZB

myNZB is a simple [NZB](https://en.wikipedia.org/wiki/NZB) client; its main purpose is not to be a flashy super-duper 1000 features NZB software but merely the kind of software you invoke like this:
```myNZB -s my.nntp.server file1.nzb file2.nzb ... fileN.nzb```
and then, magic, it will donwload the content of the NZB.

## Features

Naturally it has some nice features:

* Multithread download (you can set how many threads/connections it can use)
* Uses a connection pool (tries to minimize TCP/IP traffic)
* Automatically retries to retrieve failed files
* Uses libnotify to signal the progress of NZB files (dynamically - no need to install it to run myNZB)
* Written in C++
* Depends only on yydecode and libmlx2 packages/libraries

And that's it.
You'd say: is not that much!, but think... what do you need an NZB client for? Donwload NZB and notify what is going on.
_Easy_!

## How to use it

```
Usage: myNZB [options] file0.nzb file1.nzb ...
	-s: NNTP server to connect (use the colon notation to specify the port, eg. "nntp.server.com:119")
	-u: NNTP username (default none, eg. "")
	-p: NNTP password (default none, eg. "")
	-c: number of parallel connections to NNTP server (default 2)
	-t: wait time (ms) in between dowloading NZB segments (default 0)
	-y: yydecode path (default to: "/usr/bin/yydecode")
	-o: output directory (default "./")
	-d: create a subdirectory with NZB file name for each NZB to process (default not set)
	-x: percentage value to allow retry download segments with errors (default 0.2)
	-r: reset TCP/IP connections with NNTP server for each segment
	-n: do NOT notify file activity through libnotify (dynamically loaded)
	-l: set log level:
		0 No log
		1 Errors only
		2 Warnings and errors
		3 Info, warnings and errors (default)
		4 Debug, info, warnings and errors
	-z: NZB segments max retries (default 3 retries)
	-C: set wait time (ms) when creating new NNTP connections (default 0, no timeout)
	-e: use SSL connections
	-F: set to force writing into temporary files instead of memory (default not set)
	-P: (try to) skip the ".par2" files
	-T: set temporary directory (default "./")
	-S: set recv timeout (msec) on NNTP connections (default 2000 , 2 sec)
	-W: specify a port:password (optional) for the report HTTP web server (default 0, no web server)
	-Y: strict search for yEnc encoding in NNTP subject (default false)
	-h: print this help and exit
	-v: print version and exit
```

## How to build

Requires following dev libraries to be installed: `ssl`, `xml2` and `crypto`.

## Hints

* When using _-T_ specify `/dev/shm` to write tmp files in memory, not on disk (less disk usage)
* Specify the _-P_ flag to skip the par2 files; usually those are not needed

## ChangeLog

* _0.3.0_
  - ADD usage of SSL conections (and verification) through openssl
* _0.2.4_
  - ADD ability to save files in subdirectories
  - FIX (www.cpp) web server now uses password instead of directory
* _0.2.3_
  - ADD ability to skip par files
  - ADD memory manager to optimize mem allocation for buffers
  - ADD log debug mode (default is disabled)
  - FIX (main.cpp) now properly try to parse all the NZB files before throwing any error
  - FIX (notify.cpp) restored original notifications
  - FIX (nzb.cpp) now managers/pools/singletons are properly initialized in ST segment of code
  - FIX (nzb.cpp) now fetch_body/fetch_article/dot_unstuff properly terminate the buffer with '\0'
* _0.2.2_
  - ADD report HTTP web server (useful if you want to see what is going on from your phone/away)
  - FIX (nzb.cpp) remove in auto_filer destroyer was calling its method instead of ::remove
  - FIX (connection.cpp) close socket only when valid
  - FIX (notify.cpp) now notifications are being updated; 2 after other very fast will be overwritten (you'll se only last one) but should not have memleaks
* _0.2.1_
  - ADD option to set NNTP connections timeout
* _0.2.0_
  - First version

## Known issues

* Google Chrome is not able to render the progress bar when the web report is being used. A [bug](https://bugs.chromium.org/p/chromium/issues/detail?id=34329) has been opened. I hope they'll fix this ASAP.
* Google Chrome doesn't refresh the page when Refresh button is being hit. This is actually a defect of Chrome (another sigh); then next revision of myNZB will try to cope with this Chrome defect. For now, with Chrome, just reload the page without hitting refresh.
* Some memory is could be leaking due to libnotify usage (I have to investigate)

## F.A.Q.

_Why did you develop it?_
I was using LottaNZB but that software realies on HellaNZB that with my ISP does suck (a lot of corrupted/unfinished files), so began with the purpose to understand what was going wrong with this software and my ISP. I first developed four stupid lines of code, then I've put them together a little bit better and finally I realized I was going to incorporate some other cool features so...I ended up polishing the code and releasing it.

_Why C++ and not ...?_
Because C++ allows you to have some kind of flexibility, compactness and speed that many other languages don't offer. Plus I managed to recicle a lot of classes I already wrote for other projects.

_Some operations/classes aren't very clear to me._
Don't worry send me an email and I'll explain why...if the comment is interesting I'll post on the website.
