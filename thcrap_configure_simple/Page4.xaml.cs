using Microsoft.WindowsAPICodePack.Dialogs;
using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
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
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using Xceed.Wpf.Toolkit;

namespace thcrap_configure_simple
{
    /// <summary>
    /// Interaction logic for Page4.xaml
    /// </summary>
    public partial class Page4 : UserControl
    {
        class Game
        {
            public ThcrapDll.games_js_entry game;
            public string Text { get => game.id + ": " + game.path; }
            public Game(ThcrapDll.games_js_entry game)
            {
                this.game = game;
            }
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

        ObservableCollection<Game> LoadGamesJs()
        {
            try
            {
                var games = new ObservableCollection<Game>();
                var games_js = JsonSerializer.Deserialize<Dictionary<string, string>>(File.ReadAllText("config/games.js"));
                foreach (var it in games_js)
                {
                    var entry = new Game(new ThcrapDll.games_js_entry
                    {
                        id = it.Key,
                        path = it.Value
                    });
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

        public void Enter(WizardPage wizardPage)
        {
            this.wizardPage = wizardPage;
            this.games = LoadGamesJs();

            GamesControl.ItemsSource = games;
            Refresh();
        }

        private void RemoveGame(object sender, RoutedEventArgs e)
        {
            games.Remove((sender as Button).DataContext as Game);
            Refresh();
        }

        private async void Search(string root)
        {
            ThcrapDll.games_js_entry[] games = new ThcrapDll.games_js_entry[this.games.Count + 1];
            int i = 0;
            foreach (var it in this.games)
            {
                games[i] = it.game;
                i++;
            }
            games[i] = new ThcrapDll.games_js_entry();

            wizardPage.CanSelectPreviousPage = false;
            wizardPage.CanSelectNextPage = false;
            ProgressBar.Visibility = Visibility.Visible;
            IntPtr foundPtr = await Task.Run(() => ThcrapDll.SearchForGames(root, games));
            wizardPage.CanSelectPreviousPage = true;
            wizardPage.CanSelectNextPage = true;
            ProgressBar.Visibility = Visibility.Hidden;

            var found = ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.game_search_result>(foundPtr);

            string last = "";
            foreach (var it in found)
            {
                if (it.id == last)
                    continue;
                last = it.id;

                this.games.Add(new Game(new ThcrapDll.games_js_entry()
                {
                    id = it.id,
                    path = ThcrapHelper.PtrToStringUTF8(it.path),
                }));
            }

            ThcrapDll.SearchForGames_free(foundPtr);
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
