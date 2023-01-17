/* {{ProjectName}} by {{Your Name}} */
#include <cstdio>
#include <string>

void version(void)
{
    std::puts("{{ProjectName}} by {{Your Name}} Version 0.0.0");
}

void usage(void)
{
    std::puts(
        "Usage: {{ProjectName}} [OPTIONS] filename\n"
        "\n"
        "Options:\n"
        "  --help      Display this message.\n"
        "  --version   Display the version information.");
}

struct ConsoleApp
{
    int m_flags;
    int m_ret;
    std::string m_file;

    ConsoleApp() : m_flags(0), m_ret(0)
    {
    }

    bool parse_command_line(int argc, char **argv);
    int run(void);
};

int ConsoleApp::run(void)
{
    std::printf("Just do it!\n");
    return m_ret;
}

bool ConsoleApp::parse_command_line(int argc, char **argv)
{
    for (int iarg = 1; iarg < argc; ++iarg)
    {
        std::string arg = argv[iarg];

        if (arg == "--help")
        {
            usage();
            return false;
        }

        if (arg == "--version")
        {
            version();
            return false;
        }

        // TODO: Add command line options more...

        if (arg[0] == '-')
        {
            std::printf("ERROR: invalid argument - '%s'\n", arg.c_str());
            m_ret = 1;
            return false;
        }

        if (m_file.empty())
        {
            m_file = arg;
        }
        else
        {
            std::printf("ERROR: Too many arguments - '%s'\n", arg.c_str());
            m_ret = 1;
            return false;
        }
    }

    return true;
}

int main(int argc, char **argv)
{
    if (argc <= 1)
    {
        usage();
        return 1;
    }

    ConsoleApp console;

    if (console.parse_command_line(argc, argv))
    {
        return console.run();
    }

    return console.m_ret;
}
