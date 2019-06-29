[![Build Status](https://dev.azure.com/onivim/oni2/_apis/build/status/onivim.libvim?branchName=master)](https://dev.azure.com/onivim/oni2/_build/latest?definitionId=4&branchName=master)

## What is `libvim`?

`libvim` is a fork of [Vim](https://github.com/vim/vim), with the goal of providing a [minimal C-based API](https://github.com/onivim/libvim/blob/master/src/libvim.h), modelling Vim modal editing. It does not include any user interface at all (not even a terminal UI), and is primarily responsible for acting as a fast buffer manipulation engine, faithful to Vim keystrokes. It's still a work-in-progress and there is lots of work left to stabilize.

If you're looking for a terminal Vim, check out [neovim](https://github.com/neovim/neovim), or a GUI Vim, check out [Onivim 2](https://v2.onivim.io).

## Why?

`libvim` is primarily intended for [Onivim 2](https://v2.onivim.io). After implementing several iterations of 'UI Vims' between v1, v2, and other projects, the abstraction I wished to have was a sort of a pure functional Vim, completely decoupled from terminal UI - where 'vim' is a function of `(editor state, input) => (new editor state)`. As Onivim 2 completely handles the rendering layer, this Vim-modelled-as-a-pure-function could focus on just buffer manipulation.

To that end, `libvim` exposes a simple C API for working with Vim, and supports listening to buffer changes, messages, etc. 

It is responsible for:
- Managing and manipulating buffers
- Buffer manipulation in response to input
- Parsing and sourcing VimL
- Handling key remaps

It is __NOT__ responsible for:
- Any sort of UI rendering (terminal, etc)
- Mouse support
- Syntax Highlighting
- Spell Checking
- Terminal Support
- Completion
- Input methods (IME)

All of these are intended to be handled by the consumer of the library - leaving `libvim` to be focused on the job of fast buffer manipulation.

`libvim` builds cross-platform (since [Onivim 2](https://v2.onivim.io) requires it!), as well as for WebAssembly - we'd like to port our v1 tutorials to a browser-based experience.

There are other interesting applications of such an 'abstracted Vim':
- WebAssembly builds could be useful for implementing Vim modes in browsers / websites
- Native builds could be useful for applications that want Vim-native bindings - it'd be a nice foundation for implementing `readline`, for example.

## API

For an example of the API usage, check out the [apitests](https://github.com/onivim/libvim/blob/master/src/apitest) like [normal_mode_motion](https://github.com/onivim/libvim/blob/master/src/apitest/normal_mode_motion.c). The full API is available here: [libvim.h](https://github.com/onivim/libvim/blob/master/src/libvim.h)

The heart of the API is `vimInput` which takes a single key, and is synchronously processed by the state machine. 'Side-effects' like buffer updates, messages, etc can be subscribed to via callbacks like `vimSetBufferUpdateCallback`.

This library is in active development and we currently make no guarantees about backwards compatibility. Use the API at your own risk.

## Compiling ##

### Install [esy](https://esy.sh/)

`esy` is like `npm` for native code. If you don't have it already, install it by running:
```
npm install -g esy@0.5.7
```

### Get sources

- `git clone https://github.com/onivim/libvim`
- `cd src`

### Installing dependencies

- `esy install`
- `esy '@test' install`

### Building

- `esy build`

### Running tests

- `esy '@test' build`

## FAQ

### Why is `libvim` based on Vim and not Neovim?

I'm a huge fan of the work the Neovim team is doing (and the team has been incredibly support of the Onivim project). Ideally, we would've stuck with Neovim or implemented `libvim` based on `libnvim`. In fact, the first time I tried to build this 'minimal abstraction' - I tried to base it off Neovim's `libnvim`. I timeboxed the investigation to 2 days, and ran into some serious hurdles - our build environment is a bit challenging on Windows (it's based on Cygwin + MingW cross-compiler toolchain) - I encountered several issues getting Neovim + deps to build in that environment. Based off that spike, I estimated it would take ~3-4 weeks to get it working in that toolchain.

Note that this is not a Neovim issue - the dependency usage and leveraging of `CMake` are good decisions - it's a consequence of our OCaml build system. The Cygwin + MingW cross-compiler toolchain isn't well handled by all dependencies (being a weird hybrid of Win32 and Unix, it's often the case where #ifdefs are wrong, incorrect dependencies are pulled in, and it can be a huge time sink working through these issues).

Vim, in contrast, was able to compile in that environment easily (NOTE: If anyone is interested in building a cross-platform, `esy`-enabled Neovim package - we can revisit this!). I'm also interested in WebAssembly builds, for porting the Onivim v1 tutorials to the web, in which this C-abstracted library compiled to WebAssembly would be a perfect fit.

Beyond the build issues, both Neovim and Vim would need refactoring to provide that synchronous, functional API:
- Neovim uses an event loop at its core, which would need to be short-circuited or removed to provide that API
- Vim uses blocking input, which would need to be inverted to support the functional API

The motivation of all this work was to remove the RPC layer from Onivim v2 to reduce complexity and failure modes - at the end, this was purely a constraint-based technical decision. If we can get a similar API, buildable via `esy` cross-platform, with `nvim` - I'd be happy to use that :)

## Supporting

If `libvim` is interesting to you, and you'd like to support development, consider the following:

- [Pre-order](https://v2.onivim.io) Onivim 2
- Support on [Patreon](https://www.patreon.com/onivim)

## Contributing

If you would like to help making `libvim` better, see the [CONTRIBUTING.md](https://github.com/vim/vim/blob/master/CONTRIBUTING.md) file.

Some places for contribution:
- Help us [add test cases](https://github.com/onivim/libvim/tree/master/src/apitest)
- Help us remove [code](https://github.com/onivim/libvim/pull/31) or [features](https://github.com/onivim/libvim/pull/30) that aren't required for `libvim`
- Help us port [patches](https://github.com/vim/vim/commits/master) from Vim

## License

`libvim` code is licensed under the [MIT License](./LICENSE).

It also depends on third-party code, notably Vim, but also others - see [ThirdPartyLicenses.txt](./ThirdPartyLicenses.txt) for license details.
