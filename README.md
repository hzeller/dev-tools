Some random tools and scripts that I use in various development.
Decentral git backup :) Might or might not be useful to others.

Shell scripts
  * git-sync-upstream : sync a fork to the upstream repo.
  * git-new-feature   : create a new branch with date and short feature name
  * git-open-pr-files : find all the files that are currently open in PRs on GH

C++ scripts
These 'scripts' self-compile (see first line). If you set the executable
bit (`chmod +x`), you can run them directly as if they were a script, or,
you can invoke them with sh:

```
sh ./run-clang-tidy-cached.cc
```

These being c++ makes them easier to read and maintain for folks working with
c++ anyway, simply using the included batteries of a c++17 environment.
Only need a c++ compiler and no exotic scripting language installed.

  * run-clang-tidy-cached.cc : Run clang-tidy in parallel on project and cache
            results to speed up the pretty slow clang-tidy processing runtime.
	    Allows for some configuration to local project.
  * move-header-to-front.cc : if a particular include is found in a file,
            move it right in front of the first include of that file if
	    not already.
	    Can be used to move the declaration header for an implementation
	    to the front.
	    (Note: does not take #ifdefs into account, so might need manual
	    intervention to move if not desired at the location).
  * insert-header.cc : insert a header into file(s), if not already there.
            Puts <>-headers before the first <>-header, others before the first
	    "" header (if available). Other than that, does not make any
	    attempt to sort the final headers or understand if it ended up
	    in the right group (that is what clang-format is for :) ).


The *.cc files are done that they act as self-compiling 'scripts'. So
they can be invoked with e.g. ./insert-header.cc
