// TODO use Newtonsoft.Json for every JSON parsing because I think Newtonsoft.Json supports most of JSON5 and System.Text.Json is designed to be very strict and supports none of JSON5.
// TODO replace System.Text.Json with Newtonsoft.Json in the release script
using Newtonsoft.Json.Linq;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
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

            public class Servers : INotifyPropertyChanged
            {
                public event PropertyChangedEventHandler PropertyChanged;

                public enum Type
                {
                    ARRAY, // More than one URL; we don't allow editing
                    THPATCH_MIRROR,
                    GITHUB_REPO,
                    OTHER
                }

                private Repo repo;
                private List<string> servers;
                public Servers(Repo repo, List<string> servers)
                {
                    this.repo = repo;
                    this.servers = servers;
                    Refresh();
                }

                public string Url
                {
                    get
                    {
                        if (this.servers.Count > 1)
                            return "<several URLs>";
                        else if (this.servers.Count > 0)
                            return this.servers[0];
                        else
                            return "";
                    }
                    set
                    {
                        this.servers = new List<string>() {
                            value
                        };
                        Refresh();
                    }
                }
                private Type _type;
                public Type type { get => _type; }

                private Regex githubRegex = new Regex("https://raw\\.githubusercontent\\.com/(.*)/(.*)/master/");
                private Match githubMatch;

                public string GithubUsername { get; set; }
                public string GithubRepo { get; set; }

                public void RefreshUrlForThpatchMirror()
                {
                    Url = $"https://mirrors.thpatch.net/{repo.id}/";
                }
                public void RefreshUrlForGithubRepo()
                {
                    Url = $"https://raw.githubusercontent.com/{GithubUsername}/{GithubRepo}/master/";
                }

                public bool IsThpatchMirror
                {
                    get => type == Type.THPATCH_MIRROR;
                    set => RefreshUrlForThpatchMirror();
                }
                public bool IsGithubRepo
                {
                    get => type == Type.GITHUB_REPO;
                    set => RefreshUrlForGithubRepo();
                }
                public bool IsGithubInputEnabled { get => type == Type.GITHUB_REPO; }

                public bool IsOther
                {
                    get => type == Type.OTHER || type == Type.ARRAY;
                    set
                    {
                        _type = Type.OTHER;
                        Refresh(detectTypeFromUrl: false);
                    }
                }
                public bool IsManualInputEnabled { get => type == Type.OTHER; }

                private void Refresh(bool detectTypeFromUrl = true)
                {
                    // TODO: when switching from one repo to another, there seems to be some confusion
                    // where the new repo sometimes gets the Type of another repo.
                    // Putting a breakpoint here is a good idea to investigate this.
                    if (detectTypeFromUrl)
                    {
                        githubMatch = githubRegex.Match(Url);

                        if (Url.StartsWith("https://mirrors.thpatch.net/"))
                            _type = Type.THPATCH_MIRROR;
                        else if (githubMatch.Success)
                        {
                            _type = Type.GITHUB_REPO;
                            GithubUsername = githubMatch.Groups[1].Value;
                            GithubRepo = githubMatch.Groups[2].Value;
                        }
                        else
                            _type = Type.OTHER;
                    }

                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(Url)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsThpatchMirror)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsGithubRepo)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsGithubInputEnabled)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsOther)));
                    PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(nameof(IsManualInputEnabled)));
                }
            }
            public Servers servers { get; set; }

            public List<Patch> patches { get; set; }

            public Git git { get; set; }

            public string IdOrNewText => id ?? "New...";

            private JObject json;
            private string path;

            private Repo(string path)
            {
                this.path = path;
                this.json = JObject.Parse(File.ReadAllText(path + "/repo.js"));
                this.id = (string)this.json["id"];
                this.title = (string)this.json["title"];
                this.contact = (string)this.json["contact"];

                var serversList = new List<string>();
                var servers = this.json["servers"];
                if (servers.Type == JTokenType.Array)
                {
                    foreach (var server in servers.ToArray())
                    {
                        serversList.Add((string)server);
                    }
                }
                else
                {
                    // Assume this is a string, throw if it isn't
                    serversList.Add((string)servers);
                }
                this.servers = new Servers(this, serversList);

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

                this.git = Git.Open(path);
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
                if (repo.git.IsValid // If the user has a folder with a .git, it's likely a repo they're working on
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

        private void RefreshRepoStatus(object sender, RoutedEventArgs e)
        {
            var repo = ComboBoxRepos.SelectedItem as Repo;
            repo.git.RefreshStatus();
        }

        private void RepoIdChanged(object sender, TextChangedEventArgs e)
        {
            var repo = ComboBoxRepos.SelectedItem as Repo;
            repo.servers.RefreshUrlForThpatchMirror();
        }
        private void GithubFieldChanged(object sender, TextChangedEventArgs e)
        {
            var repo = ComboBoxRepos.SelectedItem as Repo;
            repo.servers.RefreshUrlForGithubRepo();
        }
    }
}
