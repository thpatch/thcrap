using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Diagnostics;
using System.Globalization;
using System.Linq;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Forms.VisualStyles;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page2_advanced.xaml
    /// </summary>
    public partial class Page2_advanced : UserControl
    {
        public string SearchEmoji => Environment.OSVersion.Version.Major >= 10 ? "\ud83d\udd0e" : "L";
        public string SearchCrossEmoji => Environment.OSVersion.Version.Major >= 10 ? "\u274c" : "r";

        public FontFamily EmojiWebdingsFontFamily => Environment.OSVersion.Version.Major >= 10 ? FontFamily : new FontFamily("Webdings");

        private int isUnedited = 1;
        private int configMaxLength = 0;

        private void ResetConfigName()
        {
            ConfigName.Text = "";
            isUnedited = 1;
        }

        private void AddToConfigName(string patchName)
        {
            if (isUnedited > 0)
            {
                isUnedited++;
                if (ConfigName.Text == "")
                {
                    ConfigName.Text = patchName;
                }
                else
                {
                    ConfigName.Text += "-" + patchName;
                }
            }
        }

        private void RemoveFromConfigName(string patchName)
        {
            if (isUnedited <= 0)
                return;

            string configName = ConfigName.Text;
            int idx = configName.IndexOf(patchName);
            if (idx == -1)
                return;

            configName = configName.Remove(idx, patchName.Length);
            if (idx < configName.Length && configName[idx] == '-')
                configName = configName.Remove(idx, 1);
            else if (idx > 0 && configName[idx - 1] == '-')
                configName = configName.Remove(idx - 1, 1);

            isUnedited++;
            ConfigName.Text = configName;
        }

        public class RepoPatch : INotifyPropertyChanged
        {
            public thcrap_configure_v3.RepoPatch SourcePatch { get; set; }

            public string RightArrow => Environment.OSVersion.Version.Major >= 10 ? "\ud83e\udc7a" : "è";
            public string LeftArrow => Environment.OSVersion.Version.Major >= 10 ? "\ud83e\udc78" : "ç";
            public string UpArrow => Environment.OSVersion.Version.Major >= 10 ? "\ud83e\udc79" : "é";
            public string DownArrow => Environment.OSVersion.Version.Major >= 10 ? "\ud83e\udc7b" : "ê";
            public FontFamily EmojiWingingds3FontFamily => Environment.OSVersion.Version.Major >= 10 ? SystemFonts.MessageFontFamily : new FontFamily("Wingdings 3");

            private bool isSelected = false;
            private bool isVisibleWithSearch = true;
            private bool _isFirst;
            private bool _isLast;

            public bool IsFirst
            {
                get => _isFirst;
                set
                {
                    if (_isFirst != value)
                    {
                        _isFirst = value;
                        OnPropertyChanged(nameof(IsFirst));
                        OnPropertyChanged(nameof(IsNotFirst)); // Notify when IsFirst changes
                    }
                }
            }

            public bool IsLast
            {
                get => _isLast;
                set
                {
                    if (_isLast != value)
                    {
                        _isLast = value;
                        OnPropertyChanged(nameof(IsLast));
                        OnPropertyChanged(nameof(IsNotLast)); // Notify when IsLast changes
                    }
                }
            }

            // New properties for inverted logic
            public bool IsNotFirst => !IsFirst;
            public bool IsNotLast => !IsLast;

            protected virtual void OnPropertyChanged(string propertyName)
            {
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(propertyName));
            }


            public RepoPatch(thcrap_configure_v3.RepoPatch patch)
            {
                SourcePatch = patch;
            }

            public Visibility VisibilityInTree
            {
                get { return isVisibleWithSearch && !isSelected ? Visibility.Visible : Visibility.Collapsed; }
            }

            public bool IsSelected() => isSelected;

            public void Select(bool newState)
            {
                isSelected = newState;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VisibilityInTree)));
            }

            public bool UpdateVisibilityWithSearch(string filter)
            {
                bool newIsVisible;

                newIsVisible = SourcePatch.Id.ToLower().Contains(filter) ||
                               SourcePatch.Title.ToLower().Contains(filter);
                if (newIsVisible != isVisibleWithSearch)
                {
                    isVisibleWithSearch = newIsVisible;
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VisibilityInTree)));
                }

                return newIsVisible;
            }

            public void SetVisibilityWithSearch()
            {
                isVisibleWithSearch = true;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VisibilityInTree)));
            }

            public event PropertyChangedEventHandler PropertyChanged;
        }

        public class Repo : INotifyPropertyChanged
        {
            public thcrap_configure_v3.Repo SourceRepo { get; private set; }
            public List<RepoPatch> Patches { get; private set; }

            private bool isVisible = true;

            public Repo(thcrap_configure_v3.Repo repo)
            {
                SourceRepo = repo;
                Patches = repo.Patches.ConvertAll((thcrap_configure_v3.RepoPatch patch) => new RepoPatch(patch));
            }

            public Visibility VisibilityInTree
            {
                get { return isVisible ? Visibility.Visible : Visibility.Collapsed; }
            }

            public void SetVisibility(bool v)
            {
                isVisible = v;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VisibilityInTree)));
            }

            public bool UpdateFilter(string filter)
            {
                bool selfMatch = SourceRepo.Id.ToLower().Contains(filter) ||
                                 SourceRepo.Title.ToLower().Contains(filter);
                bool patchesVisible = false;
                foreach (RepoPatch patch in Patches)
                {
                    if (selfMatch)
                    {
                        patch.SetVisibilityWithSearch();
                        patchesVisible = true;
                    }
                    else
                    {
                        if (patch.UpdateVisibilityWithSearch(filter))
                            patchesVisible = true;
                    }
                }

                return patchesVisible;
            }

            public event PropertyChangedEventHandler PropertyChanged;
        }

        ObservableCollection<RepoPatch> selectedPatches = new ObservableCollection<RepoPatch>();

        public Page2_advanced()
        {
            InitializeComponent();
            this.DataContext = this;
        }

        public void SetRepoList(List<thcrap_configure_v3.Repo> repoList)
        {
            AvailablePatches.ItemsSource = repoList.ConvertAll((thcrap_configure_v3.Repo repo) => new Repo(repo));
            SelectedPatches.ItemsSource = selectedPatches;
        }

        public List<thcrap_configure_v3.RepoPatch> GetSelectedRepoPatch() =>
            selectedPatches.ToList().ConvertAll((RepoPatch patch) => patch.SourcePatch);

        private void SelectPatch(RepoPatch patch)
        {
            AddToConfigName(patch.SourcePatch.Id);
            patch.Select(true);
            selectedPatches.Add(patch);
            UpdateFirstAndLastFlags();
        }

        private void UpdateFirstAndLastFlags()
        {
            for (var i = 0; i < selectedPatches.Count; i++)
            {
                selectedPatches[i].IsFirst = (i == 0);
                selectedPatches[i].IsLast = (i == selectedPatches.Count - 1);
            }
        }

        private void AvailablePatchDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (!(e.OriginalSource is FrameworkElement clickedItem))
                return;

            if (!(clickedItem.DataContext is RepoPatch patch))
                return;

            SelectPatch(patch);
        }

        private void AvailablePatchesMoveRight(object sender, RoutedEventArgs e)
        {
            RepoPatch selectedPatch = null;
            if (sender is Button button && button.Tag is RepoPatch patch)
            {
                selectedPatch = patch;
            }
            else if (AvailablePatches.SelectedItem is RepoPatch patch2 && !patch2.IsSelected())
            {
                selectedPatch = patch2;
            }

            if (selectedPatch == null)
                return;

            SelectPatch(selectedPatch);
        }

        public void SetInitialPatch(thcrap_configure_v3.RepoPatch patchDescription)
        {
            if (SelectedPatches.Items.Count > 0)
                return;

            RepoPatch patchToSelect = (AvailablePatches.ItemsSource as IEnumerable<Repo>)
                ?.FirstOrDefault((Repo it) => it.SourceRepo.Id == patchDescription.Repo.Id)
                ?.Patches?.FirstOrDefault((RepoPatch it) => it.SourcePatch.Id == patchDescription.Id);
            if (patchToSelect == null)
                return;

            ResetConfigName();
            SelectPatch(patchToSelect);
        }

        private void UnselectPatch(RepoPatch patch)
        {
            selectedPatches.Remove(patch);
            patch.Select(false);
            RemoveFromConfigName(patch.SourcePatch.Id);
            UpdateFirstAndLastFlags();
        }

        private void SelectedPatch_DoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (!(e.OriginalSource is FrameworkElement clickedItem))
                return;

            if (!(clickedItem.DataContext is RepoPatch patch))
                return;

            UnselectPatch(patch);
        }

        private void SelectedPatch_KeyUp(object sender, KeyEventArgs e)
        {
            if (e.Key != Key.Back && e.Key != Key.Delete)
                return;

            if (!(e.OriginalSource is FrameworkElement sourceItem))
                return;

            if (!(sourceItem.DataContext is RepoPatch patch))
                return;

            UnselectPatch(patch);
        }

        private void SelectedPatchesMoveLeft(object sender, RoutedEventArgs e)
        {
            RepoPatch selectedPatch;

            if (sender is Button button && button.Tag is RepoPatch patch)
            {
                selectedPatch = patch;
            }
            else
            {
                if (!(SelectedPatches.SelectedItem is RepoPatch patch2))
                    return;
                selectedPatch = patch2;
            }

            UnselectPatch(selectedPatch);
        }

        private void SelectedPatch_MoveUp(object sender, RoutedEventArgs e)
        {
            RepoPatch selectedPatch;
            if (sender is Button button && button.Tag is RepoPatch patch)
            {
                selectedPatch = patch;
            }
            else
            {
                if (!(SelectedPatches.SelectedItem is RepoPatch patch2))
                    return;
                selectedPatch = patch2;
            }

            int idx = selectedPatches.IndexOf(selectedPatch);
            if (idx <= 0)
                return;

            selectedPatches.Move(idx, idx - 1);
            UpdateFirstAndLastFlags();
        }

        private void SelectedPatch_MoveDown(object sender, RoutedEventArgs e)
        {
            RepoPatch selectedPatch;
            if (sender is Button button && button.Tag is RepoPatch patch)
            {
                selectedPatch = patch;
            }
            else
            {
                if (!(SelectedPatches.SelectedItem is RepoPatch patch2))
                    return;
                selectedPatch = patch2;
            }

            int idx = selectedPatches.IndexOf(selectedPatch);
            if (idx >= selectedPatches.Count - 1)
                return;

            selectedPatches.Move(idx, idx + 1);
            UpdateFirstAndLastFlags();
        }

        public void ConfigNameChanged(object sender, TextChangedEventArgs e)
        {
            if (isUnedited > 0)
            {
                isUnedited--;
            }

            if (configMaxLength == 0)
            {
                configMaxLength = 248 - (Environment.CurrentDirectory.Length + "\\config\\.js".Length);
            }

            if (ConfigName.Text.Length > configMaxLength)
            {
                ConfigName.Text = ConfigName.Text.Substring(0, configMaxLength);
            }
        }

        private void QuickFilterChanged(object sender, TextChangedEventArgs e)
        {
            var textbox = sender as TextBox;
            Debug.Assert(textbox != null, nameof(textbox) + " != null");
            var filter = textbox.Text.ToLower();


            if (filter.Length > 0)
            {
                Placeholder.Visibility = Visibility.Hidden;
                SearchButton.Content = SearchCrossEmoji;
            }
            else
            {
                Placeholder.Visibility = Visibility.Visible;
                SearchButton.Content = SearchEmoji;
            }

            if (AvailablePatches?.ItemsSource is IEnumerable<Repo> repos)
            {
                foreach (Repo repo in repos)
                    repo.SetVisibility(repo.UpdateFilter(filter));
            }
        }

        private void SearchButton_OnClick(object sender, RoutedEventArgs e)
        {
            QuickFilterTextBox.Text = "";
            QuickFilterTextBox.Focus();
        }
    }
}
