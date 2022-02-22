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
    class GlobalConfig
    {
        public bool background_updates   { get; set; }
        public long time_between_updates { get; set; }
        public bool update_at_exit       { get; set; }
        public bool update_others        { get; set; }
        public bool console              { get; set; }
        public long exception_detail     { get; set; }
        public Page5.ShortcutDestinations default_shortcut_destinations { get; set; }

        public GlobalConfig()
        {
            background_updates            = ThcrapDll.globalconfig_get_boolean("background_updates", true);
            time_between_updates          = ThcrapDll.globalconfig_get_integer("time_between_updates", 5);
            update_at_exit                = ThcrapDll.globalconfig_get_boolean("update_at_exit", false);
            update_others                 = ThcrapDll.globalconfig_get_boolean("update_others", true);
            console                       = ThcrapDll.globalconfig_get_boolean("console", false);
            exception_detail              = ThcrapDll.globalconfig_get_integer("exception_detail", 1);
            default_shortcut_destinations = (Page5.ShortcutDestinations)ThcrapDll.globalconfig_get_integer("default_shortcut_destinations",
                (long)(Page5.ShortcutDestinations.Desktop | Page5.ShortcutDestinations.StartMenu));
        }
        public void Save()
        {
            if (!Directory.Exists("config"))
                Directory.CreateDirectory("config");

             ThcrapDll.globalconfig_set_boolean("background_updates", background_updates);
             ThcrapDll.globalconfig_set_integer("time_between_updates", time_between_updates);
             ThcrapDll.globalconfig_set_boolean("update_at_exit", update_at_exit);
             ThcrapDll.globalconfig_set_boolean("update_others", update_others);
             ThcrapDll.globalconfig_set_boolean("console", console);
             ThcrapDll.globalconfig_set_integer("exception_detail", exception_detail);
             ThcrapDll.globalconfig_set_integer("default_shortcut_destinations", (long)default_shortcut_destinations);
        }
    }
}
