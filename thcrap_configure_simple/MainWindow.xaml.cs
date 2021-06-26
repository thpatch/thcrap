using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
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
    /// Logique d'interaction pour MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        public MainWindow()
        {
            InitializeComponent();
        }

        Task<List<Repo>> repoDiscovery;
        List<Repo> repoList;
        string configName;

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            ThcrapDll.log_init(0);

            repoDiscovery = new Task<List<Repo>>(() => Repo.Discovery("https://srv.thpatch.net/"));
            repoDiscovery.Start();
        }

        // Repo discovery
        private async void Page1_Enter(object sender, RoutedEventArgs e)
        {
            Page1.CanSelectNextPage = false;

            repoList = await repoDiscovery;
            Page2Content.SetRepoList(repoList);

            Page1.CanSelectNextPage = true;
            wizard.CurrentPage = Page2;
        }

        private void PrepareStack(RepoPatch patch)
        {
            ThcrapDll.stack_free();
            patch.AddToStack(repoList, new HashSet<RepoPatch>());

            this.configName = patch.Id.Substring("lang_".Length);

            var runconfig = new Runconfig();
            ThcrapDll.stack_foreach((IntPtr it, IntPtr userdata) =>
            {
                var cur_patch = Marshal.PtrToStructure<ThcrapDll.patch_t>(it);
                var archive = Marshal.PtrToStringAnsi(cur_patch.archive);
                runconfig.patches.Add(new RunconfigPatch(archive));
            }, IntPtr.Zero);
            runconfig.Save(this.configName);
        }
        // Resolve dependencies and download core files
        private async void Page3_Enter(object sender, RoutedEventArgs e)
        {
            Page3.CanSelectNextPage = false;

            RepoPatch patch = Page2Content.GetSelectedRepoPatch();
            await Task.Run(() =>
            {
                PrepareStack(patch);
                Page3Content.DownloadCoreFiles();
            });

            Page3.CanSelectNextPage = true;
            wizard.CurrentPage = Page4;
        }

        private void Page4_Enter(object sender, RoutedEventArgs e) => Page4Content.Enter(Page4);

        private async void Page5_Enter(object sender, RoutedEventArgs e)
        {
            Page5.CanSelectNextPage = false;

            var games = Page4Content.Leave();
            await Task.Run(() => Page5Content.DownloadGameFiles(games));

            Page5.CanSelectNextPage = true;
            wizard.CurrentPage = LastPage;
        }
    }
}
