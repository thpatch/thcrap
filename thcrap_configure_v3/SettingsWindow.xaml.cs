using System;
using System.IO;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;

namespace thcrap_configure_v3
{
    public partial class SettingsWindow : Window
    {
        public MainWindow mainWindow;
        public bool isClosedWithX = true;
        public SettingsWindow()
        {
            InitializeComponent();
        }

        void WindowOpened(object sender, RoutedEventArgs e)
        {
            GlobalConfig config = GlobalConfig.get();
            auto_updates.IsChecked = File.Exists("bin\\thcrap_update.dll");
            background_updates.IsChecked = config.background_updates;
            update_others.IsChecked = config.update_others;
            update_at_exit.IsChecked = config.update_at_exit;
            time_between_updates.Text = config.time_between_updates.ToString();

            developer_mode.IsChecked = config.developer_mode;
            console.IsChecked = config.console;
            use_wininet.IsChecked = config.use_wininet;
            codepage.Text = config.codepage.ToString();

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

        void AutoUpdates_Checked(object sender, RoutedEventArgs e)
        {
            UpdateDLL("bin\\thcrap_update_disabled.dll", "bin\\thcrap_update.dll");
        }

        void AutoUpdates_Unchecked(object sender, RoutedEventArgs e)
        {
            UpdateDLL("bin\\thcrap_update.dll", "bin\\thcrap_update_disabled.dll");
        }

        private void UpdateDLL(string source, string destination)
        {
            try
            {
                if (File.Exists(source))
                {
                    File.Move(source, destination);
                }
            }
            catch (Exception ex)
            {
                MessageBox.Show("Failed to toggle automatic updates:\n" + ex.Message, "Error", MessageBoxButton.OK, MessageBoxImage.Error);
            }
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
