using System;
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
            background_updates.IsChecked = ThcrapDll.globalconfig_get_boolean("background_updates", false);
            update_others.IsChecked = ThcrapDll.globalconfig_get_boolean("update_others", true);
            update_at_exit.IsChecked = ThcrapDll.globalconfig_get_boolean("update_at_exit", false);
            time_between_updates.Text = ThcrapDll.globalconfig_get_integer("time_between_updates", 5).ToString();

            console.IsChecked = ThcrapDll.globalconfig_get_boolean("console", false);
            use_wininet.IsChecked = ThcrapDll.globalconfig_get_boolean("use_wininet", false);
            codepage.Text = ThcrapDll.globalconfig_get_integer("codepage", 932).ToString();

            {
                long exceptionDetail = ThcrapDll.globalconfig_get_integer("exception_detail", 932);

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
            ThcrapDll.globalconfig_set_boolean("background_updates", background_updates.IsChecked == true);
            ThcrapDll.globalconfig_set_boolean("update_others", update_others.IsChecked == true);
            ThcrapDll.globalconfig_set_boolean("update_at_exit", update_at_exit.IsChecked == true);
            ThcrapDll.globalconfig_set_integer("time_between_updates", Int32.Parse(time_between_updates.Text));

            ThcrapDll.globalconfig_set_boolean("console", console.IsChecked == true);
            ThcrapDll.globalconfig_set_boolean("use_wininet", use_wininet.IsChecked == true);
            ThcrapDll.globalconfig_set_integer("codepage", Int32.Parse(codepage.Text));

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
                ThcrapDll.globalconfig_set_integer("exception_detail", exceptionDetail);
            }

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
