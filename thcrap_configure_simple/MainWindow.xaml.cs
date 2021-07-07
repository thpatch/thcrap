using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
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

            Environment.CurrentDirectory = AppContext.BaseDirectory + "\\..";
        }

        Task<List<Repo>> repoDiscovery;
        List<Repo> repoList;
        string configName;

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            ThcrapDll.log_init(0);

            var startUrl = "https://srv.thpatch.net/";
            var cmdline = Environment.GetCommandLineArgs();
            if (cmdline.Length > 1)
                startUrl = cmdline[1];

            repoDiscovery = new Task<List<Repo>>(() => Repo.Discovery(startUrl));
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

        private void PrepareStack(List<RepoPatch> patches)
        {
            ThcrapDll.stack_free();
            var knownPatches = new HashSet<RepoPatch>();
            foreach (var patch in patches)
                patch.AddToStack(repoList, knownPatches);

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

            (List<RepoPatch> patches, string configName) = Page2Content.GetSelectedRepoPatch();
            this.configName = configName;
            await Task.Run(() =>
            {
                PrepareStack(patches);
                Page3Content.DownloadCoreFiles();
            });

            // Page4 can take a bit of time to load, so we load it before displaying it
            await Page4Content.Enter(Page4);

            Page3.CanSelectNextPage = true;
            wizard.CurrentPage = Page4;
        }

        private void CreateShortcuts(List<ThcrapDll.games_js_entry> games)
        {
            ThcrapDll.games_js_entry[] gamesArray = new ThcrapDll.games_js_entry[games.Count + 1];
            int i = 0;
            foreach (var it in games)
            {
                gamesArray[i] = it;
                i++;
            }
            gamesArray[i] = new ThcrapDll.games_js_entry();

            ThcrapDll.CreateShortcuts(this.configName, gamesArray);
        }

        private async void Page5_Enter(object sender, RoutedEventArgs e)
        {
            Page5.CanSelectNextPage = false;

            var games = Page4Content.Leave();
            await Task.Run(() => Page5Content.DownloadGameFiles(games));
            CreateShortcuts(games);

            Page5.CanSelectNextPage = true;
            wizard.CurrentPage = LastPage;
        }

        private void RunThcrapConfigure(object sender, MouseButtonEventArgs e)
        {
            Process.Start("thcrap_configure.exe");
            this.Close();
        }
    }
}
