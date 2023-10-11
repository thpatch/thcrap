using Microsoft.WindowsAPICodePack.Dialogs;
using Microsoft.Xaml.Behaviors;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;
using Xceed.Wpf.Toolkit;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page4.xaml
    /// </summary>
    public partial class Page4 : UserControl
    {

        private Timer timer;

        private class GameJs
        {
            public string Title { get; set; }
        }

        private class Game : INotifyPropertyChanged
        {
            private readonly Page4 _parentWindow;
            public readonly string GameId;
            private List<string> Paths { get; set; }
            public string SelectedPath { get; private set; }
            private List<Control> _contextMenu;
            public List<Control> ContextMenu { get {
                if (_contextMenu != null) return _contextMenu;
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
                    return _contextMenu;
                } }

            private void SelectPath(object sender, RoutedEventArgs e)
            {
                var newMenu = sender as MenuItem;

                foreach (var control in _contextMenu)
                {
                    if (control is MenuItem menu && menu.IsCheckable && menu != newMenu)
                        menu.IsChecked = false;
                }

                if (newMenu == null) return;
                newMenu.IsChecked = true;

                SelectedPath = newMenu.Header.ToString();
            }

            private void RemoveFromList(object sender, RoutedEventArgs e)
            {
                _parentWindow.games.Remove(this);
                _parentWindow.Refresh();
            }

            public Game(Page4 parentWindow, string gameId, string path, bool wasAlreadyPresent)
            {
                this._parentWindow = parentWindow;
                GameId = gameId;
                Paths = new List<string>() { path };
                SelectedPath = path;
                NewVisibility = Visibility.Hidden;
                IsSelected = true;
                WasAlreadyPresent = wasAlreadyPresent;
            }

            private string _name = null;
            public string Name
            {
                get
                {
                    if (_name != null) return _name ?? (_name = GameId);
                    var stringDefinitions = _parentWindow.GetStringdef();
                    
                    if (stringDefinitions != null)
                    {
                        if (GameId.EndsWith("_custom") &&
                            stringDefinitions.TryGetValue(GameId.Remove(GameId.Length - "_custom".Length),
                                out var gameDisplayName))
                        {
                            if (stringDefinitions.TryGetValue(gameDisplayName + " config", out var customDisplayName))
                                _name = customDisplayName;
                            else
                                _name = gameDisplayName + " (Configuration)";
                        }
                        else
                        {
                            if (stringDefinitions.TryGetValue(GameId, out var value))
                                _name = value;
                        }
                    }
                    
                    if (_name != null) return _name ?? (_name = GameId);

                    var runConfig = ThcrapHelper.stack_json_resolve<GameJs>(GameId + ".js");
                    if (runConfig != null)
                        _name = runConfig.Title;

                    return _name ?? (_name = GameId);
                }
            }

            public ImageSource GameIcon { get; private set; }
            public async Task LoadGameIcon() => await Task.Run(() =>
            {
                var path = SelectedPath;
                if (path.EndsWith(@"/vpatch.exe"))
                {
                    // Try to find the game, in order to have a nice icon
                    var gamePath = path.Replace("/vpatch.exe", "/" + GameId + ".exe");
                    if (File.Exists(gamePath))
                        path = gamePath;
                    else if (GameId == "th06")
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
                        _parentWindow.Dispatcher.Invoke(() => GameIcon = Imaging.CreateBitmapSourceFromHIcon(icon.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions()));
                }
                catch
                {
                    // ignored
                }
            });

            public void AddPath(string path)
            {
                if (Paths.Contains(path))
                    return;

                Paths.Add(path);
                _contextMenu = null;
                SetNew(true);
            }

            public string Path => SelectedPath;

            public Visibility NewVisibility { get; private set; }
            public void SetNew(bool isNew)
            {
                var newVisibility = isNew ? Visibility.Visible : Visibility.Hidden;
                if (newVisibility == NewVisibility) return;
                NewVisibility = newVisibility;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(NewVisibility)));
            }

            private bool _isSelected;
            public bool IsSelected
            {
                get => _isSelected;
                set
                {
                    _isSelected = value;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsSelected)));
                }
            }

            // True if this game was already present in games.js, false otherwise
            public bool WasAlreadyPresent { get; private set; }

            public event PropertyChangedEventHandler PropertyChanged;
        }

        private WizardPage wizardPage;
        private ObservableCollection<Game> games;

        public Page4()
        {
            InitializeComponent();
        }

        private void Refresh()
        {
            if (games.Count == 0)
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

        private IReadOnlyDictionary<string, string> _stringdef = null;

        private IReadOnlyDictionary<string, string> GetStringdef()
        {
            return _stringdef ??
                   // ReSharper disable once StringLiteralTypo
                   (_stringdef = ThcrapHelper.stack_json_resolve<Dictionary<string, string>>("stringdefs.js"));
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
                foreach (var entry in games_js.Select(it => new Game(this, it.Key, it.Value, true)))
                {
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

        private static void SaveGamesJs(IEnumerable<Game> games)
        {
            var dict = new SortedDictionary<string, string>();
            foreach (var it in games)
                dict[it.GameId] = it.SelectedPath;

            var json = JsonSerializer.Serialize(dict, new JsonSerializerOptions { WriteIndented = true });
            try {
                File.WriteAllText("config/games.js", json);
            } catch(IOException e) {
                System.Windows.MessageBox.Show($"Failed to write games.js ({e.Message})", "Error");
            }
        }

        public async Task Enter(WizardPage newWizardPage)
        {
            wizardPage = newWizardPage;
            games = await LoadGamesJs();
            NoNewGamesFound.Visibility = Visibility.Collapsed;

            GamesControl.ItemsSource = games;
            Refresh();

            if (games.Count == 0)
            {
                await Search(null, true);    
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
        private async Task Search(string root, bool useAutoBehavior = false)
        {
            if (timer != null)
            {
                timer.Dispose();
                timer = null;
            }

            var gamesListWasEmpty = games.Count == 0;
            var newGamesWereFound = false;
            foreach (var it in games)
                it.SetNew(false);

            SetSearching(true);
            IntPtr foundPtr;
            if (useAutoBehavior)
            {
                foundPtr = await Task.Run(() => ThcrapDll.SearchForGamesInstalled(null));
            }
            else
            {
                foundPtr = await Task.Run(() => ThcrapDll.SearchForGames(new string[] { root, null }, null));
            }

            SetSearching(false);

            var found = ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.game_search_result>(foundPtr);

            foreach (var it in found)
            {
                var game = games.FirstOrDefault(x => x.GameId == it.id);
                if (game == null)
                {
                    // new game
                    game = new Game(this, it.id, ThcrapHelper.PtrToStringUTF8(it.path), false);
                    await game.LoadGameIcon();
                    if (!gamesListWasEmpty)
                        game.SetNew(true);
                    games.Add(game);
                    newGamesWereFound = true;
                }
                else
                {
                    // game already exists
                    game.AddPath(ThcrapHelper.PtrToStringUTF8(it.path));
                }
            }

            ThcrapDll.SearchForGames_free(foundPtr);

            if (!gamesListWasEmpty)
            {
                GamesScroll.ScrollToBottom();
            }

            if (!newGamesWereFound)
            {
                NoNewGamesFound.Visibility = Visibility.Visible;

                // Hide the message after 3 second
                timer = new Timer((object state) =>
                {
                    this.Dispatcher.Invoke(() =>
                    {
                        NoNewGamesFound.Visibility = Visibility.Collapsed;
                    });
                }, null, 3000, Timeout.Infinite);
            }

            Refresh();
        }

        private void SelectAll(object sender, RoutedEventArgs e)
        {
            foreach (var game in games)
            {
                game.IsSelected = true;
            }
        }

        private void UnselectAll(object sender, RoutedEventArgs e)
        {
            foreach (var game in games)
            {
                game.IsSelected = false;
            }
        }

        private void RemoveAll(object sender, RoutedEventArgs e)
        {
            games.Clear();
        }

        private async void SearchAuto(object sender, RoutedEventArgs e)
        {
            await Search(null, true);
        }

        private async void SearchDirectory(object sender, RoutedEventArgs e)
        {
            var dialog = new CommonOpenFileDialog()
            {
                IsFolderPicker = true
            };
            if (dialog.ShowDialog(Window.GetWindow(this)) == CommonFileDialogResult.Ok)
            {
                await Search(dialog.FileName);
            }
        }

        private async void SearchEverywhere(object sender, RoutedEventArgs e)
        {
            await Search(null);
        }

        private void SearchCancel(object sender, RoutedEventArgs e)
        {
            ThcrapDll.SearchForGames_cancel();
        }

        private IEnumerable<ThcrapDll.games_js_entry> outGames;

        public void Leave()
        {
            SaveGamesJs(games.Where((Game game) => game.IsSelected || game.WasAlreadyPresent));
            outGames = games.Where((Game game) => game.IsSelected).Select((Game game) => new ThcrapDll.games_js_entry
            {
                id = game.GameId,
                path = ThcrapHelper.StringUTF8ToPtr(game.SelectedPath),
            });
        }

        public IEnumerable<ThcrapDll.games_js_entry> GetGames()
        {
            return outGames;
        }
    }

    // From https://stackoverflow.com/questions/8958946/how-to-open-a-popup-menu-when-a-button-is-clicked/20710436
    public class Page4DropDownButtonBehavior : Behavior<Button>
    {
        private bool _isContextMenuOpen;

        protected override void OnAttached()
        {
            base.OnAttached();
            AssociatedObject.AddHandler(ButtonBase.ClickEvent, new RoutedEventHandler(AssociatedObject_Click), true);
        }

        private void AssociatedObject_Click(object sender, RoutedEventArgs e)
        {
            var source = sender as Button;
            if (source?.ContextMenu == null) return;
            if (_isContextMenuOpen) return;
            // Add handler to detect when the ContextMenu closes
            source.ContextMenu.AddHandler(ContextMenu.ClosedEvent, new RoutedEventHandler(ContextMenu_Closed), true);
            // If there is a drop-down assigned to this button, then position and display it
            source.ContextMenu.PlacementTarget = source;
            source.ContextMenu.Placement = PlacementMode.Bottom;
            source.ContextMenu.IsOpen = true;
            _isContextMenuOpen = true;
        }

        protected override void OnDetaching()
        {
            base.OnDetaching();
            AssociatedObject.RemoveHandler(ButtonBase.ClickEvent, new RoutedEventHandler(AssociatedObject_Click));
        }

        private void ContextMenu_Closed(object sender, RoutedEventArgs e)
        {
            _isContextMenuOpen = false;
            var contextMenu = sender as ContextMenu;
            if (contextMenu != null)
            {
                contextMenu.RemoveHandler(ContextMenu.ClosedEvent, new RoutedEventHandler(ContextMenu_Closed));
            }
        }
    }
}
