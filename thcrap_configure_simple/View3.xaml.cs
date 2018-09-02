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
    /// Logique d'interaction pour View3.xaml
    /// </summary>
    public partial class View3 : UserControl
    {
        public View3()
        {
            InitializeComponent();
        }

        private void next(object sender, RoutedEventArgs e)
        {
            //(this.Parent as MainWindow).Content = new View4();
        }
    }
}
