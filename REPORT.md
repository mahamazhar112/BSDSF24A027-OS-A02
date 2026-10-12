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