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

These 'scripts' are written in C++ but self-compile ([see their first lines](https://github.com/hzeller/dev-tools/blob/f40950208913ee9ff8cc70916b8100713087b60c/run-clang-tidy-cached.cc#L1-L3)) so behave like shell scripts.
If you set the executable bit (`chmod +x`), you can run them directly as if
they were a script, or, you can invoke them with sh:

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

As an output, you get a nice summary overview in `Project_clang-tidy.summary`,
something like

```
  137 [misc-include-cleaner]
  101 [misc-const-correctness]
   63 [modernize-use-emplace]
   36 [performance-enum-size]
   35 [misc-use-anonymous-namespace]
   33 [misc-unused-parameters]
   29 [performance-avoid-endl]
   26 [bugprone-switch-missing-default-case]
   16 [modernize-use-override]
   14 [clang-diagnostic-error]
   12 [clang-analyzer-optin.core.EnumCastOutOfRange]
```

... as well as all the details in `Project_clang-tidy.out`; I usually use
that to step through these with the IDE (e.g. in `emacs`, use
`M-x compile-command`, `cd project-root; cat Project_clang-tidy.out`) and
then step through each messages as if it was a compiler output.

Next time you run `run-clang-tidy-cached.cc` it can be very fast as it only
re-processes the changes. The cache is stored out-of-tree, so it persists even
if you wipe your project directory.

### Usage

 in the simplest case:
```
./run-clang-tidy-cached.cc
```

... but you can also provide flags that are then passed to `clang-tidy`

```
./run-clang-tidy-cached.cc --checks="-*,modernize-use-override" --fix
```

Also check the [environment variable description](https://github.com/hzeller/dev-tools/blob/f40950208913ee9ff8cc70916b8100713087b60c/run-clang-tidy-cached.cc#L30-L34) for further runtime configuration.

### [insert-header.cc](./insert-header.cc)
Insert a header into file(s), if not already there.  Puts `<>`-headers before
the first `<>`-header, others after the first `""`-header (if available) (Why
after first ? The first `""`-header might be the implementation header).

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

## Cleanup shell scripts

There are two more scripts that can help with clean-up of c++ projects, in
particular dealing with missing or superfluous headers (the
`misc-include-cleaner` clang-tidy warning).

### [header-fixer.sh](./header-fixer.sh)

This helps adding headers that are needed but missing. Unfortunately,
`clang-tidy` does not tell use where the header would be (if often certainly
would know), so you first have to create a configuration file with a regular
expression for the type in the first column, the corresponding header in the
second; say `fix-headers.txt`:

```
(std::)?u?int[123468]*_t <cstdint>
FILE                     <cstdio>
f?printf                 <cstdio>
std::sort                <algorithm>
std::pair                <utility>
std::string              <string>
std::vector              <vector>
(std::)?isspace          <cctype>
My::ProjectClass path/to/project-specific/header.h
```

... and with that replacement file you can invoke the script:

```
Usage: ./header-fixer.sh <clang-tidy-out> <replacement-list-file>

The clang-tidy-out file is a clang-tidy.out file as generated bu
ryn-clang-tidy-cached.cc

The replacment list file has two columns, that are whitespace delimited.

The first column contains a regular expression for a symbol that is missing,
the second column says corresponding header to include. If there are multiple
rows that include the same header, the header only has to be mentioned the
first time in that second column.
Example:

foo::SomeClass  path/to/myclass.h

Each line will be translated to a one-liner script to insert the
corresponding header, i.e. source the output to perform the necessary edits.

Tip:
To get an overview list of symbols that are used without their header, run

awk '/no header providing.*misc-include-cleaner/ {print $6}' my_clang-tidy.out | sort | uniq -c | sort -n
```

The script will not do anything by itself, but it will emit a set of one-liners
that will do the fixing; a typical output might look like

```
./insert-header.cc ' <vector>' -q $(awk -F: '/no header providing "std::vector".*misc-include-cleaner/ {print $1}' project_clang-tidy.out | sort | uniq)
./insert-header.cc ' <cctype>' -q $(awk -F: '/no header providing "(std::)?isspace".*misc-include-cleaner/ {print $1}' project_clang-tidy.out | sort | uniq)
```

(Note, that it emits invocations to the `insert-cleaner.cc` script).

So with that, you then can just take the output and execute in a shell, i.e.
source it:


```
. <(path/to/header-fixer.sh project_clang-tidy.out fix-headers.txt)
```

### [remove-superfluous-headers.sh](./remove-superfluous-headers.sh)

```
Usage: ./remove-superfluous-headers.sh <clang-tidy-out>
Emit a script that removes all headers clang-tidy found to not be used directly
```

## Putting it all together: doing cleanups in C++ projects

Typically what I do is to have this `dev-tools/` project checked out alongside
other projects and then can refer to them with a relative path:

```
../dev-tools/run-clang-tidy-cached.cc
```

If you need local configuration, just copy the script locally and modify the
config section.

## Add missing headers (misc-include-cleaner)

For that, you first have to create some file with the mapping of type to
header file as discussed in the header-fixer section.

It is often useful to get an overview of which types are missed the most and
start writing the configuration from there; I use a little shell one-liner
to emit a list given the output of the run-clang-tidy-cached output:

```
awk '/no header providing.*misc-include-cleaner/ {print $6}' MyProject_clang-tidy.out | sort | uniq -c | sort -n
```
And you get an output like this:

```
 3 "std::vector"
 7 "std::string"
12 "std::uint32_t"
```

So let's write a fix-headers.txt that addresses these

```
(std::)u?int[123468]*_t <cstdint>
std::string      <string>
std::vector      <vector>
```

and run the header-fixer

```
. <(../dev-tools/header-fixer.sh MyProject_clang-tidy.out fix-headers.txt)
```

the fix-headers.txt will typically accumulate a few mappings over time, but
the neat thing is, you can re-run the header-fixer many times and it will only
modify the files that actually need changing.

## Remove header inclusion that are not needed (misc-include-cleaner)

If all needed headers are added, you then can remove all not needed headers.

```
. <(../dev-tools/remove-superfluous-headers.sh MyProject_clang-tidy.out)
```

Note, this is only really a good idea if all/most missing headers have in fact
been added. If you remove superfluous headers before you have a somewhat
clean project w.r.t. needed headers, this step might headers that provide
needed symbols 'accidentally' due to transitive inclusion.
