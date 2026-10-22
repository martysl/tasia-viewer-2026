using System;
using System.IO;
using System.Threading.Tasks;

namespace TasiaViewerLauncher;

public static class Program
{
    public static async Task<int> Main(string[] args)
    {
        var report = args.Length > 0
            ? args[0]
            : Path.Combine(AppContext.BaseDirectory, "launcher-selftest.txt");
        return await LauncherSelfTest.RunAsync(report);
    }
}
