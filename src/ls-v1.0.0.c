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

extern int errno;

void do_ls(const char *dir, int long_flag);
void do_ls_long(const char *dir);
void print_long(const char *dir, const char *name,
                int w_link, int w_user, int w_group, int w_size);
void mode_to_string(mode_t mode, char *s);
void do_ls_columns(const char *dir);
int get_term_width(void);

int main(int argc, char *argv[])
{
    int opt;
    int long_flag = 0;

    while ((opt = getopt(argc, argv, "l")) != -1)
    {
        switch (opt)
        {
        case 'l':
            long_flag = 1;
            break;
        default:
            fprintf(stderr, "Usage: %s [-l] [dir...]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (optind == argc)
    {
        do_ls(".", long_flag);
    }
    else
    {
        for (int i = optind; i < argc; i++)
        {
            printf("Directory listing of %s : \n", argv[i]);
            do_ls(argv[i], long_flag);
            puts("");
        }
    }
    return 0;
}

void do_ls(const char *dir, int long_flag)
{
    if (long_flag)
        do_ls_long(dir);
    else
        do_ls_columns(dir);
}

int get_term_width(void)
{
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == -1 || w.ws_col == 0)
        return 80; /* fallback */
    return w.ws_col;
}

void do_ls_columns(const char *dir)
{
    struct dirent *entry;
    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    char **names = NULL;
    int count = 0, capacity = 0;
    size_t maxlen = 0;

    /* Step 1: read all names into a dynamic array */
    errno = 0;
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        if (count == capacity)
        {
            capacity = capacity ? capacity * 2 : 16;
            char **tmp = realloc(names, capacity * sizeof(char *));
            if (tmp == NULL)
            {
                perror("realloc");
                exit(EXIT_FAILURE);
            }
            names = tmp;
        }
        names[count] = strdup(entry->d_name);
        if (names[count] == NULL)
        {
            perror("strdup");
            exit(EXIT_FAILURE);
        }
        size_t len = strlen(entry->d_name);
        if (len > maxlen)
            maxlen = len;
        count++;
    }
    if (errno != 0)
        perror("readdir failed");
    closedir(dp);

    /* Step 2: calculate layout */
    if (count > 0)
    {
        int term_width = get_term_width();
        int col_width = (int)maxlen + 2;
        int cols = term_width / col_width;
        if (cols < 1)
            cols = 1;
        int rows = (count + cols - 1) / cols; /* ceiling division */

        /* Step 3: print down then across */
        for (int r = 0; r < rows; r++)
        {
            for (int c = 0; c < cols; c++)
            {
                int i = r + c * rows;
                if (i >= count)
                    break;
                printf("%-*s", col_width, names[i]);
            }
            putchar('\n');
        }
    }

    for (int i = 0; i < count; i++)
        free(names[i]);
    free(names);
}

void do_ls_long(const char *dir)
{
    struct dirent *entry;
    struct stat st;
    char path[PATH_MAX];
    int w_link = 0, w_user = 0, w_group = 0, w_size = 0;
    long total = 0;

    DIR *dp = opendir(dir);
    if (dp == NULL)
    {
        fprintf(stderr, "Cannot open directory : %s\n", dir);
        return;
    }

    /* Pass 1: find column widths and the "total" blocks */
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        snprintf(path, sizeof(path), "%s/%s", dir, entry->d_name);
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

    /* Pass 2: print every entry */
    rewinddir(dp);
    while ((entry = readdir(dp)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;
        print_long(dir, entry->d_name, w_link, w_user, w_group, w_size);
    }
    closedir(dp);
}

void print_long(const char *dir, const char *name,
                int w_link, int w_user, int w_group, int w_size)
{
    struct stat st;
    char path[PATH_MAX];
    char mode[11];

    snprintf(path, sizeof(path), "%s/%s", dir, name);
    if (lstat(path, &st) == -1)
    {
        perror(path);
        return;
    }

    mode_to_string(st.st_mode, mode);
    struct passwd *pw = getpwuid(st.st_uid);
    struct group *gr = getgrgid(st.st_gid);

    printf("%s %*lu ", mode, w_link, (unsigned long)st.st_nlink);
    if (pw) printf("%-*s ", w_user, pw->pw_name);
    else    printf("%-*u ", w_user, st.st_uid);
    if (gr) printf("%-*s ", w_group, gr->gr_name);
    else    printf("%-*u ", w_group, st.st_gid);
    printf("%*lld ", w_size, (long long)st.st_size);

    /* ctime() gives "Wed Jun 30 21:49:08 1993\n" */
    char *t = ctime(&st.st_mtime);
    time_t now = time(NULL);
    if (st.st_mtime < now - 15778800 || st.st_mtime > now)
        printf("%.6s  %.4s ", t + 4, t + 20);   /* old file: show year */
    else
        printf("%.12s ", t + 4);                /* recent: show time */

    printf("%s", name);

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
