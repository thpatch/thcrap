// TODO use Newtonsoft.Json for every JSON parsing because I think Newtonsoft.Json supports most of JSON5 and System.Text.Json is designed to be very strict and supports none of JSON5.
// TODO replace System.Text.Json with Newtonsoft.Json in the release script
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
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
        private class Patch
        {
            public string id { get; set; }
            public string title { get; set; }
            public List<string> dependencies { get; set; }
            public string IdOrNewText => id ?? "New...";

            private JObject json;

            private Patch(string path)
            {
                this.json = JObject.Parse(File.ReadAllText(path + "/patch.js"));
                this.id = (string)this.json["id"];
                this.title = (string)this.json["title"];
                if (this.id == null)
                    throw new Exception();

                this.dependencies = new List<string>();
                var dependencies = this.json["dependencies"];
                if (dependencies != null)
                {
                    foreach (var dep in dependencies.ToArray())
                    {
                        this.dependencies.Add((string)dep);
                    }
                }
            }
            // Empty patch, used to create a new patch
            public Patch()
            { }
            public static Patch Load(string path)
            {
                if (File.Exists(path + "/patch.js"))
                    try
                    {
                        return new Patch(path);
                    }
                    catch
                    {
                        return null;
                    }
                else
                    return null;
            }
        }

        private class Repo
        {
            public enum ServerType
            {
                THPATCH_MIRROR,
                GITHUB_REPO,
                OTHER
            }
            public string id { get; set; }
            public string title { get; set; }
            public string contact { get; set; }
            public ServerType serverType { get
                {
                    if (url.StartsWith("https://mirrors.thpatch.net/"))
                        return ServerType.THPATCH_MIRROR;
                    else if (url.StartsWith("https://raw.githubusercontent.com/"))
                        return ServerType.GITHUB_REPO;
                    else
                        return ServerType.OTHER;
                }
            }
            public string url
            {
                get => this.servers.Count > 0 ? this.servers[0] : "";
                set => this.servers = new List<string>() {
                        value
                    };
            }
            public List<string> servers;
            public List<Patch> patches { get; set; }

            public string IdOrNewText => id ?? "New...";

            private JObject json;
            private string path;

            public bool HasLocalGit() => path != null ? Directory.Exists(path + "/.git") : false;

            private Repo(string path)
            {
                this.path = path;
                this.json = JObject.Parse(File.ReadAllText(path + "/repo.js"));
                this.id = (string)this.json["id"];
                this.title = (string)this.json["title"];
                this.contact = (string)this.json["contact"];

                this.servers = new List<string>();
                var servers = this.json["servers"];
                if (servers.Type == JTokenType.Array)
                {
                    foreach (var server in servers.ToArray())
                    {
                        this.servers.Add((string)server);
                    }
                }
                else
                {
                    // Assume this is a string, throw if it isn't
                    this.servers.Add((string)servers);
                }

                this.patches = new List<Patch>();
                var patches = this.json["patches"];
                if (patches != null)
                {
                    foreach (var patchElem in (JObject)patches)
                    {
                        var patch = Patch.Load(path + "/" + (string)patchElem.Key);
                        if (patch != null)
                            this.patches.Add(patch);
                    }
                }
                this.patches.Sort((x, y) => x.id.CompareTo(y.id));
                this.patches.Add(new Patch());
            }
            // Empty repo, used to create a new repo
            private Repo()
            { }

            public static Repo Load(string path)
            {
                if (File.Exists(path + "/repo.js"))
                    return new Repo(path);
                else
                    return null;
            }

            public static List<Repo> LoadAll()
            {
                List<Repo> repos = new List<Repo>();
                foreach (var dir in Directory.EnumerateDirectories("repos"))
                {
                    var repo = Repo.Load(dir);
                    if (repo != null)
                        repos.Add(repo);
                }
                repos.Sort((x, y) => x.id.CompareTo(y.id));
                repos.Add(new Repo());
                return repos;
            }
        }

        private List<Repo> repos_ = null;
        private List<Repo> repos { get
            {
                if (repos_ == null)
                    repos_ = Repo.LoadAll();
                return repos_;
            }
        }

        public Page2_PatchEditor()
        {
            InitializeComponent();
        }

        public void Init()
        {
            var dpd = DependencyPropertyDescriptor.FromProperty(ItemsControl.ItemsSourceProperty, typeof(ComboBox));
            if (dpd != null)
                dpd.AddValueChanged(ComboBoxPatches, ComboBoxPatches_ItemsSourceChanged);

            ComboBoxRepos.ItemsSource = repos;
            foreach (var repo in repos)
            {
                if (repo.HasLocalGit() // If the user has a folder with a .git, it's likely a repo they're working on
                    || repo.id == null) // We reached our "New..." item at the end of the list, pick that one
                {
                    ComboBoxRepos.SelectedItem = repo;
                    break;
                }
            }
        }

        private void ComboBoxPatches_ItemsSourceChanged(object sender, EventArgs e)
        {
            var patches = ComboBoxPatches.ItemsSource as IEnumerable<Patch>;
            ComboBoxPatches.SelectedItem = patches.First();
        }
    }
}
