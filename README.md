![Vim Logo](https://github.com/vim/vim/blob/master/runtime/vimlogo.gif)

[![Build Status](https://dev.azure.com/onivim/oni2/_apis/build/status/onivim.libvim?branchName=master)](https://dev.azure.com/onivim/oni2/_build/latest?definitionId=3&branchName=master)

## What is `libvim`?

`libvim` is a minimal C-based abstraction of Vim modal editing (it is an unofficial fork). It does not include any user interface, and is primarily responsible for acting as a fast buffer manipulation engine. 

## Why?

`libvim` is an experiment primarily used for [Onivim 2](https://v2.onivim.io). After implementing several iterations of 'UI Vims' between v1, v2, and other projects, the abstraction I wished to have was a sort of a pure functional Vim, completely decoupled from terminal UI - where 'vim' is a function of `(editor state, input) => (new editor state)`.

To that end, `libvim` exposes a simple C API for working with Vim, and supports listening to buffer changes, messages, etc. 

It is responsible for:
- Managing and manipulating buffers
- Buffer manipulation in response to input
- Parsing and sourcing VimL
- Handling key remaps

It is not responsible for:
- Any sort of UI rendering (terminal, etc)
- Mouse support
- Syntax Highlighting
- Spell Checking
- Terminal Support

Support TBD:
- Folding
- Line breaking

`libvim` is planned to build cross-platform (since [Onivim 2](https://v2.onivim.io) requires it!), as well as for WebAssembly - we'd like to port our v1 tutorials to a browser-based experience.

## Distribution ##

You can often use your favorite package manager to install Vim.  On Mac and
Linux a small version of Vim is pre-installed, you still need to install Vim
if you want more features.

There are separate distributions for Unix, PC, Amiga and some other systems.
This `README.md` file comes with the runtime archive.  It includes the
documentation, syntax files and other files that are used at runtime.  To run
Vim you must get either one of the binary archives or a source archive.
Which one you need depends on the system you want to run it on and whether you
want or must compile it yourself.  Check http://www.vim.org/download.php for
an overview of currently available distributions.

Some popular places to get the latest Vim:
* Check out the git repository from [github](https://github.com/vim/vim).
* Get the source code as an [archive](https://github.com/vim/vim/releases).
* Get a Windows executable from the
[vim-win32-installer](https://github.com/vim/vim-win32-installer/releases) repository.



## Compiling ##

If you obtained a binary distribution you don't need to compile Vim.  If you
obtained a source distribution, all the stuff for compiling Vim is in the
`src` directory.  See `src/INSTALL` for instructions.


## Installation ##

See one of these files for system-specific instructions.  Either in the
READMEdir directory (in the repository) or the top directory (if you unpack an
archive):

	README_ami.txt		Amiga
	README_unix.txt		Unix
	README_dos.txt		MS-DOS and MS-Windows
	README_mac.txt		Macintosh
	README_vms.txt		VMS

There are other `README_*.txt` files, depending on the distribution you used.


## Documentation ##

The Vim tutor is a one hour training course for beginners.  Often it can be
started as `vimtutor`.  See `:help tutor` for more information.

The best is to use `:help` in Vim.  If you don't have an executable yet, read
`runtime/doc/help.txt`.  It contains pointers to the other documentation
files.  The User Manual reads like a book and is recommended to learn to use
Vim.  See `:help user-manual`.


## Copying ##

Vim is Charityware.  You can use and copy it as much as you like, but you are
encouraged to make a donation to help orphans in Uganda.  Please read the file
`runtime/doc/uganda.txt` for details (do `:help uganda` inside Vim).

Summary of the license: There are no restrictions on using or distributing an
unmodified copy of Vim.  Parts of Vim may also be distributed, but the license
text must always be included.  For modified versions a few restrictions apply.
The license is GPL compatible, you may compile Vim with GPL libraries and
distribute it.


## Sponsoring ##

Fixing bugs and adding new features takes a lot of time and effort.  To show
your appreciation for the work and motivate Bram and others to continue
working on Vim please send a donation.

Since Bram is back to a paid job the money will now be used to help children
in Uganda.  See `runtime/doc/uganda.txt`.  But at the same time donations
increase Bram's motivation to keep working on Vim!

For the most recent information about sponsoring look on the Vim web site:
	http://www.vim.org/sponsor/


## Contributing ##

If you would like to help making Vim better, see the [CONTRIBUTING.md](https://github.com/vim/vim/blob/master/CONTRIBUTING.md) file.


## Information ##

The latest news about Vim can be found on the Vim home page:
	http://www.vim.org/

If you have problems, have a look at the Vim documentation or tips:
	http://www.vim.org/docs.php
	http://vim.wikia.com/wiki/Vim_Tips_Wiki

If you still have problems or any other questions, use one of the mailing
lists to discuss them with Vim users and developers:
	http://www.vim.org/maillist.php

If nothing else works, report bugs directly:
	Bram Moolenaar <Bram@vim.org>


## Main author ##

Send any other comments, patches, flowers and suggestions to:
	Bram Moolenaar <Bram@vim.org>


This is `README.md` for version 8.1 of Vim: Vi IMproved.
