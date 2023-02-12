using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
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

namespace thcrap_configure_v3
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
        bool skipSearchGames = false;

        private void Window_Loaded(object sender, RoutedEventArgs e)
        {
            ThcrapDll.globalconfig_init();
            ThcrapDll.exception_load_config();
            ThcrapDll.log_init(ThcrapDll.globalconfig_get_boolean("console", false));

            var startUrl = "https://srv.thpatch.net/";
            var cmdline = Environment.GetCommandLineArgs();

            for(int i = 1; i < cmdline.Length; i++)
            {
                if (cmdline[i] == "--skip-search-games")
                {
                    this.skipSearchGames = true;
                }
                else
                {
                    startUrl = cmdline[i];
                }
            }

            repoDiscovery = new Task<List<Repo>>(() => {
                SelfUpdate();
                return Repo.Discovery(startUrl);
            });
            repoDiscovery.Start();
        }

        private void SelfUpdate()
        {
            if (ThcrapUpdateDll.update_notify_thcrap() == ThcrapUpdateDll.self_result_t.SELF_OK)
            {
                Process.Start(Assembly.GetEntryAssembly().Location);
                Application.Current.Dispatcher.Invoke(() => Application.Current.Shutdown());
            }
        }

        // Repo discovery
        private async void Page1_Enter(object sender, RoutedEventArgs e)
        {
            Page1.CanSelectNextPage = false;

            if (repoDiscovery != null)
            {
                repoList = await repoDiscovery;
                Page2Content.SetRepoList(repoList);
            }
            repoDiscovery = null;

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
                var id = ThcrapHelper.PtrToStringUTF8(cur_patch.id);
                var cur_RepoPatch = knownPatches.First(x => x.Id == id);
                runconfig.patches.Add(new RunconfigPatch(cur_RepoPatch.Archive));
            }, IntPtr.Zero);
            runconfig.Save(this.configName);
        }
        // Resolve dependencies and download core files
        private async void Page3_Enter(object sender, RoutedEventArgs e)
        {
            Page3.CanSelectNextPage = false;

            List<RepoPatch> patches = Page2Content.GetSelectedRepoPatch();
            this.configName = Page2Content.GetConfigName();

            await Task.Run(() =>
            {
                PrepareStack(patches);
                Page3Content.DownloadCoreFiles();
            });

            if(this.skipSearchGames)
            {
                wizard.CurrentPage = LastPage;
                LastPage.CanSelectNextPage = true;
            }
            else
            {
                // Page4 can take a bit of time to load, so we load it before displaying it
                await Page4Content.Enter(Page4);

                Page3.CanSelectNextPage = true;
                wizard.CurrentPage = Page4;
            }
        }

        private void Page5_Enter(object sender, RoutedEventArgs e)
        {
            Page4Content.Leave();
            Page5Content.Enter();
        }

        private void Page5_Leave(object sender, RoutedEventArgs e)
        {
            Page5Content.Leave(this.configName, Page4Content.GetGames());
        }

        private async void Page6_Enter(object sender, RoutedEventArgs e)
        {
            Page6.CanSelectNextPage = false;

            var games = Page4Content.GetGames();
            await Task.Run(() => Page6Content.DownloadGameFiles(games));

            Page6.CanSelectNextPage = true;
            wizard.CurrentPage = LastPage;
        }

        private void RunThcrapConfigure(object sender, MouseButtonEventArgs e)
        {
            Process.Start("bin\\thcrap_configure" + ThcrapDll.DEBUG_OR_RELEASE + ".exe");
            this.Close();
        }
    }
}
