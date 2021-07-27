using System;
using System.Collections.Generic;
using System.Collections.ObjectModel;
using System.ComponentModel;
using System.Linq;
using System.IO;
using System.Text;
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

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page2_advanced.xaml
    /// </summary>
    public partial class Page2_advanced : UserControl
    {
        private int isUnedited = 1;
        public int configMaxLength = 0;

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
            private bool isSelected = false;
            public RepoPatch(thcrap_configure_v3.RepoPatch patch)
            {
                SourcePatch = patch;
            }

            public Visibility VisibilityInTree
            {
                get
                {
                    return isSelected ? Visibility.Collapsed : Visibility.Visible;
                }
            }
            public bool IsSelected() => isSelected;
            public void Select(bool newState)
            {
                isSelected = newState;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VisibilityInTree)));
            }

            public event PropertyChangedEventHandler PropertyChanged;
        }

        public class Repo
        {
            public thcrap_configure_v3.Repo SourceRepo { get; private set; }
            public List<RepoPatch> Patches { get; private set; }

            public Repo(thcrap_configure_v3.Repo repo)
            {
                SourceRepo = repo;
                Patches = repo.Patches.ConvertAll((thcrap_configure_v3.RepoPatch patch) => new RepoPatch(patch));
            }
        }

        ObservableCollection<RepoPatch> selectedPatches = new ObservableCollection<RepoPatch>();

        public Page2_advanced()
        {
            InitializeComponent();
        }

        public void SetRepoList(List<thcrap_configure_v3.Repo> repoList)
        {
            AvailablePatches.ItemsSource = repoList.ConvertAll((thcrap_configure_v3.Repo repo) => new Repo(repo));
            SelectedPatches.ItemsSource = selectedPatches;
        }

        public List<thcrap_configure_v3.RepoPatch> GetSelectedRepoPatch() => selectedPatches.ToList().ConvertAll((RepoPatch patch) => patch.SourcePatch);

        private void SelectPatch(RepoPatch patch)
        {
            AddToConfigName(patch.SourcePatch.Id);
            patch.Select(true);
            selectedPatches.Add(patch);
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
            if (!(AvailablePatches.SelectedItem is RepoPatch patch))
                return;

            if (!patch.IsSelected())
                SelectPatch(patch);
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
        }

        private void SelectedPatchesDoubleClick(object sender, MouseButtonEventArgs e)
        {
            if (!(e.OriginalSource is FrameworkElement clickedItem))
                return;

            if (!(clickedItem.DataContext is RepoPatch patch))
                return;

            UnselectPatch(patch);
        }

        private void SelectedPatchesMoveLeft(object sender, RoutedEventArgs e)
        {
            if (!(SelectedPatches.SelectedItem is RepoPatch patch))
                return;

            UnselectPatch(patch);
        }

        private void SelectedPatch_MoveUp(object sender, RoutedEventArgs e)
        {
            if (!(SelectedPatches.SelectedItem is RepoPatch patch))
                return;

            int idx = selectedPatches.IndexOf(patch);
            if (idx <= 0)
                return;

            selectedPatches.Move(idx, idx - 1);
        }

        private void SelectedPatch_MoveDown(object sender, RoutedEventArgs e)
        {
            if (!(SelectedPatches.SelectedItem is RepoPatch patch))
                return;

            int idx = selectedPatches.IndexOf(patch);
            if (idx >= selectedPatches.Count - 1)
                return;

            selectedPatches.Move(idx, idx + 1);
        }

        public void ConfigNameChange(object sender, TextChangedEventArgs e)
        {
            if (isUnedited > 0)
            {
                isUnedited--;
            }
            if (ConfigName.Text.Length > configMaxLength)
            {
                ConfigName.Text = ConfigName.Text.Substring(0, configMaxLength);
            }
        }
    }
}
