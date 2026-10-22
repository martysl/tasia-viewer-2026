using System;
using System.IO;
using System.Linq;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;

namespace TasiaViewerLauncher;

/// <summary>
/// Headless verification of the launcher logic. It shares the exact same
/// LauncherSettings and ManifestParser code as the WPF launcher, so the checks
/// cover the shipped behaviour, not a copy of it.
/// </summary>
public static class LauncherSelfTest
{
    public static async Task<int> RunAsync(string? reportPath = null)
    {
        var failures = 0;
        var log = new System.Text.StringBuilder();
        void Check(string name, bool ok, string detail = "")
        {
            log.AppendLine((ok ? "PASS " : "FAIL ") + name + (detail.Length > 0 ? " :: " + detail : ""));
            if (!ok) failures++;
        }

        try
        {
            var settings = LauncherSettings.Load();
            Check("settings load", settings != null);
            var viewer = settings.ResolveViewerPath();
            Check("viewer path resolves", viewer.Length > 0, viewer);
            var version = LauncherSettings.ReadViewerVersion(viewer);
            Check("viewer version read", version.Length > 0, version.Length > 0 ? version : "no version (viewer not staged)");
            var dir = Path.GetDirectoryName(viewer) ?? ".";
            Check("portable marker", File.Exists(Path.Combine(dir, "TasiaViewer.portable")));
            Check("nonempty package launch script", File.Exists(LauncherSettings.RequireLaunchScript(viewer)));
            var scratch = Path.Combine(Path.GetTempPath(), "tasia-launch-check-" + Guid.NewGuid());
            Directory.CreateDirectory(scratch);
            try
            {
                var fakeViewer = Path.Combine(scratch, "TasiaViewer.exe");
                bool Rejected()
                {
                    try { LauncherSettings.RequireLaunchScript(fakeViewer); return false; }
                    catch (InvalidDataException) { return true; }
                }
                Check("missing launch script rejected", Rejected());
                File.WriteAllText(Path.Combine(scratch, "Launch-TasiaViewer.ps1"), "");
                Check("empty launch script rejected", Rejected());
            }
            finally { Directory.Delete(scratch, recursive: true); }

            var settingsDir = LauncherSettings.SettingsDirectory;
            Directory.CreateDirectory(settingsDir);
            var probe = Path.Combine(settingsDir, ".selftest");
            File.WriteAllText(probe, "ok");
            File.Delete(probe);
            Check("settings directory writable", true, settingsDir);

            var manifest = ManifestParser.Parse(
                "# comment\n" +
                "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF 1024 viewer/TasiaViewer.exe\n" +
                "not-a-hash 10 bad/file\n" +
                "../../escape 10 ../outside\n" +
                "C:\\windows\\system32 10 absolute\n");
            Check("manifest parses valid rows", manifest.Count == 1, manifest.Count.ToString());
            Check("manifest rejects path traversal", manifest.All(e => !e.RelativePath.Contains("..")));
            Check("manifest rejects absolute paths", manifest.All(e => !e.RelativePath.Contains(':')));

            using var http = new HttpClient { Timeout = TimeSpan.FromSeconds(7) };
            foreach (var (name, url) in new[]
            {
                ("news", "https://api.tasiaviewer.work/viewer/updates/news.txt"),
                ("build data", "https://api.tasiaviewer.work/viewer/updates/build_data.json"),
            })
            {
                try
                {
                    var body = await http.GetStringAsync(url);
                    Check(name + " reachable", body.Trim().Length > 0, body.Length + " bytes");
                }
                catch (Exception ex)
                {
                    // The service may not be published yet; that must not fail the launcher.
                    Check(name + " optional", true, "unavailable: " + ex.GetType().Name);
                }
            }
        }
        catch (Exception ex)
        {
            log.AppendLine("FAIL self test crashed :: " + ex.GetType().Name + " " + ex.Message);
            failures++;
        }
        log.AppendLine(failures == 0 ? "SELFTEST OK" : "SELFTEST FAILURES " + failures);
        var text = log.ToString();
        if (reportPath != null)
        {
            try { File.WriteAllText(reportPath, text); }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException) { }
        }
        Console.Write(text);
        return failures == 0 ? 0 : 1;
    }
}
