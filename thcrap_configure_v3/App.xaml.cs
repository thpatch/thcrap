using System;
using System.Collections.Generic;
using System.Configuration;
using System.Data;
using System.Linq;
using System.Threading.Tasks;
using System.Windows;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Logique d'interaction pour App.xaml
    /// </summary>
    public partial class App : Application
    {
        public App()
        {
            AppDomain.CurrentDomain.UnhandledException += new UnhandledExceptionEventHandler(ExceptionHandler);
        }

        static void ExceptionHandler(object sender, UnhandledExceptionEventArgs args)
        {
            if(args.IsTerminating)
            {
                ThcrapDll.log_print(".NET Exception details:\n");
                ThcrapDll.log_print((args.ExceptionObject as Exception).ToString());
            }
        }
    }
}
