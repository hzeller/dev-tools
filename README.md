# Random Development tools

Some random tools and scripts that I use in various development.
My decentral git backup; might or might not be useful to others.

## Git interaction

Small shell-scripts that make interacting with github repos

  * [`git-sync-upstream`](./git-sync-upstream) : sync a fork to the upstream
      repo. Daily life for someone contributing to github projects, keeping
      their fork up-to-date.
  * [`git-new-feature`](./git-new-feature)   : create a new branch with date
      and short feature name.
      Start of making a pull request and helps to not get lost with consistent
      naming.
  * [`git-open-pr-files`](./git-open-pr-files) : Slightly larger script: find
      all the files that are currently open in PRs on
      GitHub which can be useful for larger refactorings and you want to
      avoid touching files currently being reviewed. Requires the `gh` command
      line and possibly `gh` login to query the API.
      Outputs a list of files that are currently in open pull requests.

## C++ cleanup scripts

These scripts are used for maintaining c++ projects and help cleanups.

These 'scripts' are written in C++ but self-compile (see their first lines)
so behave like shell scripts. If you set the executable
bit (`chmod +x`), you can run them directly as if they were a script, or,
you can invoke them with sh:

```
sh ./run-clang-tidy-cached.cc
```

These being c++ makes them easier to read and maintain for folks working with
c++ anyway. Only need a c++ compiler and no exotic scripting language installed.

### [run-clang-tidy-cached.cc](./run-clang-tidy-cached.cc)
Run clang-tidy in parallel on project and cache results to speed up the pretty
slow `clang-tidy` processing runtime. It uses a content-addressed cache to keep
track of the results.

It requires a `.clang-tidy` config in your project and a compilation database,
(`compile_flags.txt` or `compile_commands.json`).

Just copy into your project and, if needed, update the configuration section https://github.com/hzeller/dev-tools/blob/f40950208913ee9ff8cc70916b8100713087b60c/run-clang-tidy-cached.cc#L121-L130

Usage in the simplest case:

```
./run-clang-tidy-cached.cc
```

... but you can also provide flags that are then passed onto `clang-tidy`

```
./run-clang-tidy-cached.cc --checks="-*,modernize-use-override" --fix
```

Also check the [environment variable description](https://github.com/hzeller/dev-tools/blob/f40950208913ee9ff8cc70916b8100713087b60c/run-clang-tidy-cached.cc#L30-L34) for further runtime configuration.

### [insert-header.cc](./insert-header.cc)
Insert a header into file(s), if not already there.  Puts `<>`-headers before
the first `<>`-header, others as the second `""`-header (if available) (Why
second ? The first `""`-header is typically the implementation header).

Other than that, does not make any attempt to sort the final headers or
understand if it ended up in the right group (that is what `clang-format` is
for :) ).
Also does not check if it accidentally ended up inside an `#ifdef`, so manual
inspection might still be required.

```
Usage: ./insert-header <header> [-q] <file>...
        Simple way to insert a header into c/c++ file(s) if not there already.
        Header can be simple string (in which case it is included with "...") or bracketed with '<...>'.
        If header starts with '<', it is attempted to be inserted near an angle-bracket header.

Example
          ./insert-header '<vector>' foo.cc bar.cc
        will insert `#include <vector>` before the first angle include
        into files foo.cc and bar.cc;

        Similarly,
          ./insert-header 'hello/world.h' foo.cc bar.cc
        will insert `#include "hello/world.h"` before the second quote include.
```

### [move-header-to-front.cc](./move-header-to-front.cc)
If a particular include is found in a file, move it right in front of the
first include of that file if not already.
Can be used to move the declaration header for an implementation to the front.
(This is fairly simple so does not take `#ifdefs` into account,
so might need manual intervention).

```
Usage: ./move-header-to-front <header> <file>
Example
        ./move-header-to-front foo/bar.h src/foo/bar.cc

If an include of #include "foo/bar.h" is found in src/foo/bar.cc, then it is moved before the first include.
```
