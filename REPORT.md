# REPORT

## Feature 2: Long Listing (v1.1.0)

**Q1. stat() vs lstat()?**
stat() follows a symbolic link and returns the metadata of the file it points to. lstat() does not follow it and returns the metadata of the link itself. In ls, lstat() is the right choice, because ls -l must show a symlink as type `l` with its own permissions and size, and print `name -> target`. Using stat() would hide that the entry is a link.

**Q2. st_mode bitwise operations?**
st_mode packs the file type and permissions in one integer. The file type is in the upper bits, and `mode & S_IFMT` isolates it; comparing the result to `S_IFDIR` or `S_IFREG` tells the type (the macros S_ISDIR(mode), S_ISREG(mode) do exactly this). The permission bits are in the lower bits, and `mode & S_IRUSR` is non-zero only if the owner read bit is set. The same works for S_IWUSR, S_IXUSR, and the GRP/OTH variants. Special bits S_ISUID, S_ISGID, S_ISVTX are tested the same way.