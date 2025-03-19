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
using System.Windows.Shapes;

/**
 * Comment from Zero:
 * I do like the idea, but it'll definitely take a lot of work to get it just right. In particular:
 * - There needs to be a clear distinction between creating/editing the files for a patch and uploading the patch to a server. Otherwise there's nothing to indicate that generating files.js and uploading are separate steps and we'll probably get a ton of questions like "why isn't my patch showing up for other people to download" from people who used the UI and expected it to immediately work.
 * - Our documentation of what all the different fields do is still between poor and non-existent, so we'll need to fix that if new modders are supposed to be able to use this menu unassisted.
 * - IIRC part of the reason existing patches get repurposed for testing is to avoid needing to create new configs and shortcuts. Some sort of simple config editor that can add/remove patches from existing configs will likely be necessary to get people to use this new system.
 * - Saved repos/patches will likely need to go into a new local_repos folder to prevent the updater/config tool overwriting the repo.js file referencing the new patches.
 */
namespace thcrap_configure_v3
{
    /// <summary>
    /// Interaction logic for PatchEditor.xaml
    /// </summary>
    public partial class Page2_PatchEditor : UserControl
    {
        public Page2_PatchEditor()
        {
            InitializeComponent();
        }
    }
}
