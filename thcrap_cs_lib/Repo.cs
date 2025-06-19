using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace thcrap_cs_lib
{
    public class RepoPatch
    {
        public Repo Repo { get; set; }
        public string Id { get; set; }
        public string Title { get; set; }
        // Initialized when calling AddToStack
        public string Archive { get; set; }

        public RepoPatch(Repo repo, ThcrapDll.repo_patch_t patch)
        {
            this.Repo = repo;
            this.Id = ThcrapHelper.PtrToStringUTF8(patch.patch_id);
            this.Title = ThcrapHelper.PtrToStringUTF8(patch.title);
        }
        private RepoPatch FindDependency(List<Repo> repoList, ThcrapDll.patch_desc_t dep)
        {
            string repo_id = ThcrapHelper.PtrToStringUTF8(dep.repo_id);
            string patch_id = ThcrapHelper.PtrToStringUTF8(dep.patch_id);

            // If we know the repo name, use it
            if (repo_id != null)
                return repoList.Find((Repo it) => it.Id == repo_id)?.Patches.Find((RepoPatch it) => it.Id == patch_id);

            // Try in the current repo
            RepoPatch patch = this.Repo.Patches.Find((RepoPatch it) => it.Id == patch_id);
            if (patch != null)
                return patch;

            // Fall back to looking in every repo
            foreach (var repo in repoList)
            {
                patch = repo.Patches.Find((RepoPatch it) => it.Id == patch_id);
                if (patch != null)
                    return patch;
            }

            // Didn't find anything
            return null;
        }
        private void LoadDependencies(List<Repo> repoList, HashSet<RepoPatch> knownPatches, IntPtr dependenciesPtr)
        {
            if (dependenciesPtr == IntPtr.Zero)
                return;

            var deps = ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.patch_desc_t>(dependenciesPtr, 4);
            foreach (var dep in deps)
            {
                RepoPatch patch = FindDependency(repoList, dep);
                if (patch != null)
                    patch.AddToStack(repoList, knownPatches);
            }
        }
        public void AddToStack(List<Repo> repoList, HashSet<RepoPatch> knownPatches)
        {
            if (knownPatches.Contains(this))
                return;
            knownPatches.Add(this);

            ThcrapDll.patch_desc_t patch_desc;
            patch_desc.repo_id = ThcrapHelper.StringUTF8ToPtr(this.Repo.Id);
            patch_desc.patch_id = ThcrapHelper.StringUTF8ToPtr(this.Id);

            IntPtr patch_desc_ptr = ThcrapHelper.AllocStructure(patch_desc);
            ThcrapDll.patch_t patch_info = ThcrapDll.patch_bootstrap_wrapper(patch_desc_ptr, this.Repo.repo_ptr);

            this.Archive = ThcrapHelper.PtrToStringUTF8(patch_info.archive);
            ThcrapDll.patch_t patch_full = ThcrapDll.patch_init(this.Archive, IntPtr.Zero, 0);

            this.LoadDependencies(repoList, knownPatches, patch_full.dependencies);

            IntPtr patch_full_ptr = ThcrapHelper.AllocStructure(patch_full);
            ThcrapDll.stack_add_patch(patch_full_ptr);

            Marshal.FreeHGlobal(patch_desc.patch_id);
            Marshal.FreeHGlobal(patch_desc.repo_id);
            ThcrapHelper.FreeStructure<ThcrapDll.patch_desc_t>(patch_desc_ptr);
            ThcrapHelper.FreeStructure<ThcrapDll.patch_t>(patch_full_ptr);
        }
    }

    public class Repo
    {
        public IntPtr repo_ptr;
        public string Id { get; set; }
        public string Title { get; set; }
        public List<RepoPatch> Patches { get; set; }
        public static List<Repo> Discovery(string start_url)
        {
            IntPtr repo_list = ThcrapDll.RepoDiscover_wrapper(start_url);
            if (repo_list == IntPtr.Zero)
                return new List<Repo>();

            var out_list = new List<Repo>();
            int current_repo = 0;
            IntPtr repo_ptr = Marshal.ReadIntPtr(repo_list, current_repo * IntPtr.Size);
            while (repo_ptr != IntPtr.Zero)
            {
                out_list.Add(new Repo(repo_ptr));
                current_repo++;
                repo_ptr = Marshal.ReadIntPtr(repo_list, current_repo * IntPtr.Size);
            }

            ThcrapDll.thcrap_free(repo_list);
            return out_list;
        }

        private Repo(IntPtr repo_ptr)
        {
            ThcrapDll.repo_t repo = Marshal.PtrToStructure<ThcrapDll.repo_t>(repo_ptr);

            Id = ThcrapHelper.PtrToStringUTF8(repo.id);
            Title = ThcrapHelper.PtrToStringUTF8(repo.title);
            this.repo_ptr = repo_ptr;

            var patchesList = ThcrapHelper.ParseNullTerminatedStructArray<ThcrapDll.repo_patch_t>(repo.patches);
            Patches = new List<RepoPatch>();
            foreach (var it in patchesList)
                Patches.Add(new RepoPatch(this, it));
        }
        ~Repo()
        {
            ThcrapDll.RepoFree(repo_ptr);
        }
    }
}
