viaduct
=======

viaduct is a C WAMP library for embedded development.

features
--------

* no heap allocations
* feature flags

dependencies
------------

* [cmp](https://github.com/camgunz/cmp) - source is included in `vendor/'
* [libtap](https://github.com/zorgnax/libtap) - for tests
  * can be installed on Arch Linux through the AUR - [libtap](https://aur.archlinux.org/packages/libtap/)

install
-------

viaduct uses CMake to build:

	cmake .
	make

To run the tests, after building, run:

    make test

To run the example:

	go get github.com/beatgammit/turnpike/examples/raw-socket/raw-socket-server
	$GOPATH/raw-socket-server &
    ./example

status
======

The example works, but the library is still in a lot of flux.

Currently only WAMP Publish is implemented, but that's enough for most micro-controllers
to send data back to a central hub.

todo
----

* support any arbitrary payload for publishing events
* authentication
* clean up example to take more parameters
* JSON as serialization format
* error handling/reporting
* more WAMP features and a feature flag for each
* more unit tests

license
=======

viaduct is licensed under the 2-clause BSD license. See LICENSE.BSD for details.
