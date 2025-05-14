using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace thcrap_cs_lib
{
    public class RunconfigPatch
    {
        public string archive { get; set; }
        public RunconfigPatch(string archive)
        {
            this.archive = archive;
        }
    }

    public class Runconfig
    {
        public bool console { get; set; } = false;
        public bool dat_dump { get; set; } = false;
        public bool patched_files_dump { get; set; } = false;
        public List<RunconfigPatch> patches { get; set; } = new List<RunconfigPatch>();

        public void Save(string config_name)
        {
            if (!Directory.Exists("config"))
                Directory.CreateDirectory("config");

            string jsonRunconfig = JsonSerializer.Serialize(this, new JsonSerializerOptions { WriteIndented = true });
            File.WriteAllText("config/" + config_name + ".js", jsonRunconfig);
        }
    }
    public class GlobalConfig
    {
        private const long configDestinationMask = 0xFFFF;
        private const int configTypeOffset = 16;

        [Flags]
        public enum ShortcutDestinations
        {
            Desktop = 1,
            StartMenu = 2,
            GamesFolder = 4,
            ThcrapFolder = 8,
        }

        private static GlobalConfig instance = null;
        public static GlobalConfig get()
        {
            if (instance == null)
            {
                instance = new GlobalConfig();
            }
            return instance;
        }

        public bool auto_updates                                  { get; set; }
        public bool background_updates                            { get; set; }
        public long time_between_updates                          { get; set; }
        public bool update_at_exit                                { get; set; }
        public bool update_others                                 { get; set; }
        public bool console                                       { get; set; }
        public bool use_wininet                                   { get; set; }
        public long exception_detail                              { get; set; }
        public long codepage                                      { get; set; }
        public bool developer_mode                                { get; set; }
        public ShortcutDestinations default_shortcut_destinations { get; set; }
        public ThcrapDll.ShortcutsType shortcuts_type             { get; set; }

        private GlobalConfig()
        {
            background_updates =   ThcrapDll.globalconfig_get_boolean("background_updates", false);
            time_between_updates = ThcrapDll.globalconfig_get_integer("time_between_updates", 5);
            update_at_exit =       ThcrapDll.globalconfig_get_boolean("update_at_exit", false);
            update_others =        ThcrapDll.globalconfig_get_boolean("update_others", true);
            console =              ThcrapDll.globalconfig_get_boolean("console", false);
            use_wininet =          ThcrapDll.globalconfig_get_boolean("use_wininet", false);
            exception_detail =     ThcrapDll.globalconfig_get_integer("exception_detail", 1);
            codepage =             ThcrapDll.globalconfig_get_integer("codepage", 932);
            developer_mode =       ThcrapDll.globalconfig_get_boolean("developer_mode", false);

            long _default_shortcut_destinations = ThcrapDll.globalconfig_get_integer("default_shortcut_destinations",
                (long)(ShortcutDestinations.Desktop | ShortcutDestinations.StartMenu));
            default_shortcut_destinations = (ShortcutDestinations)(_default_shortcut_destinations & configDestinationMask);
            shortcuts_type = (ThcrapDll.ShortcutsType)(_default_shortcut_destinations >> configTypeOffset);
            if (shortcuts_type != ThcrapDll.ShortcutsType.SHTYPE_AUTO &&
                shortcuts_type != ThcrapDll.ShortcutsType.SHTYPE_SHORTCUT &&
                shortcuts_type != ThcrapDll.ShortcutsType.SHTYPE_WRAPPER_ABSPATH &&
                shortcuts_type != ThcrapDll.ShortcutsType.SHTYPE_WRAPPER_RELPATH)
                shortcuts_type = ThcrapDll.ShortcutsType.SHTYPE_SHORTCUT;
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
            ThcrapDll.globalconfig_set_boolean("use_wininet", use_wininet);
            ThcrapDll.globalconfig_set_integer("exception_detail", exception_detail);
            ThcrapDll.globalconfig_set_integer("codepage", codepage);
            ThcrapDll.globalconfig_set_boolean("developer_mode", developer_mode);

            long _default_shortcut_destinations = (long)default_shortcut_destinations | ((long)shortcuts_type << configTypeOffset);
            ThcrapDll.globalconfig_set_integer("default_shortcut_destinations", _default_shortcut_destinations);
        }
    }
}
