using System;
using System.Collections.Generic;
using System.ComponentModel;
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
using System.Windows.Shapes;

namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for Page2_PatchEditor_GitRemote.xaml
    /// </summary>
    public partial class Page2_PatchEditor_GitRemote : Window, INotifyPropertyChanged
    {
        public event PropertyChangedEventHandler PropertyChanged;

        private Page2_PatchEditor_Github githubConnection = null;
        private Page2_PatchEditor_Github.DeviceFlowParams _deviceFlowParams = null;
        private Page2_PatchEditor_Github.DeviceFlowParams deviceFlowParams {
            get => _deviceFlowParams;
            set {
                _deviceFlowParams = value;
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(UserCode)));
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(VerificationUri)));
            }
        }
        public string UserCode { get => deviceFlowParams != null ? deviceFlowParams.user_code : "genearting code, please wait..."; }
        public string VerificationUri { get => deviceFlowParams != null ? deviceFlowParams.verification_uri : "generating link, please wait..."; }

        public Page2_PatchEditor_GitRemote()
        {
            InitializeComponent();
            DataContext = this;
            new Task(async () =>
            {
                githubConnection = new Page2_PatchEditor_Github();
                deviceFlowParams = await githubConnection.StartDeviceFlow();
                while (!await githubConnection.GetToken(deviceFlowParams.device_code, deviceFlowParams.interval))
                    deviceFlowParams = await githubConnection.StartDeviceFlow();
                // TODO generate ssh key
                // TODO save key pair and configure git repo to use them
                // TODO upload public key to Github
            }).Start();
        }

        private void CopyCode(object sender, RoutedEventArgs e)
        {
            Clipboard.SetDataObject(UserCode);
        }

        private void OpenUri(object sender, System.Windows.Navigation.RequestNavigateEventArgs e)
        {
            System.Diagnostics.Process.Start(e.Uri.AbsoluteUri);
        }
    }
}
