#GOFIRST Sensor Interface Specification#

This is maybe not as much a specification as it is a naming guide for functions
meant to expose aspects of sensor funcionality to the main sensor buffer, i.e.
to the rest of the software system. It is also a loose specification of the
semantics of the specified functions. Please follow these guidlines to maintain
internal code consistency and to make the writing of the main sensor data
buffer much eaiser.

This document aims to follow the conventions already followed in the code to the
greatest possible extent. In the case that multiple conventions are in use in
the code, the one that I see as the prevailing convention will be chosen.

The conventions established here _may_ be changed if they are found to ignore an
already-established convention set in the code that I somehow missed. In the
case that no convention exists where one is needed, I will set one that best
fits with the existing conventions.

Comments, questions, clarifications, and other requests and addenda may be
emailed to Max Veit.

##Terminology and Technical Details##

Usually, the 'caller' will be implicity understood to be the main sensor data
buffer class, whose task it is to gather data from all the robot sensors and
present the latest version to the main loop, which feeds it to the rest of the
program. The 'callee' is the specific sensor interface component in question,
for example, the LIDAR, Arduino, or Vision software components, which presents
an interface to a specific sensor or a group of sensors for the purpose of
requesting data, receiving data, and configuration.

All the function names explicitly described here should be made `public`, as
they constitute an interface that any other class should be able to use.

##Modes of Operation##

There are two modes of interacting with a sensor: Serial and asynchronous. In
synchronous (or blocking) operation, the caller requests data and waits for the
data to be available before proceeding. In asynchronous (or non-blocking) mode,
the callee runs in its own thread, and gets new data from the sensors when this
data is requested by the caller. The operation of getting new data is
potentially time-consuming, so the request returns immediately and the caller
is free to continue polling other sensors or performing other operations. The
callee internally caches the new data when it becomes available, so the
caller can access it on its next pass.

One feature of asynchronous mode is that the callee is able to perform the
potentially time-consuming task of gathering new data from the sensor while
providing the caller with the previous copy of the data, so at any given
moment, the system is working on one copy of the data while the next one is
being retrieved. This feature is also called 'pipelined operation'.

##Requesting Data##

In asynchronous operation, the caller may initiate an update of the sensor data
by calling the function with the C++ signature:
    void readData()
The caller will expect this function to return immediately.

##Accessing Data##

In both synchronous and asynchronous operation, specific items of data may be
accessed using the function named:

    <type> get<Description>()

Where `<type>` is the datatype of the data item accessed, and `<Description>`
is a short CamelCase description of the data item that will be returned. For
example, the function `double getHeading()` is expected to return the current
heading in raw double form.
In some cases, the short form `<type> getData()` may be used, in the case where
it is obvious what will be returned. Of course, the type, format, and meaning
of the returned data must always be documented, preferably in the header file!

One specific function that all sensor interfaces should implement is:

    timeval getTimeStamp()

The caller may want the timestamp to determine, for example, how recent a
certain set of sensor data is. Note the use of the `timeval` type, which has
nominally microsecond precision. This type is defined in `sys/time.h`, along
with the function `int gettimeofday(timeval *tv, timezone *tz)`, which
returns the current time with approximately 0.01-second precision.

In blocking operation, the `get<Data>()` functions are expected to take as long
to complete and return as is necessary to fetch the data. In asynchronous
operation, they are expected to return immediately.

###Issues with Asynchronous Operation###

The main potential issue with asynchronous sensor operation is the possibility
that the sensor interface may update its cached data at the same time that
the caller is accessing it. If the data consists of multiple components
accessed through multiple function calls, there is the possiblity that the
caller may access pieces of data from different times, which may be
undesirable. In addition, calls to `getTimeStamp()` may return timestamps that
are incorrect for some of the data. To prevent this issue, callees may use one
of two different strategies.

The first strategy is to provide a function with the signature
`bool isUpdating()`, which tells whether the sensor is currently in the process
of fulfilling an update that was requested by calling `readData()`. This
function should return `false` once all the new data has been copied into the
callee's internal cache. Once this function returns `false`, the `get<Data>()`
functions must return the same values regardless of when or how many times they
are called, up until the next time `readData()` is called. The caller may thus
use this function as an indicator of whether it is safe to access the callee's
data.

The second strategy, which may be used _as an alternative_ to the first in the
case where the first strategy is somehow inconvenient or undesirable to
implement, is to provide a set of functions that protect the callee's cache
from modification. The functions are:

    void acquireLock();
    void releaseLock();

Between the calls of `acquireLock()` and `releaseLock()`, the `get<Data>()`
functions must return the same values, regardless of when or how many times
they are called. Implementations must ensure that any new data received while
the lock is held by the caller is buffered without overwriting the cache that
provides data to the caller.

The first approach is recommended, as it provides the least implementation
complexity and greatest interface simplicity. However, it may require extra
complexity on the part of the caller in the case that an update routinely takes
longer than the main-loop period. The caller will then often receive a `true`
return value from `isUpdating()`, necessitating the caller to have a locally
stored copy of the data on which to fall back. The second approach alleviates
this problem somewhat by simply having the callee return the latest cached
version of the data regardless of whether the sensor data is currently being
updated.

###Naming for Serial and Asynchronous Operation###

In most cases, the sensor interfaces should support either serial operation or
asynchronous operation, but usually not both. Which mode is being used should
be _clearly documented_ in the header file for the class (the preferred place
for high-level source-code documentation), both in the class documentation and
as a note in the documentation of the individual `get<Data>()` functions.

In the case that a sensor interface needs to support both modes, all `get<Data>`
function names should be suffixed with either the word `Blking` to indicate
that it is a serial or blocking function, or with `Async` to indicate that the
function operates asynchronously. Naturally, the header-file documentation for
each function should also mention which mode that function uses. Ideally, each
`get<Data>()` function would be implemented as both blocking and asynchronous
versions in such interface classes.

##Capitalization of Function Names##

All function names in the sensor interface are to be spelled in camel-case with
the initial letter uncapitalized, as in `getTimeStamp`. This convention is the
prevailing one in the codebase and is a valid C++ convention that is often in
use. While the convention with an initial uppercase letter (cf. `GetTimeStamp`)
is also in use and is perhaps more common, the former convention should be used
in the interest of uniformity.

Class names should, as always, be spelled in camel-case with the initial letter
capitalized, as in `SabertoothPacket`.


