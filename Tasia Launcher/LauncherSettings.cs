using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text.Json;

namespace TasiaViewerLauncher;

public sealed class LauncherSettings
{
    public string ViewerPath { get; set; } = "TasiaViewer.exe";
    // Kept for display only; the real version is read from the viewer executable.
    public string InstalledVersion { get; set; } = "";
    public string BuildDataUrl { get; set; } = "https://api.tasiaviewer.work/viewer/updates/build_data.json";
    public string ManifestUrl { get; set; } = "https://api.tasiaviewer.work/viewer/updates/files/mainifest.list";
    public string NewsUrl { get; set; } = "https://api.tasiaviewer.work/viewer/updates/news.txt";

    public static string BaseDirectory => AppContext.BaseDirectory;

    // Portable first: keep the launcher next to its viewer, like the viewer itself.
    public static string SettingsDirectory
    {
        get
        {
            var portable = Path.Combine(BaseDirectory, "TasiaViewerData");
            try
            {
                Directory.CreateDirectory(portable);
                var probe = Path.Combine(portable, ".write-probe");
                File.WriteAllText(probe, "ok");
                File.Delete(probe);
                return portable;
            }
            catch (Exception ex) when (ex is IOException or UnauthorizedAccessException)
            {
                return Path.Combine(
                    Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),
                    "TasiaViewerLauncher");
            }
        }
    }

    public static string SettingsFile => Path.Combine(SettingsDirectory, "launcher-settings.json");
    public static string CachedNewsFile => Path.Combine(SettingsDirectory, "cached-news.txt");

    public static LauncherSettings Load()
    {
        try
        {
            if (!File.Exists(SettingsFile)) return new LauncherSettings();
            var obj = JsonSerializer.Deserialize<LauncherSettings>(File.ReadAllText(SettingsFile));
            return obj ?? new LauncherSettings();
        }
        catch { return new LauncherSettings(); }
    }

    public LauncherSettings Copy() => new()
    {
        ViewerPath = ViewerPath,
        InstalledVersion = InstalledVersion,
        BuildDataUrl = BuildDataUrl,
        ManifestUrl = ManifestUrl,
        NewsUrl = NewsUrl
    };

    public void Save()
    {
        Directory.CreateDirectory(SettingsDirectory);
        var temp = SettingsFile + ".tmp";
        File.WriteAllText(temp, JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true }));
        File.Move(temp, SettingsFile, overwrite: true);
    }

    public string ResolveViewerPath()
    {
        var path = ViewerPath;
        if (string.IsNullOrWhiteSpace(path)) path = "TasiaViewer.exe";
        if (!Path.IsPathFullyQualified(path)) path = Path.Combine(BaseDirectory, path);
        return Path.GetFullPath(path);
    }

    public static string RequireLaunchScript(string viewerPath)
    {
        var script = Path.Combine(Path.GetDirectoryName(viewerPath) ?? BaseDirectory, "Launch-TasiaViewer.ps1");
        if (!File.Exists(script) || new FileInfo(script).Length < 100)
            throw new InvalidDataException("The package launch script is missing or empty. Repair this viewer package before launching.");
        return script;
    }

    // The real version comes from the executable, never from a typed-in value.
    // Windows reports numeric versions with commas ("26.9,4015,0"); they are
    // normalized to the dotted form used by the release metadata.
    public static string NormalizeVersion(string value)
    {
        var text = (value ?? "").Trim();
        if (text.Length == 0) return "";
        return text.Replace(',', '.');
    }

    public static string ReadViewerVersion(string viewerPath)
    {
        try
        {
            if (!File.Exists(viewerPath)) return "";
            var info = FileVersionInfo.GetVersionInfo(viewerPath);
            var product = NormalizeVersion(info.ProductVersion);
            var file = NormalizeVersion(info.FileVersion);
            if (product.Length > 0 && product != "0.0.0.0") return product;
            return file.Length > 0 && file != "0.0.0.0" ? file : "";
        }
        catch { return ""; }
    }

    public string InstalledVersionOrEmpty()
    {
        var detected = ReadViewerVersion(ResolveViewerPath());
        return detected.Length > 0 ? detected : InstalledVersion;
    }
}

public sealed record ManifestEntry(string Sha256, long Size, string RelativePath);

public static class ManifestParser
{
    // Format, one file per line: <sha256> <size> <relative path>
    public static List<ManifestEntry> Parse(string raw)
    {
        var entries = new List<ManifestEntry>();
        if (string.IsNullOrWhiteSpace(raw)) return entries;
        foreach (var line in raw.Replace("\r\n", "\n").Split('\n'))
        {
            var trimmed = line.Trim();
            if (trimmed.Length == 0 || trimmed.StartsWith('#')) continue;
            var parts = trimmed.Split(' ', 3);
            if (parts.Length != 3) continue;
            if (parts[0].Length != 64 || !parts[0].All(Uri.IsHexDigit)) continue;
            if (!long.TryParse(parts[1], out var size) || size < 0) continue;
            var path = parts[2].Trim();
            if (path.Length == 0 || path.Length > 512) continue;
            // Never trust a server path: it must stay inside the update folder.
            if (path.Contains("..") || path.Contains(':') || path.StartsWith('/') || path.StartsWith('\\'))
                continue;
            entries.Add(new ManifestEntry(parts[0].ToLowerInvariant(), size, path.Replace('\\', '/')));
        }
        return entries;
    }
}
