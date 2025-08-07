using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using thcrap_cs_lib;
using ShortcutDestinations = thcrap_cs_lib.GlobalConfig.ShortcutDestinations;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page5.xaml
    /// </summary>
    public partial class Page5 : UserControl
    {
        bool? isUTLPresent = null;
        GlobalConfig config = null;

        public Page5()
        {
            InitializeComponent();
        }

        public void Enter()
        {
            warningImage.Source = Imaging.CreateBitmapSourceFromHIcon(
                SystemIcons.Warning.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions());

            if (File.Exists("bin\\Universal THCRAP Launcher.exe"))
                warningImage.Visibility = Visibility.Collapsed;

            if (config == null)
            {
                config = GlobalConfig.get();
                ShortcutDestinations dest = config.default_shortcut_destinations;

                checkboxDesktopGames.IsChecked      = dest.HasFlag(ShortcutDestinations.Desktop);
                checkboxStartMenuGames.IsChecked    = dest.HasFlag(ShortcutDestinations.StartMenu);
                checkboxGamesFolder.IsChecked  = dest.HasFlag(ShortcutDestinations.GamesFolder);
                checkboxThcrapFolder.IsChecked = dest.HasFlag(ShortcutDestinations.ThcrapFolder);
            }

            if (config.developer_mode)
            {
                shortcutTypePanel.Visibility = Visibility.Visible;
                shortcutType.SelectedIndex = (int)config.shortcuts_type;
            }
        }

        private void checkbox_Checked(object sender, RoutedEventArgs e)
        {
            if (warningPanel == null)
                return;

            if (isUTLPresent == null)
                isUTLPresent = File.Exists("bin\\Universal THCRAP Launcher.exe");

            if (checkboxDesktopGames.IsChecked == true || checkboxStartMenuGames.IsChecked == true ||
                checkboxGamesFolder.IsChecked == true || checkboxThcrapFolder.IsChecked == true || isUTLPresent == true)
                warningPanel.Visibility = Visibility.Collapsed;
            else
                warningPanel.Visibility = Visibility.Visible;
        }

        private void NoShortcutsMoreDetails(object sender, MouseButtonEventArgs e)
        {
            MessageBox.Show("With the way thcrap works, your game files are never modified, and running the games after setup program will still run the original, unpatched games.\n\n" +
                            "If you want something more like traditional patches with a patched exe aside the original one in the game folder, choose to create the shortcuts \"in the games' folder\".",
                            "More details regarding launching Touhou games with thcrap", MessageBoxButton.OK, MessageBoxImage.Information);
        }

        private void CreateShortcuts(string configName, IEnumerable<ThcrapDll.games_js_entry> games, ThcrapDll.ShortcutsDestination destination)
        {
            ThcrapDll.games_js_entry[] gamesArray = new ThcrapDll.games_js_entry[games.Count() + 1];
            int i = 0;
            foreach (var it in games)
            {
                gamesArray[i] = it;
                i++;
            }
            gamesArray[i] = new ThcrapDll.games_js_entry();

            ThcrapDll.CreateShortcuts(configName, gamesArray, destination, config.shortcuts_type);
        }

        public void Leave(string configName, IEnumerable<ThcrapDll.games_js_entry> games)
        {
            config.default_shortcut_destinations =
                (checkboxDesktopGames.IsChecked      == true ? ShortcutDestinations.Desktop      : 0) |
                (checkboxStartMenuGames.IsChecked    == true ? ShortcutDestinations.StartMenu    : 0) |
                (checkboxGamesFolder.IsChecked  == true ? ShortcutDestinations.GamesFolder  : 0) |
                (checkboxThcrapFolder.IsChecked == true ? ShortcutDestinations.ThcrapFolder : 0);
            if (config.developer_mode)
            {
                config.shortcuts_type = (ThcrapDll.ShortcutsType)shortcutType.SelectedIndex;
            }
            config.Save();

            if (checkboxDesktopGames.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_DESKTOP);
            if (checkboxStartMenuGames.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_START_MENU);
            if (checkboxGamesFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_GAMES_DIRECTORY);
            if (checkboxThcrapFolder.IsChecked == true)
                CreateShortcuts(configName, games, ThcrapDll.ShortcutsDestination.SHDESTINATION_THCRAP_DIR);
        }
    }
}
