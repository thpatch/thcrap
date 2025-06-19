using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using thcrap_cs_lib;

namespace thcrap_configure_v3
{
    public partial class SettingsWindow : Window
    {
        public MainWindow mainWindow;
        public bool isClosedWithX = true;
        private string updateDLL = $"bin\\thcrap_update{ThcrapDll.DEBUG_OR_RELEASE}.dll";
        private string updateDLLDisabled = $"bin\\thcrap_update{ThcrapDll.DEBUG_OR_RELEASE}_disabled.dll";
        private bool autoUpdateChoice;
        public SettingsWindow()
        {
            InitializeComponent();
        }

        void WindowOpened(object sender, RoutedEventArgs e)
        {
            GlobalConfig config = GlobalConfig.get();
            bool updatesEnabled = File.Exists(updateDLL);
            autoUpdateChoice = updatesEnabled;
            auto_updates.IsChecked = updatesEnabled;
            background_updates.IsChecked = config.background_updates;
            update_others.IsChecked = config.update_others;
            update_at_exit.IsChecked = config.update_at_exit;
            time_between_updates.Text = config.time_between_updates.ToString();

            developer_mode.IsChecked = config.developer_mode;
            console.IsChecked = config.console;
            use_wininet.IsChecked = config.use_wininet;
            codepage.Text = config.codepage.ToString();

            if (File.Exists(updateDLL) && File.Exists(updateDLLDisabled)) // Clean up if both DLLs exist at the same time, prefer enabled
            {
                try
                {
                    File.Delete(updateDLLDisabled);
                }
                catch { }
            }

            {
                long exceptionDetail = config.exception_detail;

                if ((exceptionDetail & 1) > 0)
                {
                    ExceptionDetailBasic.IsChecked = true;
                } else
                {
                    ExceptionDetailBasic.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 1)) > 0)
                {
                    ExceptionDetailFPU.IsChecked = true;
                } else
                {
                    ExceptionDetailFPU.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 2)) > 0)
                {
                    ExceptionDetailSSE.IsChecked = true;
                } else
                {
                    ExceptionDetailSSE.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 3)) > 0)
                {
                    ExceptionDetailSegments.IsChecked = true;
                } else
                {
                    ExceptionDetailSegments.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 4)) > 0)
                {
                    ExceptionDetailDebug.IsChecked = true;
                } else
                {
                    ExceptionDetailDebug.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 5)) > 0)
                {
                    ExceptionDetailManualTrace.IsChecked = true;
                } else
                {
                    ExceptionDetailManualTrace.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 6)) > 0)
                {
                    ExceptionDetailTraceDump.IsChecked = true;
                } else
                {
                    ExceptionDetailTraceDump.IsChecked = false;
                }
                if ((exceptionDetail & (1 << 7)) > 0)
                {
                    ExceptionDetailExtra.IsChecked = true;
                } else
                {
                    ExceptionDetailExtra.IsChecked = false;
                }
            }
        }
        private static readonly Regex NumbersOnlyRegex = new Regex("[^0-9]+");
        void NumericTextbox(object sender, TextCompositionEventArgs e)
        {
            e.Handled = NumbersOnlyRegex.IsMatch(e.Text);
        }

        private void RenameDLL(string source, string destination)
        {
            try
            {
                if(File.Exists(destination))
                {
                    File.Delete(destination);
                }

                if (File.Exists(source))
                {
                    File.Move(source, destination);
                }
            }
            catch{ }
        }

        void BackgroundUpdates_Checked(object sender, RoutedEventArgs e)
        {
            update_others.IsEnabled = true;
            time_between_updates_grid.IsEnabled = true;
        }

        void BackgroundUpdates_Unchecked(object sender, RoutedEventArgs e)
        {
            update_others.IsEnabled = false;
            time_between_updates_grid.IsEnabled = false;
        }

        private void Settings_Save(object sender, RoutedEventArgs e)
        {
            GlobalConfig config = GlobalConfig.get();
            config.background_updates = background_updates.IsChecked == true;
            config.update_others = update_others.IsChecked == true;
            config.update_at_exit = update_at_exit.IsChecked == true;
            config.time_between_updates = Int32.Parse(time_between_updates.Text);

            config.developer_mode = developer_mode.IsChecked == true;
            config.console = console.IsChecked == true;
            config.use_wininet = use_wininet.IsChecked == true;
            config.codepage = Int32.Parse(codepage.Text);

            bool newAutoUpdatesEnabled = auto_updates.IsChecked == true;
            if (newAutoUpdatesEnabled != autoUpdateChoice)
            {
                if (newAutoUpdatesEnabled)
                {
                    RenameDLL(updateDLLDisabled, updateDLL);
                }
                else
                {
                    RenameDLL(updateDLL, updateDLLDisabled);
                }
            }

            {
                int exceptionDetail = 0;

                if (ExceptionDetailBasic.IsChecked == true)
                {
                    exceptionDetail = 1;
                }
                if (ExceptionDetailFPU.IsChecked == true)
                {
                    exceptionDetail |= 1 << 1;
                }
                if (ExceptionDetailSSE.IsChecked == true)
                {
                    exceptionDetail |= 1 << 2;
                }
                if (ExceptionDetailSegments.IsChecked == true)
                {
                    exceptionDetail |= 1 << 3;
                }
                if (ExceptionDetailDebug.IsChecked == true)
                {
                    exceptionDetail |= 1 << 4;
                }
                if (ExceptionDetailManualTrace.IsChecked == true)
                {
                    exceptionDetail |= 1 << 5;
                }
                if (ExceptionDetailTraceDump.IsChecked == true)
                {
                    exceptionDetail |= 1 << 6;
                }
                if (ExceptionDetailExtra.IsChecked == true)
                {
                    exceptionDetail |= 1 << 7;
                }
                config.exception_detail = exceptionDetail;
            }
            config.Save();

            isClosedWithX = false;
            this.Close();
        }

        private void Settings_Cancel(object sender, RoutedEventArgs e)
        {
            isClosedWithX = false;
            this.Close();
        }

        private void ExceptionSetDefault(object sender, RoutedEventArgs e)
        {
            ExceptionDetailBasic.IsChecked = true;
            ExceptionDetailFPU.IsChecked = false;
            ExceptionDetailSSE.IsChecked = false;
            ExceptionDetailSegments.IsChecked = false;
            ExceptionDetailDebug.IsChecked = false;
            ExceptionDetailManualTrace.IsChecked = false;
            ExceptionDetailTraceDump.IsChecked = false;
            ExceptionDetailExtra.IsChecked = false;
        }

        private void ExceptionSetEnchanced(object sender, RoutedEventArgs e)
        {
            ExceptionDetailBasic.IsChecked = true;
            ExceptionDetailFPU.IsChecked = true;
            ExceptionDetailSSE.IsChecked = true;
            ExceptionDetailSegments.IsChecked = false;
            ExceptionDetailDebug.IsChecked = false;
            ExceptionDetailManualTrace.IsChecked = true;
            ExceptionDetailTraceDump.IsChecked = true;
            ExceptionDetailExtra.IsChecked = false;
        }

        private void ExceptionSetFull(object sender, RoutedEventArgs e)
        {
            ExceptionDetailBasic.IsChecked = true;
            ExceptionDetailFPU.IsChecked = true;
            ExceptionDetailSSE.IsChecked = true;
            ExceptionDetailSegments.IsChecked = true;
            ExceptionDetailDebug.IsChecked = true;
            ExceptionDetailManualTrace.IsChecked = true;
            ExceptionDetailTraceDump.IsChecked = true;
            ExceptionDetailExtra.IsChecked = false;
        }
    }
}
