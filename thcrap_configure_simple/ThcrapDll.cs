using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace thcrap_configure_simple
{
    class ThcrapHelper
    {
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
            int len = 0;
            while (Marshal.ReadByte(str_in, len) != 0)
                len++;

            byte[] buffer = new byte[len];
            Marshal.Copy(str_in, buffer, 0, buffer.Length);

            return Encoding.UTF8.GetString(buffer);
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
    }
    public class ThcrapDll
    {
        public const string THCRAP_DLL_PATH = @"..\..\..\bin\bin\";
#if DEBUG
        public const string DLL = THCRAP_DLL_PATH + "thcrap_d.dll";
#else
        public const string DLL = THCRAP_DLL_PATH + "thcrap.dll";
#endif

        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct repo_patch_t
        {
            public string patch_id;
            public string title;
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct repo_t
        {
            public string id;
            public string title;
            public string contact;
            public IntPtr servers;
            public IntPtr neighbors;
            public IntPtr patches;
        }
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct patch_desc_t
        {
            public string repo_id;
            public string patch_id;
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
            public string path;
        };
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct game_search_result
        {
            public IntPtr path;
            public string id;
            public string build;
            // Description based on the version and the variety
            public string description;
        }

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate void log_print_cb(string text);
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

        // log
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_init(int console);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_set_hook(log_print_cb hookproc, log_nprint_cb hookproc2);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void log_print(string log);

        // repo
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void RepoFree(IntPtr /* repo_t* */ repo);

        // patch
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern patch_t patch_init(string patch_path, IntPtr /* json_t* */ patch_info, UInt32 level);

        // stack
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_add_patch(IntPtr /* patch_t* */ patch);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_free();
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_foreach(stack_foreach_cb callback, IntPtr userdata);

        // search
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
        public static extern IntPtr /* game_search_result* */ SearchForGames(string dir, games_js_entry[] games_in);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void SearchForGames_free(IntPtr /* game_search_result* */ games);

    }
    class ThcrapUpdateDll
    {
#if DEBUG
        public const string DLL = ThcrapDll.THCRAP_DLL_PATH + "thcrap_update_d.dll";
#else
        public const string DLL = ThcrapDll.THCRAP_DLL_PATH + "thcrap_update.dll";
#endif
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
        [StructLayout(LayoutKind.Sequential, Pack = 4), Serializable]
        public struct progress_callback_status_t
        {
            // Patch containing the file in download
            public IntPtr /* patch_t* */ patch;
            // File name
            public string fn;
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
        public delegate int update_filter_func_t(string fn, IntPtr filter_data);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool progress_callback_t(IntPtr /* progress_callback_status_t* */ status, IntPtr param);


        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr /* repo_t** */ RepoDiscover(string start_url);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern ThcrapDll.patch_t patch_bootstrap(IntPtr /* patch_desc_t* */ sel, IntPtr /* repo_t* */ repo);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern void stack_update(update_filter_func_t filter_func, IntPtr filter_data, progress_callback_t progress_callback, IntPtr progress_param);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int update_filter_global(string fn, IntPtr unused);
        [DllImport(DLL, CallingConvention = CallingConvention.Cdecl)]
        public static extern int update_filter_games(string fn, IntPtr games);
    }
}
