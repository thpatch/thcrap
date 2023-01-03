using System;
using System.Collections.Generic;
using System.Linq;
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
    /// Interaction logic for Page2.xaml
    /// </summary>
    public partial class Page2_simple : UserControl
    {
        class RadioPatch
        {
            public RadioPatch(RepoPatch patch)
            {
                this.patch = patch;
                this.IsChecked = false;
            }
            public RepoPatch patch { get; set; }
            public bool IsChecked { get; set; }
        };
        private List<RadioPatch> patches;

        public Page2_simple()
        {
            InitializeComponent();
        }
        public void SetRepoList(List<Repo> repoList)
        {
            string isoCountryCode = GetIsoCountryCode();
            patches = new List<RadioPatch>();
            RadioPatch lang_en = null;
            var allLanguages = new List<RepoPatch>();

            foreach (var repo in repoList)
            {
                if (repo.Id == "thpatch")
                {
                    foreach (var patch in repo.Patches)
                    {
                        if (patch.Id == "lang_" + isoCountryCode ||
                            patch.Id.StartsWith(string.Format("lang_{0}-", isoCountryCode)))
                            patches.Add(new RadioPatch(patch));
                        if (patch.Id.StartsWith("lang_"))
                            allLanguages.Add(patch);
                        if (patch.Id == "lang_en")
                            lang_en = new RadioPatch(patch);
                    }
                }
            }
            if (isoCountryCode != "en" && lang_en != null)
                patches.Add(lang_en);
            if (patches.Count > 0)
                patches[0].IsChecked = true;

            UserLanguagePatches.ItemsSource = patches;
            allLanguages.Sort((RepoPatch a, RepoPatch b) => a.Title.CompareTo(b.Title));
            AllLanguages.ItemsSource = allLanguages;
            if (allLanguages.Count > 0)
                AllLanguages.SelectedIndex = 0;
            // Wine checks that radio when we change it, we need to uncheck it again
            AllLanguagesRadio.IsChecked = false;
        }
        private string GetIsoCountryCode()
        {
            return System.Globalization.CultureInfo.CurrentCulture.TwoLetterISOLanguageName;
        }

        private void AllLanguages_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            AllLanguagesRadio.IsChecked = true;
        }

        public List<RepoPatch> GetSelectedRepoPatch()
        {
            RepoPatch patch;
            var radioPatch = patches.Find((RadioPatch it) => it.IsChecked);
            if (radioPatch != null)
                patch = radioPatch.patch;
            else
                patch = AllLanguages.SelectedItem as RepoPatch;

            return new List<RepoPatch>() { patch };
        }
    }
}
