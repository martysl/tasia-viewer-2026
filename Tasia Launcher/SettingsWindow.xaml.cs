using System.Windows;
using Microsoft.Win32;

namespace TasiaViewerLauncher;

public partial class SettingsWindow : Window
{
    public LauncherSettings Result { get; }

    public SettingsWindow(LauncherSettings current)
    {
        InitializeComponent();
        Result = current;
        ViewerPathInput.Text = current.ViewerPath;
        var detected = LauncherSettings.ReadViewerVersion(current.ResolveViewerPath());
        VersionInput.Text = detected.Length > 0 ? detected : current.InstalledVersion;
        VersionInput.IsReadOnly = detected.Length > 0;
        VersionInput.ToolTip = detected.Length > 0
            ? "Read from the viewer executable."
            : "Enter the installed version when the viewer cannot be read.";
        BuildUrlInput.Text = current.BuildDataUrl;
        ManifestUrlInput.Text = current.ManifestUrl;
        NewsUrlInput.Text = current.NewsUrl;
    }

    private void Browse_Click(object sender, RoutedEventArgs e)
    {
        var picker = new OpenFileDialog
        {
            Title = "Select Tasia Viewer executable",
            Filter = "Executable files (*.exe)|*.exe|All files (*.*)|*.*"
        };
        if (picker.ShowDialog(this) == true) ViewerPathInput.Text = picker.FileName;
    }

    private void Save_Click(object sender, RoutedEventArgs e)
    {
        if (string.IsNullOrWhiteSpace(ViewerPathInput.Text) || string.IsNullOrWhiteSpace(VersionInput.Text)
            || !Uri.TryCreate(BuildUrlInput.Text.Trim(), UriKind.Absolute, out var b) || b.Scheme != Uri.UriSchemeHttps
            || !Uri.TryCreate(ManifestUrlInput.Text.Trim(), UriKind.Absolute, out var m) || m.Scheme != Uri.UriSchemeHttps
            || !Uri.TryCreate(NewsUrlInput.Text.Trim(), UriKind.Absolute, out var n) || n.Scheme != Uri.UriSchemeHttps)
        {
            MessageBox.Show(this, "Enter the viewer path, version, and valid HTTPS URLs.",
                "Invalid settings", MessageBoxButton.OK, MessageBoxImage.Warning);
            return;
        }
        Result.ViewerPath = ViewerPathInput.Text.Trim();
        Result.InstalledVersion = VersionInput.Text.Trim();
        Result.BuildDataUrl = BuildUrlInput.Text.Trim();
        Result.ManifestUrl = ManifestUrlInput.Text.Trim();
        Result.NewsUrl = NewsUrlInput.Text.Trim();
        DialogResult = true;
    }
}
