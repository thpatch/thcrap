using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace thcrap_configure_v3
{
    class RunconfigPatch
    {
        public string archive { get; set; }
        public RunconfigPatch(string archive)
        {
            this.archive = archive;
        }
    }

    class Runconfig
    {
        public bool dat_dump { get; set; } = false;
        public List<RunconfigPatch> patches { get; set; } = new List<RunconfigPatch>();

        public void Save(string config_name)
        {
            if (!Directory.Exists("config"))
                Directory.CreateDirectory("config");

            string jsonRunconfig = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText("config/" + config_name + ".js", jsonRunconfig);
        }
    }
}
