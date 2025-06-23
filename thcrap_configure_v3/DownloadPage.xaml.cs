using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
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
using thcrap_cs_lib;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page3.xaml
    /// </summary>
    public partial class DownloadPage : UserControl
    {
        public DownloadPage()
        {
            InitializeComponent();
        }

        void ResetProgress()
        {
            this.Dispatcher.Invoke(() =>
            {
                this.Status.Text = "";
                this.Progress.IsIndeterminate = true;
            });
        }

        bool DownloadCallbak(IntPtr status_, IntPtr param)
        {
            var status = Marshal.PtrToStructure<ThcrapDll.progress_callback_status_t>(status_);
            if (status.status == ThcrapDll.get_status_t.GET_OK)
            {
                var cur_patch = Marshal.PtrToStructure<ThcrapDll.patch_t>(status.patch);
                string cur_patch_id = ThcrapHelper.PtrToStringUTF8(cur_patch.id);
                this.Dispatcher.Invoke(() =>
                {
                    if (status.nb_files_total != 0)
                    {
                        this.Status.Text = String.Format("[{0}/{1}] {2}/{3}",
                            status.nb_files_downloaded, status.nb_files_total, cur_patch_id, ThcrapHelper.PtrToStringUTF8(status.fn));
                        if (this.Progress.IsIndeterminate)
                        {
                            this.Progress.IsIndeterminate = false;
                            this.Progress.Minimum = 0;
                            this.Progress.Maximum = status.nb_files_total;
                        }
                        if (status.nb_files_total != this.Progress.Maximum)
                            this.Progress.Maximum = status.nb_files_total;
                        this.Progress.Value = status.nb_files_downloaded;
                    }
                    else
                    {
                        this.Status.Text = String.Format("{0}/{1}", cur_patch_id, ThcrapHelper.PtrToStringUTF8(status.fn));
                    }
                });
            }
            return true;
        }

        // Called from another thread. Can block, must use a dispatcher to change the UI.
        public void DownloadCoreFiles()
        {
            ResetProgress();
            ThcrapDll.stack_update_wrapper((string fn, IntPtr unused) => ThcrapDll.update_filter_global_wrapper(fn, unused),
                IntPtr.Zero, DownloadCallbak, IntPtr.Zero);
        }

        IntPtr StringToIntPtr(string str)
        {
            byte[] bytes = Encoding.UTF8.GetBytes(str + '\0');
            IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, ptr, bytes.Length);
            return ptr;
        }
        IntPtr GamesArrayToIntPtr(List<IntPtr> games)
        {
            IntPtr ptr = Marshal.AllocHGlobal(games.Count * IntPtr.Size);
            Marshal.Copy(games.ToArray(), 0, ptr, games.Count);
            return ptr;
        }
        void FreeGames(IntPtr unmanagedList, List<IntPtr> listOfStrings)
        {
            Marshal.FreeHGlobal(unmanagedList);
            foreach (var it in listOfStrings)
            {
                if (it != IntPtr.Zero)
                    Marshal.FreeHGlobal(it);
            }
        }
        // Called from another thread. Can block, must use a dispatcher to change the UI.
        public void DownloadGameFiles(IEnumerable<ThcrapDll.games_js_entry> games)
        {
            ResetProgress();
            var gamesArray = games.Select((ThcrapDll.games_js_entry game) => StringToIntPtr(game.id)).ToList();
            gamesArray.Add(IntPtr.Zero);
            var gamesPtr = GamesArrayToIntPtr(gamesArray);

            ThcrapDll.stack_update_wrapper((string fn, IntPtr games_) => ThcrapDll.update_filter_games_wrapper(fn, games_), gamesPtr, DownloadCallbak, IntPtr.Zero);

            FreeGames(gamesPtr, gamesArray);
        }
    }
}
