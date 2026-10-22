using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Input;
using System.Windows.Media;

namespace TasiaViewerLauncher;

public partial class MainWindow : Window
{
    private static readonly HttpClient Http = new() { Timeout = TimeSpan.FromSeconds(7) };
    private LauncherSettings _settings = LauncherSettings.Load();
    private bool _checking;
    private List<ManifestEntry> _manifest = new();

    public MainWindow()
    {
        InitializeComponent();
        RefreshInstalledVersion();
    }

    private void RefreshInstalledVersion()
    {
        var version = _settings.InstalledVersionOrEmpty();
        InstalledVersionText.Text = version.Length > 0 ? version : "not detected";
        LauncherVersionBadge.Text = version.Length > 0 ? version : "PREVIEW";
    }

    private async void Window_Loaded(object sender, RoutedEventArgs e)
    {
        // The endpoints may not exist yet. Failure must never prevent launching the viewer.
        await Task.WhenAll(ReadNewsAsync(), CheckUpdatesAsync());
    }

    private async Task ReadNewsAsync()
    {
        try
        {
            var news = (await Http.GetStringAsync(_settings.NewsUrl)).Trim();
            if (news.Length == 0) throw new InvalidDataException("Empty news");
            if (news.Length > 3500) news = news[..3500];
            ApplyNews(news);
            NewsStatusText.Text = "Latest news · live server";
            try
            {
                Directory.CreateDirectory(LauncherSettings.SettingsDirectory);
                File.WriteAllText(LauncherSettings.CachedNewsFile, news);
            }
            catch (IOException) { /* Cache is optional. */ }
            catch (UnauthorizedAccessException) { /* Cache is optional. */ }
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or InvalidDataException or UriFormatException)
        {
            if (File.Exists(LauncherSettings.CachedNewsFile))
            {
                try
                {
                    ApplyNews(File.ReadAllText(LauncherSettings.CachedNewsFile));
                    NewsStatusText.Text = "Cached news · server unavailable";
                    return;
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
            NewsStatusText.Text = "Preview news · server unavailable";
        }
    }

    private void ApplyNews(string raw)
    {
        // news.txt is plain UTF-8 text. Never evaluate it as HTML or commands.
        var lines = raw.Replace("\r\n", "\n").Split('\n', 2);
        NewsTitleText.Text = lines[0].Length <= 120 ? lines[0] : lines[0][..120];
        NewsBodyText.Text = lines.Length > 1 && !string.IsNullOrWhiteSpace(lines[1])
            ? lines[1].Trim()
            : "Visit the Tasia Viewer website for the full announcement.";
    }

    private static int CompareVersions(string current, string latest)
    {
        // Keep release versions numeric (e.g. 1.0.1). Windows reports the
        // executable version with commas, so both sides are normalized first.
        var a = LauncherSettings.NormalizeVersion(current);
        var b = LauncherSettings.NormalizeVersion(latest);
        if (Version.TryParse(a, out var va) && Version.TryParse(b, out var vb))
            return va.CompareTo(vb);
        return string.Equals(a, b, StringComparison.Ordinal) ? 0 : -1;
    }

    private async Task CheckUpdatesAsync()
    {
        if (_checking) return;
        _checking = true;
        CheckUpdatesButton.IsEnabled = false;
        UpdateStatusText.Text = "Checking…";
        UpdateDetailsText.Text = "";
        try
        {
            var json = await Http.GetStringAsync(_settings.BuildDataUrl);
            using var doc = JsonDocument.Parse(json);
            if (!doc.RootElement.TryGetProperty("version", out var v) || v.ValueKind != JsonValueKind.String)
                throw new InvalidDataException("Missing version in build_data.json");
            var latest = v.GetString() ?? "";
            var current = _settings.InstalledVersionOrEmpty();
            if (CompareVersions(current, latest) < 0)
            {
                UpdateStatusText.Text = $"Update available: {latest}";
                UpdateStatusText.Foreground = new SolidColorBrush(Color.FromRgb(237, 205, 133));
                NewsStatusText.Text = "An update is available.";
            }
            else
            {
                UpdateStatusText.Text = "Up to date";
                UpdateStatusText.Foreground = new SolidColorBrush(Color.FromRgb(153, 222, 200));
            }
            await LoadManifestAsync();
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or JsonException or InvalidDataException or UriFormatException)
        {
            UpdateStatusText.Text = "Update server unavailable";
            UpdateStatusText.Foreground = new SolidColorBrush(Color.FromRgb(190, 185, 209));
            UpdateDetailsText.Text = "The release server could not be reached. The viewer still launches normally.";
        }
        finally
        {
            CheckUpdatesButton.IsEnabled = true;
            _checking = false;
        }
    }

    private async Task LoadManifestAsync()
    {
        try
        {
            var raw = await Http.GetStringAsync(_settings.ManifestUrl);
            _manifest = ManifestParser.Parse(raw);
            var bytes = _manifest.Sum(e => e.Size);
            UpdateDetailsText.Text = _manifest.Count == 0
                ? "The release manifest is empty."
                : $"Update payload: {_manifest.Count} file(s), {bytes / 1024.0 / 1024.0:0.0} MB. Installation stays manual and verified.";
        }
        catch (Exception ex) when (ex is HttpRequestException or TaskCanceledException or UriFormatException)
        {
            _manifest.Clear();
            UpdateDetailsText.Text = "The file manifest is unavailable.";
        }
    }

    private async void CheckUpdates_Click(object sender, RoutedEventArgs e) => await CheckUpdatesAsync();

    private async void Launch_Click(object sender, RoutedEventArgs e)
    {
        try
        {
            var path = _settings.ResolveViewerPath();
            if (!File.Exists(path))
            {
                MessageBox.Show(this, "Viewer executable not found. Open Settings and select TasiaViewer.exe.\n\n" + path,
                    "Tasia Viewer", MessageBoxButton.OK, MessageBoxImage.Information);
                return;
            }
            var directory = Path.GetDirectoryName(path) ?? AppContext.BaseDirectory;
            // The package ships a key launcher that supplies the media provider keys.
            // Starting the executable directly would silently disable GIPHY/KLIPY.
            var keyLauncher = LauncherSettings.RequireLaunchScript(path);
                using var launchProcess = Process.Start(new ProcessStartInfo
                {
                    FileName = Path.Combine(
                        Environment.GetFolderPath(Environment.SpecialFolder.System),
                        "WindowsPowerShell", "v1.0", "powershell.exe"),
                    Arguments = "-NoProfile -NonInteractive -ExecutionPolicy Bypass -WindowStyle Hidden -File \"" +
                                keyLauncher + "\"",
                    WorkingDirectory = directory,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }) ?? throw new InvalidOperationException("The launch process was not created.");
                await launchProcess.WaitForExitAsync();
                if (launchProcess.ExitCode != 0)
                    throw new InvalidOperationException("Viewer launch validation failed. Check this package's executable hash and encrypted media file, or close this copy if it is already running.");
            Close();
        }
        catch (Exception ex)
        {
            MessageBox.Show(this, "Could not launch viewer:\n" + ex.Message,
                "Tasia Viewer", MessageBoxButton.OK, MessageBoxImage.Error);
        }
    }

    private async void Settings_Click(object sender, RoutedEventArgs e)
    {
        var dialog = new SettingsWindow(_settings.Copy()) { Owner = this };
        if (dialog.ShowDialog() != true) return;
        _settings = dialog.Result;
        try { _settings.Save(); }
        catch (Exception ex) { MessageBox.Show(this, "Settings could not be saved: " + ex.Message); }
        RefreshInstalledVersion();
        await Task.WhenAll(ReadNewsAsync(), CheckUpdatesAsync());
    }

    private void DragWindow(object sender, MouseButtonEventArgs e)
    {
        if (e.LeftButton == MouseButtonState.Pressed) DragMove();
    }

    private void Close_Click(object sender, RoutedEventArgs e) => Close();
    private void Minimize_Click(object sender, RoutedEventArgs e) => WindowState = WindowState.Minimized;
}
