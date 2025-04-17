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
using System.IO;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page2.xaml
    /// </summary>
    public partial class Page2 : UserControl
    {
        enum Mode
        {
            Simple,
            Advanced
        }

        Mode mode = Mode.Simple;

        public Page2()
        {
            InitializeComponent();
            this.Loaded += Page2_Loaded;
        }

        private void Page2_Loaded(object sender, RoutedEventArgs e)
        {
            toggleWarning();
        }

        public void toggleWarning()
        {
            bool updatesEnabled = File.Exists($"bin\\thcrap_update{ThcrapDll.DEBUG_OR_RELEASE}.dll");
            warnMessage.Visibility = updatesEnabled ? Visibility.Collapsed : Visibility.Visible;
        }

        public void SetRepoList(List<Repo> repoList)
        {
            Simple.SetRepoList(repoList);
            Advanced.SetRepoList(repoList);
        }

        private List<RepoPatch> patches;

        public List<RepoPatch> GetSelectedRepoPatch()
        {
            if (mode == Mode.Simple)
                patches = Simple.GetSelectedRepoPatch();
            else
                patches = Advanced.GetSelectedRepoPatch();

            return patches;
        }

        public string GetConfigName()
        {
            if(mode == Mode.Advanced)
                return Advanced.ConfigName.Text;
            else
                return patches[0].Id.Substring("lang_".Length);
        }

        private void TabChanged(object sender, SelectionChangedEventArgs e)
        {
            if ((sender as TabControl).SelectedContent == Advanced)
            {
                Advanced.SetInitialPatch(Simple.GetSelectedRepoPatch()[0]);

                mode = Mode.Advanced;
            }
            else
            {
                mode = Mode.Simple;
            }
        }
    }
}
