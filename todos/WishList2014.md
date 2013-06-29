#Wish List for the 2013/2014 Build Season#

This is a general list of all the things we (the software team) would like to
accomplish or, more generally, devote our time to, in the 2013/2014 build
season. They could be codebase improvements, new features, workflow changes,
you name it; feel free to add any suggestions you might have.

In no particular order, here are the tasks:

##Definitely##
* Threading: Move to the [Boost threading library][boost-threads] for better
  compatibility with C++ object-oriented code. An alternative, perhaps a bit
  more natural to program, is the C++ [atomic operations library][atomic].

* Cleanup: Go through code, unify style (conformance to GOFIRST coding
  conventions), make non-object-oriented code object-oriented (avoid globals).

* Documentation: Make sure all code that is meant to be usable by other modules
  (API) is well-documented in its usage, preferably in the header file.
  Implementation-level documentation should be just enough to give a reader
  a general idea of how the code executes.

* Testing: Build a robot simulator framework for easier testing of individual
  components. Consider unit testing on critical pieces of code. The goal is
  that the robot program can run on any computer with simulated (or real)
  sensor inputs and the user can monitor its progress.

* Operating System: Either fix the silly issues we've been having with Ubuntu
  (Ethernet on laptop, OpenCV) or tutor everyone in the basics of using Arch
  (it's not _that_ different from other distributions, on some level).

* Development Tools: Find a good graphical debugger that works under Linux.
  Switch to [`clang`][clang] compiler instead of `gcc` (reasons: Better
  performance, more sensible error messages, fewer potential licensing issues,
  more [here][clang-comp]).

* Version Control: Move the codebase into _one_ repository (it's one program,
  so it needs to all be contained in a single repo). Introduce a feature-branch
  workflow so no development takes place directly in the main branch, only in
  separate feature branches (see [Feature Branch Workflow][fbranch] for more
  details). This should prevent the commits from getting messed up and
  interfering with each other, whatever that means.

##Hopefully##
* Platform-independent code: Modify the codebase so none of it depends on Unix-
  specific system functions, and the code can theoretically build equally well
  on \*nix (including Mac OS X) and on Windows environments. This way, those
  who prefer the Windows development tools for whatever reason can use them
  without inconveniencing others. Boost may be of some help in this pursuit.
  Related: "Programming languages", below.

* Licensing: Decide whether to publish our codebase, and if so, which license
  to publish it under. There are several possible licenses, such as the
  [GPL][], the [Apache license][apache], or the [BSD license][bsd]. Go over
  the possible legal issues involved (transferring copyright from authors,
  copyright disclaimer from U of M).

* Advanced documentation: Make header-file (API) documentation compatible with
  documentation-processing tools such as [Doxygen][], so we can generate a clean
  HTML or PDF set of documentation for our code.

##Maybe##
* Programming languages: Consider using a higher-level language such as Python
  for the code that doesn't require the low-level approach of C/C++. High-level
  modules such as the main loop, vision, and possibly AI could be ported to
  Python to make it more readable and more easily maintainable. Performance may
  not be an issue; using the proper libraries, Python code can be written that
  is actually faster than a naïve C++ implementation (this was the case with a
  Monte Carlo simulation I helped write for a physics project -- the NumPy
  version was around 7x faster than the C++ version). The interactivity
  capability provided in Python is also helpful for quick experimentation
  or even debugging. Code that needs to work with e.g. serial ports or other
  low-level functions can stay in C++ and be called from Python code; it's
  definitely possible, but we would need to investigate how to do this.

* [ROS][]: There may be a point where we find that the abstractions, flexibility,
  modularity, and access to existing algorithms and software offered by ROS are
  worth the tremendous overhead of installing and using it. Additionally, if a
  ROS guru joins the team and is able to contribute significant help in using
  it, that would be another possible impetus for moving to ROS. We might also
  consider other lightweight ROS-like robot frameworks; there were a few
  listed on the ROS wiki or thereabouts.

[boost-threads]: http://www.boost.org/doc/libs/1_53_0/doc/html/thread.html
[atomic]: http://en.cppreference.com/w/cpp/atomic
[clang]: http://clang.llvm.org/index.html
[clang-comp]: http://clang.llvm.org/comparison.html
[fbranch]: http://www.atlassian.com/git/workflows#!workflow-feature-branch
[GPL]: http://www.gnu.org/licenses/gpl.html
[apache]: http://apache.org/licenses/
[bsd]: http://directory.fsf.org/wiki/License:BSD_3Clause
[Doxygen]: http://www.stack.nl/~dimitri/doxygen/
[ROS]: http://www.ros.org/wiki/
