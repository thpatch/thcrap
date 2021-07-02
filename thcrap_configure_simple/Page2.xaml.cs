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

namespace thcrap_configure_simple
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
        }

        public void SetRepoList(List<Repo> repoList)
        {
            Simple.SetRepoList(repoList);
            Advanced.SetRepoList(repoList);
        }

        string GetStackName(List<RepoPatch> patches)
        {
            string ret = "";

            foreach (var it in patches)
            {
                if (ret != "")
                    ret += "-";

                string id = it.Id;
                if (id.StartsWith("lang_"))
                    id = id.Substring("lang_".Length);

                ret += id;
            }

            return ret;
        }

        public (List<RepoPatch>, string) GetSelectedRepoPatch()
        {
            List<RepoPatch> patches;
            if (mode == Mode.Simple)
                patches = Simple.GetSelectedRepoPatch();
            else
                patches = Advanced.GetSelectedRepoPatch();

            return (patches, GetStackName(patches));
        }

        private void GoToAdvanced()
        {
            Simple.Visibility = Visibility.Collapsed;
            SimpleText.Visibility = Visibility.Collapsed;
            Advanced.Visibility = Visibility.Visible;
            AdvancedText.Visibility = Visibility.Visible;
            mode = Mode.Advanced;
        }

        private void GoToSimple()
        {
            Advanced.Visibility = Visibility.Collapsed;
            AdvancedText.Visibility = Visibility.Collapsed;
            Simple.Visibility = Visibility.Visible;
            SimpleText.Visibility = Visibility.Visible;
            mode = Mode.Simple;
        }

        private void ChangeMode(object sender, MouseButtonEventArgs e)
        {
            if (mode == Mode.Simple)
                GoToAdvanced();
            else
                GoToSimple();
        }
    }
}
