# Dynamic Universal Music Bibliotheque (libdumb)

## v2.0.0, not released yet

* Memory leak and bug fixes
* Audio playback quality improvements for STM
* Added support for FEST MOD files
* Default resampling quality is now cubic
* Allegro4 support
* New dumbplay, dumbout examples
* Multiple cmake fixes
* duh_render() deprecated. Instead, there are new duh_render_float() 
  and duh_render_int() API functions.

## v1.0.0, released 17 January 2017

* Support newer compilers
* Better audio playback quality
* More supported formats
* SSE optimizations support
* CMake support
* New resamplers
* Seek support
* Fixes, cleanups, speedups.

## v0.9.3, released 7 August 2005

Hello! Welcome to a long-awaited-or-probably-just-given-up-on-by-everybody
release! New to this release are lower memory usage, faster mixing loops,
loading of text fields in the module files, and faster load functions for
projects that don't need to seek within the module or know its length.
Additionally, Chad Austin has contributed a dumb2wav tool for converting
modules to .wav files and updated the Visual Studio 6 project files to
compile all the examples as well as the library. Users of Unix-like systems
will be pleased to know that on Chad's suggestion I have made the build
system cope with variables such as $HOME or ${HOME} in the prefix.

Chad has also contributed an Autotools build system, but neither of us
recommends its use. The Autotools are an evil black box, we haven't quite
managed to get it right, and goodness help you if it happens not to work for
you. The files are available in a separate download if you absolutely need
them. Notice that that download is almost twice as large as the rest of DUMB!

Maybe we'll do SCons next time.

Thanks to Chad for all his work. Chad is the author of Audiere, a portable
sound library which has started using DUMB for its module playback! Wahoo!

   http://audiere.sf.net/

There are three main optimisations that went into the mixing loops.

First, I downloaded ModPlugXMMS and had a peek at the mixing code, which is
Public Domain. It uses look-up tables for the cubic mixing. I pinched the
idea, and that sped DUMB's cubic (best quality) resamplers up by a factor of
two or three.

Secondly, the samples loaded from the file are now kept in 8-bit or 16-bit
format, whereas previously they were being converted to 24-bit-in-32-bit on
loading. This means the samples occupy a half or a quarter of the memory they
used to occupy. It also had the side-effect of speeding up the mixing loops,
but it meant I had to duplicate the resampling code. (It is all done with
macros in the source code, but it becomes two copies on the binary level.)

Secondly, stereo samples and stereo mixing buffers are now kept in
interleaved format, where previously the two channels were done separately to
keep the code simpler. This change has made the library quite a bit bigger,
but has made the code run almost twice as fast for stereo output (tested for
modules whose samples are mostly mono)!

DUMB is now as fast as ModPlugXMMS on my system.

Some people have also commented that DUMB seems to take a long time loading
files. This is because immediately upon loading the file it runs the playback
engine over it up as far as the point of first loop, taking snapshots at 30-
second intervals to be used as references for fast seeking and finally
storing the playback time. Of course, most games don't need this. You can now
skip it by calling the _quick versions of the dumb_load_*(), dumb_read_*() or
dumb_register_dat_*() functions. Should you need the data later, you can call
dumb_it_do_initial_runthrough() to calculate it. Please note that this cannot
currently be done safely from a concurrent thread while the music is playing.

As mentioned, DUMB loads the text fields in module files now. You can
retrieve the song title with duh_get_tag(). Sample names and file names and
instrument names and filenames, and the song message for IT files, are
available with a call to duh_get_it_sigdata() and various dumb_it_sd_*()
functions. Please note that text fields added as extensions by ModPlug
Tracker are not supported.

DUMB's timing is ever so slightly more accurate. This is hardly noticeable,
but it has meant that the length computed will increase very slightly.

There are many small playback fixes in this release:

* The Lxx effect in XM files (set envelope position) is now supported.

* Pattern looping is now correct for XM files. Bizarrely, an ordinary pattern
  loop whose start point isn't the first row seems to cause the next pattern
  to start at the row corresponding to the loop start point. That must have
  been a headache for people creating XM files! Nevertheless, DUMB now
  emulates this behaviour. If you have an XM file that was written in a
  tracker other than Fast Tracker II and breaks in DUMB, you can get around
  it by putting a D00 effect (break to row 0) right at the end of the pattern
  containing the loop.

* XM pattern looping can be infinite. DUMB should detect this and call the
  loop callback when it happens. Specifically, it has a loop counter for each
  channel, so each time it sets or decrements that counter, it remembers the
  loop end point for that channel. When the loop terminates, the loop end
  point is reset to 0. If the loop end point ever decreases during a loop,
  the loop callback is called. If anyone manages to get around this check and
  prevent DUMB from calling the callback, please let me know and send me an
  illustrative XM file!

* For IT files, notes are now removed from channels if they have faded out,
  even if they are still in the foreground. After this has happened, a row
  with a note and Gxx (tone portamento) specified will cause a new note to
  start playing, which is what Impulse Tracker does in this scenario.
  (Normally, Gxx prevents the new note from playing and instead causes the
  old note to start sliding towards the new note.)

* If a tone portamento command occurred when no note was playing, the effect
  value wasn't stored. This has been fixed. Thanks to Maim from #trax on
  EFnet for discovering this bug.

* DUMB now treats the parameter to the undocumented XM key off effect Kxx as
  a delay, consistent with Fast Tracker II's behaviour. It has also been made
  not to clear the note, so a subsequent volume command will restore it, as
  in Fast Tracker II.

* DUMB used to process the first row when you created the
  DUMB_IT_SIGRENDERER. This happened before you had a chance to install any
  callbacks. If an F00 effect occurred on the first row, the music would stop
  immediately and the xm_speed_zero callback would be called if it were
  present. Unfortunately, it wasn't present, and the algorithm for
  calculating the length subsequently went into an endless loop while waiting
  for it. Worse still, the same algorithm accumulated data for fast seeking,
  and never stopped, so it pretty quickly consumed all the resources. DUMB
  will now not process the first row until you first request some samples,
  provided you pass zero for pos. Of course, any MOD or XM file with F00 in
  the very first row won't do much anyway, but such files won't crash the
  library now.

* There was a subtle bug that affected a few XM files. For instruments with
  no associated samples, the array mapping notes to samples is uninitialised.
  This became a problem if such instruments were then used, which does happen
  sometimes. On many systems, memory is initialised to zero when first given
  to a program (for security reasons), so the problem didn't come up most of
  the time. However, on platforms like DOS where memory isn't initialised, or
  in programs that reuse memory later on (this includes the XMMS plug-in with
  which I discovered the bug), a rogue note would occasionally play. This has
  now been fixed.

* DUMB's envelope handling for IT files was subtly wrong. Upon note off, it
  stopped obeying the sustain loop points one tick too early. Notes were
  often shorter than they should have been, and in pathological cases a whole
  extra iteration of the sustain loop section might have been skipped. The
  envelope code has now been rewritten. Thanks go to Allefant for Valgrinding
  the new code!

Finally, there were two build problems in the last version, which were fixed
in the download marked with -fixed. They are of course correct in this
version. For the record:

* The make/config.bat file, responsible for generating make/config.txt, wrote
  a crucial line to the wrong place, causing it to be left out of the file.
  As a result, the makefile would fail to install everything for Allegro
  users, and enter infinite recursion for other users. This applied to people
  using DJGPP and MinGW.

* DUMB's Makefile was supposed to install the example programs on Unix-based
  platforms, but it wasn't doing. The fix was to edit Makefile and change the
  one occurrence of $COMSPEC to $(COMSPEC).

That's it! I hope you enjoy this long-awaited-or-probably-just-given-up-on-
by-everybody release of DUMB!


## v0.9.2, released 2 April 2003

Yes, there really has been a release. This is not a day-late April fools'
joke.

DUMB's full name has changed! The old "Dedicated Universal Music
Bastardisation" was rather silly, and not much more than a forced attempt at
finding words beginning with D, U, M and B. I spent weeks and weeks browsing
dictionaries and hopelessly asking others for bright ideas, until the
brilliant Chris "Kitty Cat" Robinson came up with "Dynamic". I decided to
keep the U as Universal, since a DUH struct can hold digital music in any
format. Now all that remained was the B, but it didn't take me long to come
up with Bibliotheque, which, despite looking French, is indeed considered an
English word by Oxford English Dictionary Online, to which my university has
a subscription. So there you have it - the name now makes sense.

The two most significant additions to the project would have to be the new
thread safety (with an important restriction, detailed in docs/dumb.txt), and
the new build system. The silly 'makeall' and 'makecore' scripts are gone. If
you are a GCC user, all you need do now is run 'make' and 'make install', as
for other projects. You don't even have to run a 'fix' script any more! There
are some caveats, which are covered in readme.txt. If you use Microsoft
Visual C++ 6, you no longer need to obtain GCC and GNU Make - there is a
project file just for you.

Huge thanks go to Steve Terry for testing on Windows XP - about five times -
and to lillo for testing on BeOS and Mac OS X. Thanks also to X-G for testing
on a Windows system that has consistently posed problems for DUMB's old
makefiles.

There was a bug whereby al_poll_duh() would sometimes cause the music to
resume playing if you called it after al_pause_duh(). Whether this was DUMB's
fault for misusing Allegro's API, or a bug in Allegro, is unclear, but this
release makes it work.

In one of my projects, I found that my AL_DUH_PLAYER stopped playing when
there were lots of other sound effects. In order to fix this, I programmed
DUMB to set the priority of the stream's voice to 255, the maximum. I also
added al_duh_set_priority(), so you can set the priority yourself if you need
to.

The resampling code has undergone a transformation. The bad news is that the
linear average code is no longer in use. The good news is that where DUMB's
resamplers used to require three extra samples' worth of memory to be
allocated and initialised, it now copes with just the sample data. And it
does a very good job at bouncing off loop points and otherwise hurtling
around the sample. The resampling code is considerably more complicated, but
the code that uses the resamplers is considerably simpler - and if you
noticed a slight click in some bidirectionally looping samples, you'll be
pleased to know that that click is gone!

I have also devoted some effort to optimisation. It seemed hopeless for a
while, but then I actually figured out a way of making it faster AND more
accurate at the same time! DUMB is now quite a bit faster than it was, and it
mixes not with 16-bit precision, but with 24-bit precision. (It used 32-bit
integers all along, but the difference is that it now makes use of 256 times
as much of the integer's range.)

There have been the usual improvements to playback. The last release occurred
rather too soon after I had fixed the XM effect memories; EAx and EBx, fine
volume ramps, had been neglected. These are now handled properly.

In previous versions of DUMB, muted channels in IT were actually played with
surround sound panning (where the right-hand channel is inverted). This has
been fixed, so muted channels will really be muted now.

There were also some subtle problems with the way DUMB handled New Note
Actions for IT files. It turned out that, in all releases of DUMB so far,
pitch, filter and panning envelopes and sample vibrato were not being
processed for any note that was forced into the background by a new note on
the same channel! This only affected IT files. Not only has this been fixed,
but envelope interpolation is much more accurate. Long trailing envelope-
driven fade-outs sound a lot better now!

Since panning and filter envelopes are more precise, extra fields have been
added to the DUMB_IT_CHANNEL_STATE struct, used by
dumb_it_sr_get_channel_state(). These fields hold the 'decimal' parts of the
pan and filter cut-off. See dumb.txt for details.

Mxx (set channel volume) now correctly only modifies the last note played on
the channel, not any previous notes that have been forced into the background
by New Note Actions, and filter effect processing is now closer to what
Impulse Tracker does.

The XM loader was slightly flawed and could crash on files containing samples
with out-of-range loop points. One such file was given to me. This has been
fixed.

Finally, the legal stuff. Julien Cugniere has been added to the list of
copyright owners. He deserves it, for all the work he did on the XM support!
And the licence has been changed. You are no longer required to include a
link to DUMB in a project that uses DUMB; the reasons for this relaxation are
explained in licence.txt. However, the request is still there ...

As usual, enjoy!


## v0.9.1, released 19 December 2002

Hi again! Lots to say this time, so I shall cut right to the chase.

DUMB now supports Impulse Tracker's low-pass resonant filters! Huge thanks go
to Jeffrey Lim, author of Impulse Tracker, for giving me what information he
still had regarding the algorithm; to cut a long story short, modifying
ModPlug Tracker's source code (which is in the Public Domain) led to an
algorithm whose output matched Impulse Tracker's perfectly.

Please note that ModPlug Tracker's filters as they stand do not match Impulse
Tracker's, and I have no interest in supporting ModPlug Tracker's variant
(especially not the integer rounding problems). Please see docs/modplug.txt,
new in this release, for details.

Thanks also go to Fatso Huuskonen for motivating me to add filter support,
and providing me with several great IT files to test it with!

The other important feature added for this release is click removal. Up until
now, DUMB has generated clicks when cutting notes, starting samples in the
middle, and so on. This version of DUMB will remove any such clicks. Note
that DUMB does not use volume ramps to accomplish this; the algorithm will
not take the bite out of the music!

In other news, DUMB now supports sample vibrato for IT files, and instrument
vibrato for XM files. A slight bug in New Note Action handling for IT files
has been fixed; Note Fade will not break the sustain loops of the sample and
envelope, as it did before. Tremor handling (Ixy) had a strange bug in it,
which has been fixed.

Support for XM files has been greatly enhanced. The XM envelope handling new
in the last release contained a huge bug, resulting in notes seeming not to
stop when they should; this has been fixed. Some XM files crashed DUMB, while
others failed to load; these problems have been solved. Effect memories now
work properly for XM and MOD files, to the best of my knowledge. Some other
differences between IT and XM have been accounted for, most notably the
Retrigger Note effects, Rxy and E9x.

DUMB's sound quality and accuracy are not the only areas that have been
enhanced. The API has been expanded, at last. You can now detect when a
module loops, or make it play through just once. You can ask DUMB to inform
you every time it generates some samples; this is useful for visualisation.
For IT files, you can intercept the MIDI messages generated by Zxx macros,
enabling you to synchronise your game with the music to some extent. (There
is no such method for XM, S3M or MOD files yet; sorry. Also note that the
function will be called before you actually hear the sound; I cannot improve
this until DUMB has its own sound drivers, which won't be for a while.) You
can query the current order and row. Finally, operations like changing the
speed and tempo are now possible, and you can query the playback state on
each channel.

Some parts of DUMB's API have been deprecated. Simple programs that use
Allegro will be unaffected, but if you get some compiler warnings or errors,
please review docs/deprec.txt. This file explains why those parts of the API
were deprecated, and tells you how to adapt your code; the changes you need
to make are straightforward. Sorry for the inconvenience.

For various reasons, I have made DUMB's makefiles use different compiler
flags depending on your GCC version (unless you are using MSVC). There is no
elegant way of getting the makefiles to detect when GCC is upgraded. If you
upgrade GCC, you should execute 'make clean' in order to make DUMB detect the
GCC version again. Otherwise you may get some annoying error messages. (It is
wise to do this in any case, so that all the object files are built with the
same GCC version.)

DUMB's example players have been unified into a single player called
'dumbplay'. The player has been enhanced to display messages when the music
loops, and when XM and MOD files freeze (effect F00; more information on this
in docs/howto.txt).

Finally, as noted on DUMB's website, the release notes from the last release
were inaccurate. It has been verified that DUMBOGG v0.5 does still work with
that release, and still works with this release. The esoteric DUMBOGG v0.6
has not been created yet, since DUMBOGG v0.5 still works.

Please scroll down and read through the indented paragraphs in the notes for
the last release; they are relevant for this release too.

That's all folks! Until next time.


## v0.9, released 16 October 2002

MOD support is here! DUMB now supports all four of the common module formats.
As usual, there have also been some improvements to the way modules are
played back. Most notably, handling of tone portamento in IT files has been
improved a lot, and XM envelopes are now processed correctly.

The other major change is that DUMB now does a dummy run through each module
on loading. It stores the playback state at thirty-second intervals. It stops
when the module first loops, and then stores the playback time. This results
in a slightly longer load time and a greater memory overhead, but seeking is
faster (to any point before the module first loops) and the length is
calculated! duh_get_length() will return this and is now documented in
docs/howto.txt and docs/dumb.txt.

DUMB's build process has been changed to use 'mingw' wherever it used
'mingw32' before; some directories have been renamed, and the 'fix' command
you had to run for MinGW has been changed from 'fix mingw32' to 'fix mingw'.

Last time, I directed you to scroll down and read the notes from a past
release, but ignore this point, and that point applies to something else, and
so on. Did anyone do so? Well, if you're reading this at all, you probably
did. Nevertheless, this time I shall be much less confusing and restate any
relevant information. So the least you can do is read it!

- If your program ever aborts with exit code 37 while loading an IT file,
  PLEASE LET ME KNOW! The IT file in question has a stereo compressed sample
  in it, and the format is unspecified for this case (Impulse Tracker itself
  doesn't use stereo samples at all). I will need the IT file in question,
  and any information you can give me about how the IT file was created (e.g.
  what program). (If you don't get to see an exit code, let me know anyway.)

- If your program ever outputs a line resembling "Inst 01 Env: 0,64 8,32
  15,48" to stderr while loading an IT file, PLEASE LET ME KNOW! You have an
  old IT file (saved by an Impulse Tracker version older than 2.00), and
  support for such files is STILL untested.

- Not all parts of DUMB's API are documented yet. You will find some
  functions in dumb.h which are not listed in docs/dumb.txt; the reason is
  that these functions still need work and will probably change. If you
  really, really want to use them, talk to me first (IRC EFnet #dumb is a
  good place for this; see readme.txt for details on using IRC). I intend to
  finalise and document the whole of DUMB's API for Version 1.0.

There have been some changes to the naming conventions in DUMB's undocumented
API. DUMBOGG v0.5 will not work with this and subsequent releases of DUMB;
please upgrade to DUMBOGG v0.6. These changes should not break anything in
your own code, since you didn't use those parts of the API, did you ;)

There is still a great deal of work to be done before DUMB's API can be
finalised, and thus it will be a while before DUMB v1.0 comes out. It should
be worth the wait. In the meantime, there will be 0.9.x releases with
additional functionality, improved playback, and possibly support for some
extra file formats.

Finally I should like to offer an apology; there is a strong possibility that
some of DUMB's official API will change in the near future. There will not be
any drastic changes, and the corresponding changes to your source code will
be simple enough. If I didn't make these changes, DUMB's API would start to
become limited, or messy, or both, so it's for the better. I apologise in
advance for this.

Now scroll down and read the notes for the first r... oh wait, we already did
that. I guess that's it then. You can stop reading now.

Right after you've read this.

And this.

Off you go.

Bye.


## v0.8.1, released 11 August 2002

This is a minor release that fixes a few bugs. One of these bugs, however,
was pretty serious. dumb_register_dat_xm() was never coded! It was prototyped
in aldumb.h, so code would compile, but there would be an unresolved symbol
at the linking stage. This has been fixed.

Platforms other than Unix did not have a working 'make veryclean' target;
this has been fixed. In addition, the makefiles now use 'xcopy' instead of
'copy', since on some systems GNU Make seems to have trouble calling commands
built in to the shell.

Contrary to the errata that was on the DUMB website, the makeall.sh and
makecore.sh scripts actually DID install in /usr. This has now been
corrected, and regardless of whether you use these scripts or call make
directly, the files will now be installed to /usr/local by default.

The XM loader used to treat stereo samples as mono samples with the data for
the right channel positioned after the data for the left channel. This
generally resulted in an unwanted echo effect. This has been fixed.

When playing XM files, specifying an invalid instrument would cause an old
note on that channel to come back (roughly speaking). Fast Tracker 2 does not
exhibit this behaviour. This has been fixed.

The GCC makefiles used -mpentium, which is deprecated in gcc 3.x. This was
generating warnings, and has now been fixed.

In XM files, the length of a sample is stored in bytes. DUMB was assuming
that the length of a 16-bit sample would be even. I had two XM files where
this was not the case, and DUMB was unable to load them. This has been fixed.

In order to accommodate the extra part of the version number,
DUMB_REVISION_VERSION has been added. DUMB_VERSION has also been added in
order to facilitate checking if the version of DUMB installed is sufficient.
See docs/dumb.txt for details.

As a last-minute fix, the XM "Break to row" effect is now loaded properly. It
was necessary to convert from binary-coded decimal to hexadecimal (those who
have experience with Fast Tracker 2 will know what I mean). In short, this
means the effect will now work properly when breaking to row 10 or greater.

DUMB v0.8 had faulty release date constants; DUMB_MONTH and DUMB_DAY were
swapped! For this reason, DUMB_DATE should not be compared against any date
in 2002. This note has been added to docs/dumb.txt and also to dumb.h.

Please scroll to the end and read the release notes for the first version,
DUMB v0.7. Most of them apply equally to this release. However, the
non-portable code was rewritten for DUMB v0.8, so that point does not apply.
The point about length not being calculated also applies to XM files.

Enjoy :)


## v0.8, released 14 June 2002

Welcome to the second release of DUMB!

In addition to these notes, please read below the release notes for the
previous version, DUMB v0.7. Most of them apply equally to this release.
However, the non-portable code has been rewritten; DUMB should now port to
big-endian platforms.

The main improvement in this release of DUMB is the support for XM files.
Enormous thanks go to Julien Cugniere for working on this while I had to
revise for my exams!

There was a mistake in the makefiles in the last release. The debugging
Allegro interface library was mistakenly named libaldmbd.a instead of
libaldmd.a, meaning you had to compile with -laldmbd, contrary to what the
docs said. Apologies to everyone who lost sleep trying to work out what was
wrong! The reason for using libaldmd.a is to maintain compatibility with
plain DOS, where filenames are limited to eight characters (plus a three-
letter extension). The makefiles have now been changed to match the
information in the docs, so you may have to alter your project files
accordingly.

The example programs were faulty, and crashed on Windows if they were unable
to load the file. It was also difficult to work out how to exit them (you had
to click the taskbar button that didn't have a window, then press a key).
They have been improved in both these respects.

I have now added a docs/faq.txt file (Frequently Asked Questions), which is
based on problems and misconceptions people have had with the first release.
Please refer to it before contacting me with problems.

Thanks to networm for touching up the Unix makefile and writing the
instructions on using it.

Incidentally, today (Friday 14 June) is the Robinson College May Ball at
Cambridge Uni. God knows why it's called a May Ball if it's in June. I'm not
going myself (72 GBP, and I'd have to wear a suit, ugh), but with all the
noise outside I shall enjoy pumping up the speakers tonight!


## DUMB v0.7, released 2 March 2002

This is the first release of DUMB, and parts of the library are not
crystallised. Don't let this put you off! Provided you don't try to use any
features that aren't documented in docs/dumb.txt, the library should be rock
solid and you should be able to upgrade more or less without problems.

Here are some notes on this release:

- There is some non-portable code in this release of DUMB. It is likely that
  the library will fail to load IT files with compressed samples on
  big-endian machines such as the Apple Macintosh.

- If your program ever aborts with exit code 37 while loading an IT file,
  PLEASE LET ME KNOW! The IT file in question has a stereo compressed sample
  in it, and the format is unspecified for this case (Impulse Tracker itself
  doesn't use stereo samples at all). I will need the IT file in question,
  and any information you can give me about how the IT file was created (e.g.
  what program). (If you don't get to see an exit code, let me know anyway.)

- If your program ever outputs a line resembling "Inst 01 Env: 0,64 8,32
  15,48" to stderr while loading an IT file, PLEASE LET ME KNOW! You have an
  old IT file (saved by an Impulse Tracker version older than 2.00), and
  support for such files is untested.

- The length of IT and S3M files is not currently calculated. It is just set
  to ten minutes.
