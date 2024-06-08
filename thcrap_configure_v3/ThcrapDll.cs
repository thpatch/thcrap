using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Threading.Tasks;

namespace thcrap_configure_v3
{
    class ThcrapHelper
    {
        public class UTF8StringMarshaler : ICustomMarshaler
        {
            public object MarshalNativeToManaged(IntPtr pNativeData)
            {
                return ThcrapHelper.PtrToStringUTF8(pNativeData);
            }

            public void CleanUpManagedData(object ManagedObj)
            { }

            public IntPtr MarshalManagedToNative(object ManagedObj)
            {
                return StringUTF8ToPtr(ManagedObj as string);
            }

            public void CleanUpNativeData(IntPtr pNativeData)
            {
                Marshal.FreeHGlobal(pNativeData);
            }

            public int GetNativeDataSize() => -1;

            public static ICustomMarshaler GetInstance(string pstrCookie) => new UTF8StringMarshaler();
        }

        private static bool LoadElem<T>(IntPtr ptr, int nullOffset, out T elem)
        {
            // Read field
            IntPtr field_pointer = Marshal.ReadIntPtr(ptr + nullOffset);
            if (field_pointer == IntPtr.Zero)
            {
                elem = default;
                return false;
            }

            elem = Marshal.PtrToStructure<T>(ptr);
            return true;
        }
        public static List<T> ParseNullTerminatedStructArray<T>(IntPtr ptr, int nullOffset = 0)
        {
            if (ptr == IntPtr.Zero)
                return new List<T>();

            var list = new List<T>();
            while (LoadElem(ptr, nullOffset, out T elem))
            {
                list.Add(elem);
                ptr += Marshal.SizeOf<T>();
            }
            return list;
        }
        public static string PtrToStringUTF8(IntPtr str_in)
        {
            if (str_in == IntPtr.Zero)
                return null;

            int len = 0;
            while (Marshal.ReadByte(str_in, len) != 0)
                len++;

            byte[] buffer = new byte[len];
            Marshal.Copy(str_in, buffer, 0, buffer.Length);

            return Encoding.UTF8.GetString(buffer);
        }
        public static IntPtr StringUTF8ToPtr(string str_in)
        {
            if (str_in == null)
                return IntPtr.Zero;

            byte[] bytes = Encoding.UTF8.GetBytes(str_in + '\0');

            IntPtr ptr = Marshal.AllocHGlobal(bytes.Length);
            Marshal.Copy(bytes, 0, ptr, bytes.Length);

            return ptr;
        }
        public static IntPtr AllocStructure<T>(T inObj)
        {
            IntPtr ptr = Marshal.AllocHGlobal(Marshal.SizeOf<T>());
            Marshal.StructureToPtr(inObj, ptr, false);
            return ptr;
        }
        public static void FreeStructure<T>(IntPtr ptr)
        {
            Marshal.DestroyStructure<T>(ptr);
            Marshal.FreeHGlobal(ptr);
        }
        public static T stack_json_resolve<T>(string fn)
        {
            IntPtr janssonObj = ThcrapDll.stack_json_resolve(fn, IntPtr.Zero);
            if (janssonObj == IntPtr.Zero)
                return default;

            uint bufferSize = ThcrapJanssonDll.json_dumpb(janssonObj, IntPtr.Zero, 0, 0);
            IntPtr bufferPtr = Marshal.AllocHGlobal((int)bufferSize);
            ThcrapJanssonDll.json_dumpb(janssonObj, bufferPtr, bufferSize, 0);
            ThcrapJanssonDll.json_delete(janssonObj);

            byte[] bufferArray = new byte[bufferSize];
            Marshal.Copy(bufferPtr, bufferArray, 0, (int)bufferSize);
            Marshal.FreeHGlobal(bufferPtr);

            string jsonStr = Encoding.UTF8.GetString(bufferArray);
            T obj = JsonSerializer.Deserialize<T>(jsonStr, new JsonSerializerOptions() { AllowTrailingCommas = true, ReadCommentHandling = JsonCommentHandling.Skip });

            return obj;
        }
    }
    public class ThcrapDll
    {
        public const string THCRAP_DLL_PATH = @".\";
#if DEBUG
        public const string DEBUG_OR_RELEASE = "_d";
#else
        public const string DEBUG_OR_RELEASE = "";
#endif
        public const string DLL = THCRAP_DLL_PATH + "thcrap" + DEBUG_OR_RELEASE + ".dll";

        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct repo_patch_t
        {
            public IntPtr patch_id;
            public IntPtr title;
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct repo_t
        {
            public IntPtr id;
            public IntPtr title;
            public IntPtr contact;
            public IntPtr servers;
            public IntPtr neighbors;
            public IntPtr patches;
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct patch_desc_t
        {
            public IntPtr repo_id;
            public IntPtr patch_id;
        };
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct patch_t
        {
            public IntPtr archive;
            UInt32 archive_length;
            public IntPtr id;
            UInt32 version;
            IntPtr title;
            IntPtr servers;
            public IntPtr dependencies;
            IntPtr fonts;
            IntPtr ignore;
            Byte update;
            UInt32 level;
            IntPtr config;
            IntPtr motd;
            IntPtr motd_title;
            UInt32 motd_type;
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct games_js_entry
        {
            public string id;
            public IntPtr path;
        };
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct game_search_result
        {
            public IntPtr path;
            public string id;
            public string build;
            // Description based on the version and the variety
            public IntPtr description;
        }

        public enum ShortcutsDestination
        {
            SHDESTINATION_THCRAP_DIR = 1,
            SHDESTINATION_DESKTOP = 2,
            SHDESTINATION_START_MENU = 3,
            SHDESTINATION_GAMES_DIRECTORY = 4,
        }

        public enum ShortcutsType
        {
            SHTYPE_SHORTCUT = 1,
            SHTYPE_WRAPPER_ABSPATH = 2,
            SHTYPE_WRAPPER_RELPATH = 3,
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void log_print_cb([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string text);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void log_nprint_cb(
            [MarshalAs(UnmanagedType.LPArray, ArraySubType = UnmanagedType.U1, SizeParamIndex = 1)]
            byte[] text,
            UInt32 size);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void stack_foreach_cb(IntPtr patch, IntPtr userdata);

        // global
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void thcrap_free(IntPtr ptr);

        // globalconfig
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void globalconfig_init();
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern bool globalconfig_get_boolean(string key, bool default_value);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int globalconfig_set_boolean(string key, bool value);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern long globalconfig_get_integer(string key, long default_value);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int globalconfig_set_integer(string key, long value);

        // runconfig
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void runconfig_thcrap_dir_set([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string thcrap_dir);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void exception_load_config();

        // log
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_init(bool console);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_set_hook(log_print_cb hookproc, log_nprint_cb hookproc2);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_print([MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string log);

        // repo
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RepoFree(IntPtr /* repo_t* */ repo);

        // patch
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern patch_t patch_init(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string patch_path,
            IntPtr /* json_t* */ patch_info, UInt32 level);

        // stack
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_add_patch(IntPtr /* patch_t* */ patch);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_free();
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_foreach(stack_foreach_cb callback, IntPtr userdata);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr /* json_t* */ stack_json_resolve(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string fn,
            IntPtr /* size_t * */ file_size);


        // search
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern IntPtr /* game_search_result* */ SearchForGames(string[] dir, games_js_entry[] games_in);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr /* game_search_result* */ SearchForGamesInstalled(games_js_entry[] games_in);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SearchForGames_cancel();
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SearchForGames_free(IntPtr /* game_search_result* */ games);


        // Shelllink
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int CreateShortcuts(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string run_cfg_fn,
            games_js_entry[] games, ShortcutsDestination destination, ShortcutsType type);

        // Update
        public enum get_status_t
        {
            GET_DOWNLOADING,
            GET_OK,
            GET_CLIENT_ERROR,
            GET_CRC32_ERROR,
            GET_SERVER_ERROR,
            GET_CANCELLED,
            GET_SYSTEM_ERROR,
        }
        public enum self_result_t
        {
            SELF_NO_UPDATE = -1,
            SELF_OK = 0,
            SELF_NO_PUBLIC_KEY,
            SELF_SERVER_ERROR,
            SELF_DISK_ERROR,
            SELF_NO_SIG,
            SELF_SIG_FAIL,
            SELF_REPLACE_ERROR,
            SELF_VERSION_CHECK_ERROR,
            SELF_INVALID_NETPATH,
            SELF_NO_TARGET_VERSION,
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct progress_callback_status_t
        {
            // Patch containing the file in download
            public IntPtr /* patch_t* */ patch;
            // File name
            public IntPtr /* char* */ fn;
            // Download URL
            public string url;
            // File download status or result
            public get_status_t status;
            // Human-readable error message if status is
            // GET_CLIENT_ERROR, GET_SERVER_ERROR or
            // GET_SYSTEM_ERROR, nullptr otherwise.
            public string error;

            // Bytes downloaded for the current file
            public UInt32 file_progress;
            // Size of the current file
            public UInt32 file_size;

            // Number of files downloaded in the current session
            public UInt32 nb_files_downloaded;
            // Number of files to download. Note that it will be 0 if
            // all the files.js haven't been downloaded yet.
            public UInt32 nb_files_total;
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate int update_filter_func_t(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string fn,
            IntPtr filter_data);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool progress_callback_t(IntPtr /* progress_callback_status_t* */ status, IntPtr param);

        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern self_result_t update_notify_thcrap_wrapper();
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr /* repo_t** */ RepoDiscover_wrapper(string start_url);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern ThcrapDll.patch_t patch_bootstrap_wrapper(IntPtr /* patch_desc_t* */ sel, IntPtr /* repo_t* */ repo);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_update_wrapper(update_filter_func_t filter_func, IntPtr filter_data, progress_callback_t progress_callback, IntPtr progress_param);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int update_filter_global_wrapper(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string fn,
            IntPtr unused);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int update_filter_games_wrapper(
            [MarshalAs(UnmanagedType.CustomMarshaler, MarshalTypeRef = typeof(ThcrapHelper.UTF8StringMarshaler))] string fn,
            IntPtr games);
    }

    class ThcrapJanssonDll
    {
        public const string DLL = ThcrapDll.THCRAP_DLL_PATH + "jansson" + ThcrapDll.DEBUG_OR_RELEASE + ".dll";
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern uint json_dumpb(IntPtr /* json_t * */ json, IntPtr /* char* */ buffer, uint size, uint flags);
        // Not part of the public jansson API, but json_decref is defined as an inline function
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void json_delete(IntPtr /* json_t * */ json);
    }
}
