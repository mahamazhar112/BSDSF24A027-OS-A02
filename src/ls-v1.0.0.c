#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#define MODE_DEFAULT    0
#define MODE_LONG       1
#define MODE_HORIZONTAL 2

/* ANSI escape codes */
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[0;34m"
#define COLOR_GREEN   "\033[0;32m"
#define COLOR_RED     "\033[0;31m"
#define COLOR_PINK    "\033[0;35m"
#define COLOR_REVERSE "\033[7m"

extern int errno;

int use_color = 0;      /* color only when output is a terminal */
int recursive_flag = 0; /* set by -R */

void do_ls(const char *dir, int mode);
int get_term_width(void);
int read_names(const char *dir, char ***names, int *count, size_t *maxlen);
int compare_names(const void *a, const void *b);
void print_vertical(const char *dir, char **names, int count, size_t maxlen);
void print_horizontal(const char *dir, char **names, int count, size_t maxlen);
void free_names(char **names, int count);
void do_ls_long(const char *dir, char **names, int count);
void print_long(const char *dir, const char *name,
                int w_link, int w_user, int w_group, int w_size);
void mode_to_string(mode_t mode, char *s);
const char *get_color(const char *name, mode_t mode);
void print_colored(const char *name, mode_t mode);
void print_entry(const char *dir, const char *name, int width);
int ends_with(const char *s, const char *suffix);

int main(int argc, char *argv[])
{
    int opt;
    int mode = MODE_DEFAULT;

    while ((opt = getopt(argc, argv, "lxR")) != -1)
    {
        switch (opt)
        {
        case 'l':
            mode = MODE_LONG;
            break;
        case 'x':
            mode = MODE_HORIZONTAL;
            break;
        case 'R':
            recursive_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [-x] [-R] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    use_color = isatty(STDOUT_FILENO);

    if (optind == argc)
    {
        do_ls(".", mode);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            /* with -R, do_ls prints its own "dir:" header */
            if (!recursive_flag)
                printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], mode);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int mode)
{
    char **names = NULL;
    int count = 0;
    size_t maxlen = 0;

    if (read_names(dir, &names, &count, &maxlen) == -1)
        return;

    if (recursive_flag)
        printf("%s:\n", dir);

    if (count > 1)
        qsort(names, count, sizeof(char *), compare_names);

    if (mode == MODE_LONG)
        do_ls_long(dir, names, count);
    else if (mode == MODE_HORIZONTAL)
        print_horizontal(dir, names, count, maxlen);
    else
        print_vertical(dir, names, count, maxlen);

    /* recursion: visit every sub-directory of this directory */
    if (recursive_flag)
    {
        for (int i = 0; i < count; i++)
        {
            if (strcmp(names[i], ".") == 0 || strcmp(names[i], "..") == 0)
                continue;

            /* build the FULL path: dir/name (avoid a double slash) */
            char path[PATH_MAX];
            size_t l = strlen(dir);
            const char *sep = (l > 0 && dir[l - 1] == '/') ? "" : "/";
            snprintf(path, sizeof(path), "%s%s%s", dir, sep, names[i]);

            struct stat st;
            if (lstat(path, &st) == -1)
            {
                perror(path);
                continue;
            }
            if (S_ISDIR(st.st_mode))
            {
                putchar('\n');
                do_ls(path, mode); /* recursive call */
            }
        }
    }

    free_names(names, count);
}

int compare_names(const void *a, const void *b)
{
    const char *s1 = *(const char **)a;
    const char *s2 = *(const char **)b;
    return strcmp(s1, s2);
}

int get_term_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80; /* fallback */
    return w.ws_col;
}

int read_names(const char *dir, char ***names, int *count, size_t *maxlen)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return -1;
    }

    char **list = NULL;
    int n = 0, capacity = 0;
    size_t longest = 0;

    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (n == capacity)
        {
            capacity = capacity ? capacity * 2 : 16;
            char **tmp = realloc(list, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            list = tmp;
        }
        list[n] = strdup(entry->d_name);
        if (list[n] == NULL)
        {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
        size_t len = strlen(entry->d_name);
        if (len > longest)
            longest = len;
        n++;
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    *names = list;
    *count = n;
    *maxlen = longest;
    return 0;
}

/* ---------- color helpers ---------- */

int ends_with(const char *s, const char *suffix)
{
    size_t ls = strlen(s), lx = strlen(suffix);
    if (lx > ls)
        return 0;
    return strcmp(s + ls - lx, suffix) == 0;
}

const char *get_color(const char *name, mode_t mode)
{
    if (S_ISLNK(mode))
        return COLOR_PINK;
    if (S_ISDIR(mode))
        return COLOR_BLUE;
    if (S_ISCHR(mode) || S_ISBLK(mode) || S_ISFIFO(mode) || S_ISSOCK(mode))
        return COLOR_REVERSE;
    if (ends_with(name, ".tar") || ends_with(name, ".gz") ||
        ends_with(name, ".zip"))
        return COLOR_RED;
    if (S_ISREG(mode) && (mode & (S_IXUSR | S_IXGRP | S_IXOTH)))
        return COLOR_GREEN;
    return "";
}

void print_colored(const char *name, mode_t mode)
{
    const char *color = use_color ? get_color(name, mode) : "";
    if (color[0] != '\0')
        printf("%s%s%s", color, name, COLOR_RESET);
    else
        printf("%s", name);
}

void print_entry(const char *dir, const char *name, int width)
{
    struct stat st;
    char path[PATH_MAX];
    mode_t m = 0;

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == 0)
        m = st.st_mode;

    print_colored(name, m);

    int pad = width - (int)strlen(name);
    if (pad > 0)
        printf("%*s", pad, "");
}

/* ---------- display functions ---------- */

void print_vertical(const char *dir, char **names, int count, size_t maxlen)
{
    if (count == 0)
        return;

    int term_width = get_term_width();
    int col_width = (int)maxlen + 2;
    int cols = term_width / col_width;
    if (cols < 1)
        cols = 1;
    int rows = (count + cols - 1) / cols;

    for (int r = 0; r < rows; r++)
    {
        for (int c = 0; c < cols; c++)
        {
            int i = r + c * rows;
            if (i >= count)
                break;
            print_entry(dir, names[i], col_width);
        }
        putchar('\n');
    }
}

void print_horizontal(const char *dir, char **names, int count, size_t maxlen)
{
    int term_width = get_term_width();
    int col_width = (int)maxlen + 2;
    int pos = 0;

    for (int i = 0; i < count; i++)
    {
        if (pos > 0 && pos + col_width > term_width)
        {
            putchar('\n');
            pos = 0;
        }
        print_entry(dir, names[i], col_width);
        pos += col_width;
    }
    if (count > 0)
        putchar('\n');
}

void free_names(char **names, int count)
{
    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

void do_ls_long(const char *dir, char **names, int count)
{
    struct stat st;
    char path[PATH_MAX];
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;
    long total = 0;

    for (int i = 0; i < count; i++)
    {
        snprintf(path, sizeof(path), "%s/%s", dir, names[i]);
        if (lstat(path, &st) == -1)
        {
            perror(path);
            continue;
        }
        total += st.st_blocks;

        struct passwd *pw = getpwuid(st.st_uid);
        struct group *gr = getgrgid(st.st_gid);
        int l;

        l = snprintf(NULL, 0, "%lu", (unsigned long)st.st_nlink);
        if (l > w_link) w_link = l;
        l = pw ? (int)strlen(pw->pw_name) : snprintf(NULL, 0, "%u", st.st_uid);
        if (l > w_user) w_user = l;
        l = gr ? (int)strlen(gr->gr_name) : snprintf(NULL, 0, "%u", st.st_gid);
        if (l > w_group) w_group = l;
        l = snprintf(NULL, 0, "%lld", (long long)st.st_size);
        if (l > w_size) w_size = l;
    }

    printf("total %ld\n", total / 2);

    for (int i = 0; i < count; i++)
        print_long(dir, names[i], w_link, w_user, w_group, w_size);
}

void print_long(const char *dir, const char *name,
                int w_link, int w_user, int w_group, int w_size)
{
    struct stat st;
    char path[PATH_MAX];
    char perms[11];

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    mode_to_string(st.st_mode, perms);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);

    printf("%s %*lu ", perms, w_link, (unsigned long)st.st_nlink);
    if (pw) printf("%-*s ", w_user, pw->pw_name);
    else    printf("%-*u ", w_user, st.st_uid);
    if (gr) printf("%-*s ", w_group, gr->gr_name);
    else    printf("%-*u ", w_group, st.st_gid);
    printf("%*lld ", w_size, (long long)st.st_size);

    char *t = ctime(&st.st_mtime);
    time_t now = time(NULL);
    if (st.st_mtime < now - 15778800 || st.st_mtime > now)
        printf("%.6s  %.4s ", t + 4, t + 20);
    else
        printf("%.12s ", t + 4);

    print_colored(name, st.st_mode);

    if (S_ISLNK(st.st_mode))
    {
        char target[PATH_MAX];
        ssize_t n = readlink(path, target, sizeof(target) - 1);
        if (n != -1)
        {
            target[n] = '\0';
            printf(" -> %s", target);
        }
    }
    putchar('\n');
}

void mode_to_string(mode_t mode, char *s)
{
    if (S_ISREG(mode))       s[0] = '-';
    else if (S_ISDIR(mode))  s[0] = 'd';
    else if (S_ISLNK(mode))  s[0] = 'l';
    else if (S_ISCHR(mode))  s[0] = 'c';
    else if (S_ISBLK(mode))  s[0] = 'b';
    else if (S_ISFIFO(mode)) s[0] = 'p';
    else if (S_ISSOCK(mode)) s[0] = 's';
    else                     s[0] = '?';

    s[1] = (mode & S_IRUSR) ? 'r' : '-';
    s[2] = (mode & S_IWUSR) ? 'w' : '-';
    s[3] = (mode & S_IXUSR) ? 'x' : '-';
    s[4] = (mode & S_IRGRP) ? 'r' : '-';
    s[5] = (mode & S_IWGRP) ? 'w' : '-';
    s[6] = (mode & S_IXGRP) ? 'x' : '-';
    s[7] = (mode & S_IROTH) ? 'r' : '-';
    s[8] = (mode & S_IWOTH) ? 'w' : '-';
    s[9] = (mode & S_IXOTH) ? 'x' : '-';

    if (mode & S_ISUID) s[3] = (mode & S_IXUSR) ? 's' : 'S';
    if (mode & S_ISGID) s[6] = (mode & S_IXGRP) ? 's' : 'S';
    if (mode & S_ISVTX) s[9] = (mode & S_IXOTH) ? 't' : 'T';
    s[10] = '\0';
}