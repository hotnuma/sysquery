#include <query.h>
#include <prc_query.h>

#include <stdlib.h>
#include <string.h>
#include <print.h>

static int _app_exit(bool usage, int ret)
{
    if (usage)
    {
        print("*** usage");
        print("sysquery -dev");
        print("sudo sysquery -prc");
    }

    return ret;
}

int main(int argc, char **argv)
{
    //return prc_query();

    if (argc < 2)
        return _app_exit(true, EXIT_FAILURE);

    int n = 1;

    while (n < argc)
    {
        const char *part = argv[n];

        if (strcmp(part, "-dev") == 0)
        {
            return (pkg_dev() == true ? EXIT_SUCCESS : EXIT_FAILURE);
        }
        else if (strcmp(part, "-prc") == 0)
        {
            return (prc_query() == true ? EXIT_SUCCESS : EXIT_FAILURE);
        }
        else
        {
            return _app_exit(true, EXIT_FAILURE);
        }

        ++n;
    }

    return 0;
}


