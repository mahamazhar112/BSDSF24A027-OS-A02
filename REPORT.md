# REPORT

## Feature 2: Long Listing (v1.1.0)

**Q1. stat() vs lstat()?**
stat() follows a symbolic link and returns the metadata of the file it points to. lstat() does not follow it and returns the metadata of the link itself. In ls, lstat() is the right choice, because ls -l must show a symlink as type `l` with its own permissions and size, and print `name -> target`. Using stat() would hide that the entry is a link.

**Q2. st_mode bitwise operations?**
st_mode packs the file type and permissions in one integer. The file type is in the upper bits, and `mode & S_IFMT` isolates it; comparing the result to `S_IFDIR` or `S_IFREG` tells the type (the macros S_ISDIR(mode), S_ISREG(mode) do exactly this). The permission bits are in the lower bits, and `mode & S_IRUSR` is non-zero only if the owner read bit is set. The same works for S_IWUSR, S_IXUSR, and the GRP/OTH variants. Special bits S_ISUID, S_ISGID, S_ISVTX are tested the same way.
## Feature 3: Column Display (v1.2.0)

**Q1. Down-then-across logic? Why is a single loop insufficient?**
All names are first stored in an array. With N files and C columns, rows = ceil(N / C). For row r and column c, the item to print is names[r + c * rows]. So each row jumps by `rows` positions to fetch its items from successive columns. A simple loop over the array prints names in sequential order, which gives one long column (or fills one row at a time); it cannot place item 0 and item `rows` on the same line.

**Q2. Purpose of ioctl? Limitations of a fixed 80 columns?**
ioctl with TIOCGWINSZ asks the terminal driver for the current window size, so the number of columns adapts to the real terminal width. With a fixed 80, the output would waste space on wide terminals, and on narrower terminals the lines would wrap and break the alignment. It also would not adapt when the window is resized.

## Feature 4: Horizontal Display (v1.3.0)

**Q1. Compare the implementation complexity of the "down then across" (vertical) printing logic versus the "across" (horizontal) printing logic. Which one requires more pre-calculation and why?**
The vertical ("down then across") layout needs more pre-calculation. Before printing, it must compute the number of columns (terminal_width / column_width), then the number of rows (ceil(count / cols)), and then map each cell to an index with names[r + c * rows]. The horizontal layout needs only the column width and the terminal width. It prints the names in their original order and keeps a running position counter; when the next name would not fit on the current line, it prints a newline and resets the counter. No row count or index arithmetic is needed, so it is simpler.

**Q2. Describe the strategy you used in your code to manage the different display modes (-l, -x, and default). How did your program decide which function to call for printing?**
I use a single integer variable `mode` with the values MODE_DEFAULT, MODE_LONG and MODE_HORIZONTAL. In main(), the getopt() loop sets `mode` when it sees -l or -x. The value is passed to do_ls(), which checks it: if it is MODE_LONG, do_ls_long() is called. Otherwise, all names are read once into an array with read_names(), and then print_horizontal() is called for MODE_HORIZONTAL, or print_vertical() for the default mode. Finally the array is freed.

## Feature 5: Alphabetical Sort (v1.4.0)

**Q1. Why is it necessary to read all directory entries into memory before you can sort them? What are the potential drawbacks of this approach for directories containing millions of files?**
readdir() returns entries one at a time in whatever order the file system stores them, so the program cannot know which name comes first alphabetically until it has seen all of them. Sorting needs the full list available at once, so every name is first read into a dynamic array. For a directory with millions of files this has drawbacks: high memory use (every name is stored), a long delay before the first line is printed because nothing can be shown until reading and sorting are finished, and sorting time of O(n log n).

**Q2. Explain the purpose and signature of the comparison function required by qsort(). How does it work, and why must it take const void * arguments?**
The comparison function tells qsort how to order two elements. Its signature is `int cmp(const void *a, const void *b)`. It returns a negative value if a should come before b, zero if they are equal, and a positive value if a should come after b. qsort is generic and works on arrays of any type, so it cannot know the element type and passes untyped pointers; `const` promises that the function will not modify the data. Inside the function I cast the pointers to the real type. My array holds char *, so each argument points to a char *: I cast it to `const char **`, dereference once to get the string, and return strcmp(s1, s2).

## Feature 6: Colorized Output (v1.5.0)

**Q1. How do ANSI escape codes work to produce color in a standard Linux terminal? Show the specific code sequence for printing text in green.**
ANSI escape codes are special character sequences that begin with the ESC character (octal \033). The terminal does not display them; it interprets them as formatting commands. The sequence `\033[` followed by a number and `m` sets a text attribute, for example 32 for a green foreground. Everything printed afterwards uses that color until it is reset with `\033[0m`. To print text in green: `printf("\033[0;32m%s\033[0m", "hello");`

**Q2. To color an executable file, which bits in the st_mode field must be checked to determine if a file is executable by the owner, group, or others?**
The execute permission bits are S_IXUSR (0100) for the owner, S_IXGRP (0010) for the group, and S_IXOTH (0001) for others. Each is tested with the bitwise AND operator, e.g. `mode & S_IXUSR`, which is non-zero when the bit is set. A file is treated as executable if any of the three is set: `mode & (S_IXUSR | S_IXGRP | S_IXOTH)`. I also check S_ISREG(mode), because directories have execute bits too (needed to enter them) but are colored as directories instead.

## Feature 7: Recursive Listing -R (v1.6.0)

**Q1. In a recursive function, what is a "base case"? In the context of your recursive ls, what is the base case that stops the recursion from continuing forever?**
A base case is the condition under which a recursive function stops calling itself and returns directly; without it the recursion never ends. In my recursive ls, the base case is a directory that contains no subdirectories. For such a directory, the loop over its entries finds nothing for which S_ISDIR is true, so no further call to do_ls() is made and the function returns. Skipping the entries "." and ".." also prevents infinite recursion, since they point back to the same or the parent directory. Using lstat() means symbolic links are not followed as directories, which avoids cycles.

**Q2. Explain why it is essential to construct a full path (e.g., "parent_dir/subdir") before making a recursive call. What would happen if you simply called do_ls("subdir") from within the do_ls("parent_dir") function call?**
readdir() returns only the entry's name ("subdir"), not its location. opendir() and lstat() resolve a relative name against the program's current working directory, not against the directory being listed. If I called do_ls("subdir") from inside do_ls("parent_dir"), the program would look for "subdir" in the current working directory, where it usually does not exist, so opendir() would fail with "Cannot open directory" (or, worse, it could open a different directory with the same name). Building "parent_dir/subdir" gives the correct location regardless of the current working directory.