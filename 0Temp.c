#if 0

char buffer[1024];
unsigned int dummy;

CStringAuto *filename = cstr_new_size(32);
cstr_fmt(filename, "/proc/%d/status", item->pid);

FILE *file = fopen(c_str(filename), "r");

if (!file)
    return false;

while (fgets (buffer, sizeof(buffer), file) != NULL)
{
    // UIDs in order: real, effective, saved set, and filesystem
    if (sscanf(buffer, "Uid:\t%u\t%u\t%u\t%u",
               &dummy,
               &item->uid,
               &dummy,
               &dummy) == 1)
        break;
}

fclose (file);

// ----------------------------------------------------------------------------

if (exists)
{
    if (_have_pss)
    {
        memitem->shared += item->shared;
    }
    else if (memitem->shared < item->shared)
    {
        memitem->shared = item->shared;
    }

    if (memitem->shared_huge < item->shared_huge)
    {
        memitem->shared_huge = item->shared_huge;
    }
}
else
{
    memitem->shared = item->shared;
    memitem->shared_huge = item->shared_huge;
}

#define CMDSIZE 1024
char _cmdline[CMDSIZE];

static bool get_task_cmdline(char *result, int size, const char *filename)
{
    FILE *file = fopen (filename, "r");
    if (!file)
        return false;

    /* Read full command byte per byte until EOF */
    int i;
    int c;
    for (i = 0; (c = fgetc (file)) != EOF && i < (int) size - 1; i++)
        result[i] = (c == '\0') ? ' ' : (char) c;

    result[i] = '\0';

    if (result[i-1] == ' ')
        result[i-1] = '\0';

    fclose (file);

    /* Kernel processes don't have a cmdline nor an exec path */
    if (i == 0)
        return false;

    return true;
}

#endif

