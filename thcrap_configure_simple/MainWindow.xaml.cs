using System;
using System.Collections.Generic;
using System.Linq;
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

        private void Wizard_PageChanged(object sender, RoutedEventArgs e)
        {
            if (wizard.CurrentPage == Page1)
            {
                new Thread(() =>
                {
                    Thread.Sleep(2000);
                    Dispatcher.Invoke(() => wizard.CurrentPage = Page2);
                }).Start(); 
            }
            else if (wizard.CurrentPage == Page3)
            {
                new Thread(() =>
                {
                    Thread.Sleep(2000);
                    Dispatcher.Invoke(() => wizard.CurrentPage = Page4);
                }).Start();
            }
            else if (wizard.CurrentPage == Page5)
            {
                new Thread(() =>
                {
                    Thread.Sleep(2000);
                    Dispatcher.Invoke(() =>
                    {
                        Page5Text.Text = "Downloading and installing files...";
                        Page5Filename.Text = @"C:\Fake\Path\th16.exe";
                    });
                    Thread.Sleep(2000);
                    Dispatcher.Invoke(() => wizard.CurrentPage = LastPage);
                }).Start();
            }
        }
    }
}
