using Microsoft.WindowsAPICodePack.Dialogs;
using Microsoft.Xaml.Behaviors;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Xceed.Wpf.Toolkit;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page4.xaml
    /// </summary>
    public partial class Page4 : UserControl
    {
        private Timer autoHideTimer;
        private ObservableCollection<Game> gameList;
        private IEnumerable<ThcrapDll.games_js_entry> outGames;
        private Dictionary<string, string> stringdef_ = null;
        private WizardPage wizardPage;

        public Page4()
        {
            InitializeComponent();
        }

        public async Task Enter(WizardPage wizardPage)
        {
            this.wizardPage = wizardPage;
            this.gameList = await LoadGamesJs();
            NoNewGamesFound.Visibility = Visibility.Collapsed;

            GamesControl.ItemsSource = gameList;
            Refresh();

            if (gameList.Count == 0)
            {
                Search(null, true, true);
            }
        }

        public IEnumerable<ThcrapDll.games_js_entry> GetGames()
        {
            return outGames;
        }

        public void Leave()
        {
            SaveGamesJs(this.gameList.Where((Game game) => game.IsSelected || game.WasAlreadyPresent));
            outGames = this.gameList.Where((Game game) => game.IsSelected).Select((Game game) => new ThcrapDll.games_js_entry
            {
                id = game.game_id,
                path = ThcrapHelper.StringUTF8ToPtr(game.SelectedPath),
            });
        }

        private Dictionary<string, string> GetStringdef()
        {
            return stringdef_ ?? (stringdef_ = ThcrapHelper.stack_json_resolve<Dictionary<string, string>>("stringdefs.js"));
        }

        private async Task<ObservableCollection<Game>> LoadGamesJs()
        {
            try
            {
                var games = new ObservableCollection<Game>();
                var games_js_file = File.OpenRead("config/games.js");
                var games_js = await JsonSerializer.DeserializeAsync<Dictionary<string, string>>(
                    games_js_file,
                    new JsonSerializerOptions() { AllowTrailingCommas = true, ReadCommentHandling = JsonCommentHandling.Skip }
                    );
                games_js_file.Dispose();
                foreach (var it in games_js)
                {
                    var entry = new Game(this, it.Key, it.Value, true);
                    await entry.LoadGameIcon();
                    games.Add(entry);
                }
                return games;
            }
            catch
            {
                return new ObservableCollection<Game>();
            }
        }

        private void Refresh()
        {
            if (gameList.Count == 0)
            {
                wizardPage.CanSelectNextPage = false;
                AddGamesNotice.Visibility = Visibility.Visible;
                ButtonSelectAll.IsEnabled = false;
                ButtonUnselectAll.IsEnabled = false;
                ButtonRemoveAll.IsEnabled = false;
            }
            else
            {
                wizardPage.CanSelectNextPage = true;
                AddGamesNotice.Visibility = Visibility.Collapsed;
                ButtonSelectAll.IsEnabled = true;
                ButtonUnselectAll.IsEnabled = true;
                ButtonRemoveAll.IsEnabled = true;
            }
        }

        private void RemoveAll(object sender, RoutedEventArgs e)
        {
            this.gameList.Clear();
        }

        private void SaveGamesJs(IEnumerable<Game> games)
        {
            var dict = new SortedDictionary<string, string>();
            foreach (var it in games)
                dict[it.game_id] = it.SelectedPath;

            var json = JsonSerializer.Serialize(dict, new JsonSerializerOptions { WriteIndented = true });
            try
            {
                File.WriteAllText("config/games.js", json);
            }
            catch (System.IO.IOException e)
            {
                System.Windows.MessageBox.Show(String.Format("Failed to write games.js ({0})", e.Message), "Error");
            }
        }

        private void Search(string root, bool useAutoBehavior = false, bool firstSearch = false)
        {
            NoNewGamesFound.Visibility = Visibility.Collapsed;
            if (autoHideTimer != null)
            {
                autoHideTimer.Dispose();
                autoHideTimer = null;
            }

            bool gamesListWasEmpty = this.gameList.Count == 0;
            bool newGamesWereFound = false;
            foreach (var it in this.gameList)
            {
                it.SetNew(false);
            }

            SetSearching(true);
            IntPtr foundPtr = default;
            if (useAutoBehavior)
            {
                Task.Run(() => foundPtr = ThcrapDll.SearchForGamesInstalled(null)).Wait();
            }
            else
            {
                Task.Run(() => foundPtr = ThcrapDll.SearchForGames(new string[] { root, null }, null)).Wait();
            }

            SetSearching(false);

            if (!Object.Equals(foundPtr, default(IntPtr)))
            {
                foreach (var it in ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.game_search_result>(foundPtr))
                {
                    var game = this.gameList.FirstOrDefault(x => x.game_id == it.id);
                    if (game == null)
                    {
                        // new game
                        game = new Game(this, it.id, ThcrapHelper.PtrToStringUTF8(it.path), false);
                        game.LoadGameIcon().Wait();
                        if (!gamesListWasEmpty)
                        {
                            game.SetNew(true);
                        }

                        this.gameList.Add(game);
                        newGamesWereFound = true;
                    }
                    else
                    {
                        // game already exists
                        game.AddPath(ThcrapHelper.PtrToStringUTF8(it.path));
                    }
                }

                if (!newGamesWereFound && !firstSearch)
                {
                    NoNewGamesAutoHideProgress.Value = 100;
                    NoNewGamesFound.Visibility = Visibility.Visible;

                    // Update the progress bar slowly going down then collapse message
                    autoHideTimer = new Timer(new TimerCallback((object state) =>
                    {
                        Dispatcher.Invoke(() =>
                        {
                            NoNewGamesAutoHideProgress.Value--;
                            if (NoNewGamesAutoHideProgress.Value == 0)
                            {
                                NoNewGamesFound.Visibility = Visibility.Collapsed;
                                autoHideTimer.Dispose();
                                autoHideTimer = null;
                            }
                        });
                    }), null, 1, 30);
                }

                ThcrapDll.SearchForGames_free(foundPtr);

                if (!gamesListWasEmpty)
                    GamesScroll.ScrollToBottom();
                Refresh();
            }
        }

        private void SearchAuto(object sender, RoutedEventArgs e)
        {
            Search(null, true);
        }

        private void SearchCancel(object sender, RoutedEventArgs e)
        {
            ThcrapDll.SearchForGames_cancel();
        }

        private void SearchDirectory(object sender, RoutedEventArgs e)
        {
            var dialog = new CommonOpenFileDialog()
            {
                IsFolderPicker = true
            };
            if (dialog.ShowDialog(Window.GetWindow(this)) == CommonFileDialogResult.Ok)
            {
                Search(dialog.FileName);
            }
        }

        private void SearchEverywhere(object sender, RoutedEventArgs e)
        {
            Search(null);
        }

        private void SelectAll(object sender, RoutedEventArgs e)
        {
            foreach (var game in this.gameList)
            {
                game.IsSelected = true;
            }
        }

        private void SetSearching(bool state)
        {
            if (state)
            {
                wizardPage.CanSelectPreviousPage = false;
                wizardPage.CanSelectNextPage = false;
                ProgressBar.Visibility = Visibility.Visible;
                SearchButtonCancel.Visibility = Visibility.Visible;
                SearchButtonAuto.IsEnabled = false;
                SearchButtonDirectory.IsEnabled = false;
                SearchButtonEverywhere.IsEnabled = false;
            }
            else
            {
                wizardPage.CanSelectPreviousPage = true;
                wizardPage.CanSelectNextPage = true;
                ProgressBar.Visibility = Visibility.Hidden;
                SearchButtonCancel.Visibility = Visibility.Hidden;
                SearchButtonAuto.IsEnabled = true;
                SearchButtonDirectory.IsEnabled = true;
                SearchButtonEverywhere.IsEnabled = true;
            }
        }

        private void UnselectAll(object sender, RoutedEventArgs e)
        {
            foreach (var game in this.gameList)
            {
                game.IsSelected = false;
            }
        }

        private class Game : INotifyPropertyChanged
        {
            public readonly string game_id;
            private readonly Page4 parentWindow;
            private List<Control> _contextMenu;
            private bool _isSelected;
            private string name_ = null;

            public Game(Page4 parentWindow, string game_id, string path, bool wasAlreadyPresent)
            {
                this.parentWindow = parentWindow;
                this.game_id = game_id;
                this.Paths = new List<string>() { path };
                this.SelectedPath = path;
                this.NewVisibility = Visibility.Hidden;
                this.IsSelected = true;
                this.WasAlreadyPresent = wasAlreadyPresent;
            }

            public event PropertyChangedEventHandler PropertyChanged;

            public List<Control> ContextMenu
            {
                get
                {
                    if (_contextMenu == null)
                    {
                        _contextMenu = new List<Control>();
                        _contextMenu.AddRange(Paths.Select(path =>
                        {
                            var itemPath = new MenuItem()
                            {
                                Header = path,
                                IsCheckable = true,
                                IsChecked = (path == SelectedPath),
                            };
                            itemPath.Click += SelectPath;
                            return itemPath;
                        }));

                        _contextMenu.Add(new Separator());

                        var itemRemove = new MenuItem()
                        {
                            Header = "Remove from this list",
                        };
                        itemRemove.Click += RemoveFromList;
                        _contextMenu.Add(itemRemove);
                    }
                    return _contextMenu;
                }
            }

            public ImageSource GameIcon { get; private set; }

            public bool IsSelected
            {
                get => _isSelected;
                set
                {
                    _isSelected = value;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
                }
            }

            public string Name
            {
                get
                {
                    if (name_ == null)
                    {
                        var stringdefs = parentWindow.GetStringdef();
                        if (stringdefs != null)
                        {
                            string value;
                            if (stringdefs.TryGetValue(game_id, out value))
                                name_ = value;
                            else if (game_id.EndsWith("_custom") && stringdefs.TryGetValue(game_id.Remove(game_id.Length - "_custom".Length), out value))
                                name_ = value + " (configuration)";
                        }
                    }

                    if (name_ == null)
                    {
                        var runconfig = ThcrapHelper.stack_json_resolve<ThXX_js>(game_id + ".js");
                        if (runconfig != null)
                            name_ = runconfig.title;
                    }

                    if (name_ == null)
                        name_ = game_id;

                    return name_;
                }
            }

            public Visibility NewVisibility { get; private set; }
            public string Path { get => SelectedPath; }
            public List<string> Paths { get; private set; }
            public string SelectedPath { get; private set; }

            // True if this game was already present in games.js, false otherwise
            public bool WasAlreadyPresent { get; private set; }

            public void AddPath(string path)
            {
                if (Paths.Contains(path))
                    return;

                Paths.Add(path);
                _contextMenu = null;
                SetNew(true);
            }

            public async Task LoadGameIcon() => await Task.Run(() =>
            {
                string path = SelectedPath;
                if (path.EndsWith("/vpatch.exe"))
                {
                    // Try to find the game, in order to have a nice icon
                    string gamePath = path.Replace("/vpatch.exe", "/" + game_id + ".exe");
                    if (File.Exists(gamePath))
                        path = gamePath;
                    else if (game_id == "th06")
                    {
                        // Special case - EoSD
                        gamePath = path.Replace("/vpatch.exe", "/東方紅魔郷.exe");
                        if (File.Exists(gamePath))
                            path = gamePath;
                    }
                }

                try
                {
                    var icon = Icon.ExtractAssociatedIcon(path);
                    if (icon != null)
                        parentWindow.Dispatcher.Invoke(() => GameIcon = Imaging.CreateBitmapSourceFromHIcon(icon.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions()));
                }
                catch
                { }
            });

            public void SetNew(bool isNew)
            {
                Visibility newVisibility;
                if (isNew)
                    newVisibility = Visibility.Visible;
                else
                    newVisibility = Visibility.Hidden;
                if (newVisibility != NewVisibility)
                {
                    NewVisibility = newVisibility;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NewVisibility)));
                }
            }

            private void RemoveFromList(object sender, RoutedEventArgs e)
            {
                parentWindow.gameList.Remove(this);
                parentWindow.Refresh();
            }

            private void SelectPath(object sender, RoutedEventArgs e)
            {
                var newMenu = sender as MenuItem;

                foreach (var control in _contextMenu)
                {
                    if (control is MenuItem menu && menu.IsCheckable && menu != newMenu)
                        menu.IsChecked = false;
                }
                newMenu.IsChecked = true;

                SelectedPath = newMenu.Header.ToString();
            }
        }

        private class ThXX_js
        {
            public string title { get; set; }
        }
    }

    // From https://stackoverflow.com/questions/8958946/how-to-open-a-popup-menu-when-a-button-is-clicked/20710436
    public class Page4DropDownButtonBehavior : Behavior<Button>
    {
        private bool isContextMenuOpen;

        protected override void OnAttached()
        {
            base.OnAttached();
            AssociatedObject.AddHandler(Button.ClickEvent, new RoutedEventHandler(AssociatedObject_Click), true);
        }

        protected override void OnDetaching()
        {
            base.OnDetaching();
            AssociatedObject.RemoveHandler(Button.ClickEvent, new RoutedEventHandler(AssociatedObject_Click));
        }

        private void AssociatedObject_Click(object sender, System.Windows.RoutedEventArgs e)
        {
            Button source = sender as Button;
            if (source != null && source.ContextMenu != null)
            {
                if (!isContextMenuOpen)
                {
                    // Add handler to detect when the ContextMenu closes
                    source.ContextMenu.AddHandler(ContextMenu.ClosedEvent, new RoutedEventHandler(ContextMenu_Closed), true);
                    // If there is a drop-down assigned to this button, then position and display it
                    source.ContextMenu.PlacementTarget = source;
                    source.ContextMenu.Placement = PlacementMode.Bottom;
                    source.ContextMenu.IsOpen = true;
                    isContextMenuOpen = true;
                }
            }
        }

        private void ContextMenu_Closed(object sender, RoutedEventArgs e)
        {
            isContextMenuOpen = false;
            var contextMenu = sender as ContextMenu;
            if (contextMenu != null)
            {
                contextMenu.RemoveHandler(ContextMenu.ClosedEvent, new RoutedEventHandler(ContextMenu_Closed));
            }
        }
    }
}
