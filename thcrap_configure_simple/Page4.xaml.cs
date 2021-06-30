using Microsoft.WindowsAPICodePack.Dialogs;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
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
using System.Windows.Threading;
using Xceed.Wpf.Toolkit;

namespace thcrap_configure_simple
{
    /// <summary>
    /// Interaction logic for Page4.xaml
    /// </summary>
    public partial class Page4 : UserControl
    {
        class ThXX_js
        {
            public string title { get; set; }
        }
        class Game : INotifyPropertyChanged
        {
            private Page4 parentWindow;
            public ThcrapDll.games_js_entry game;

            public Game(Page4 parentWindow, ThcrapDll.games_js_entry game)
            {
                this.game = game;
                this.parentWindow = parentWindow;
                this.NewVisibility = Visibility.Hidden;
            }

            private string name_ = null;
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
                            if (stringdefs.TryGetValue(game.id, out value))
                                name_ = value;
                            else if (game.id.EndsWith("_custom") && stringdefs.TryGetValue(game.id.Remove(game.id.Length - "_custom".Length), out value))
                                name_ = value + " (configuration)";
                        }
                    }

                    if (name_ == null)
                    {
                        var runconfig = ThcrapHelper.stack_json_resolve<ThXX_js>(game.id + ".js");
                        if (runconfig != null)
                            name_ = runconfig.title;
                    }

                    if (name_ == null)
                        name_ = game.id;

                    return name_;
                }
            }

            public ImageSource GameIcon { get; private set; }
            public async Task LoadGameIcon() => await Task.Run(() =>
            {
                string path = game.path;
                if (path.EndsWith("/vpatch.exe"))
                {
                    // Try to find the game, in order to have a nice icon
                    string gamePath = path.Replace("/vpatch.exe", "/" + game.id + ".exe");
                    if (File.Exists(gamePath))
                        path = gamePath;
                }

                var icon = Icon.ExtractAssociatedIcon(path);
                if (icon != null)
                    parentWindow.Dispatcher.Invoke(() => GameIcon = Imaging.CreateBitmapSourceFromHIcon(icon.Handle, Int32Rect.Empty, BitmapSizeOptions.FromEmptyOptions()));
            });

            public string Path { get => game.path; }

            public Visibility NewVisibility { get; private set; }
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

            public event PropertyChangedEventHandler PropertyChanged;
        }

        WizardPage wizardPage;
        ObservableCollection<Game> games;

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
            }
            else
            {
                wizardPage.CanSelectNextPage = true;
                AddGamesNotice.Visibility = Visibility.Collapsed;
            }
        }

        Dictionary<string, string> stringdef_ = null;
        Dictionary<string, string> GetStringdef()
        {
            if (stringdef_ == null)
                stringdef_ = ThcrapHelper.stack_json_resolve<Dictionary<string, string>>("stringdefs.js");
            return stringdef_;
        }

        async Task<ObservableCollection<Game>> LoadGamesJs()
        {
            try
            {
                var games = new ObservableCollection<Game>();
                var games_js = await JsonSerializer.DeserializeAsync<Dictionary<string, string>>(File.OpenRead("config/games.js"));
                foreach (var it in games_js)
                {
                    var entry = new Game(this, new ThcrapDll.games_js_entry
                    {
                        id = it.Key,
                        path = it.Value
                    });
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
        void SaveGamesJs(ObservableCollection<Game> games)
        {
            var dict = new SortedDictionary<string, string>();
            foreach (var it in games)
                dict[it.game.id] = it.game.path;

            var json = JsonSerializer.Serialize(dict, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText("config/games.js", json);
        }

        public async Task Enter(WizardPage wizardPage)
        {
            this.wizardPage = wizardPage;
            this.games = await LoadGamesJs();

            GamesControl.ItemsSource = games;
            Refresh();
        }

        private void RemoveGame(object sender, RoutedEventArgs e)
        {
            games.Remove((sender as Button).DataContext as Game);
            Refresh();
        }

        private void SetSearching(bool state)
        {
            if (state)
            {
                wizardPage.CanSelectPreviousPage = false;
                wizardPage.CanSelectNextPage = false;
                ProgressBar.Visibility = Visibility.Visible;
                SearchButton1.IsEnabled = false;
                SearchButton2.IsEnabled = false;
            }
            else
            {
                wizardPage.CanSelectPreviousPage = true;
                wizardPage.CanSelectNextPage = true;
                ProgressBar.Visibility = Visibility.Hidden;
                SearchButton1.IsEnabled = true;
                SearchButton2.IsEnabled = true;
            }
        }
        private async void Search(string root)
        {
            bool gamesListWasEmpty = this.games.Count == 0;
            foreach (var it in this.games)
                it.SetNew(false);

            ThcrapDll.games_js_entry[] games = new ThcrapDll.games_js_entry[this.games.Count + 1];
            int i = 0;
            foreach (var it in this.games)
            {
                games[i] = it.game;
                i++;
            }
            games[i] = new ThcrapDll.games_js_entry();

            SetSearching(true);
            IntPtr foundPtr = await Task.Run(() => ThcrapDll.SearchForGames(root, games));
            SetSearching(false);

            var found = ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.game_search_result>(foundPtr);

            string last = "";
            foreach (var it in found)
            {
                if (it.id == last)
                    continue;
                last = it.id;

                var game = new Game(this, new ThcrapDll.games_js_entry()
                {
                    id = it.id,
                    path = ThcrapHelper.PtrToStringUTF8(it.path),
                });
                await game.LoadGameIcon();
                if (!gamesListWasEmpty)
                    game.SetNew(true);
                this.games.Add(game);
            }

            ThcrapDll.SearchForGames_free(foundPtr);

            if (!gamesListWasEmpty)
                GamesScroll.ScrollToBottom();
            Refresh();
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

        public List<ThcrapDll.games_js_entry> Leave()
        {
            SaveGamesJs(this.games);
            return this.games.ToList().ConvertAll((Game game) => game.game);
        }
    }
}
