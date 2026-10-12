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