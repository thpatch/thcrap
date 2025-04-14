using LibGit2Sharp;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Windows;
using System.Windows.Controls;

namespace thcrap_configure_v3
{
    public partial class Page2_PatchEditor : UserControl
    {
        // Empty git class that provides no-op functions for all functions.
        // Use in place of a GitImpl object when operating on a folder without git initialized.
        private class Git : INotifyPropertyChanged
        {
            public event PropertyChangedEventHandler PropertyChanged;
            protected void OnPropertyChanged(string name = null)
            {
                PropertyChanged?.Invoke(this, new PropertyChangedEventArgs(name));
            }
            public class StatusResult
            {
                public List<string> filesAdded = new List<string>();
                public List<string> filesChanged = new List<string>();
                public List<string> filesRemoved = new List<string>();

                // Subset of filesAdded and filesChanged; files that need to be added to the index
                public List<string> filesToAdd = new List<string>();
                // Subset of filesRemoved; files that need to be removed from the index
                public List<string> filesToRemove = new List<string>();

                public bool hasError = false;
            }

            public static Git Open(string path)
            {
                return Directory.Exists(path + "/.git") ? new GitImpl(path) : new Git();
            }
            protected Git()
            {
                Status = new StatusResult();
            }
            public StatusResult Status { get; set; }
            public virtual bool IsValid => false;
            public Visibility VisibleIfNotInitialized => Status == null ? Visibility.Visible : Visibility.Collapsed;
            public Visibility VisibleIfEmpty => !IsValid && Status != null ? Visibility.Visible : Visibility.Collapsed;
            public Visibility VisibleIfStatusIsGood => IsValid && Status != null && !Status.hasError ? Visibility.Visible : Visibility.Collapsed;
            public Visibility VisibleIfStatusHasError => IsValid && Status != null && Status.hasError ? Visibility.Visible : Visibility.Collapsed;

            public virtual void RefreshStatus() { }
        }

        private class GitImpl : Git
        {
            private readonly Repository repo;

            public GitImpl(string path)
            {
                repo = new Repository(path);
                Status = null;
            }

            private StatusResult GetStatus()
            {
                StatusResult result = new StatusResult();

                foreach (var item in repo.RetrieveStatus(new StatusOptions()
                {
                    ExcludeSubmodules = true,
                    IncludeIgnored = false,
                })) {
                    switch (item.State)
                    {
                        case FileStatus.NewInIndex:
                            result.filesAdded.Add(item.FilePath);
                            break;
                        case FileStatus.NewInWorkdir:
                            result.filesAdded.Add(item.FilePath);
                            result.filesToAdd.Add(item.FilePath);
                            break;

                        case FileStatus.ModifiedInIndex:
                        case FileStatus.RenamedInIndex:
                        case FileStatus.TypeChangeInIndex:
                            result.filesChanged.Add(item.FilePath);
                            break;
                        case FileStatus.ModifiedInWorkdir:
                        case FileStatus.RenamedInWorkdir:
                        case FileStatus.TypeChangeInWorkdir:
                            result.filesChanged.Add(item.FilePath);
                            result.filesToAdd.Add(item.FilePath);
                            break;

                        case FileStatus.DeletedFromIndex:
                            result.filesRemoved.Add(item.FilePath);
                            break;
                        case FileStatus.DeletedFromWorkdir:
                            result.filesRemoved.Add(item.FilePath);
                            result.filesToRemove.Add(item.FilePath);
                            break;

                        case FileStatus.Conflicted:
                        case FileStatus.Unreadable:
                            result.hasError = true;
                            break;

                        case FileStatus.Ignored:
                        case FileStatus.Unaltered:
                        case FileStatus.Nonexistent:
                            // Can't happen
                            break;
                    }
                }
                return result;
            }
            public override void RefreshStatus()
            {
                Status = GetStatus();
                OnPropertyChanged(nameof(Status));
                OnPropertyChanged(nameof(VisibleIfNotInitialized));
                OnPropertyChanged(nameof(VisibleIfEmpty));
                OnPropertyChanged(nameof(VisibleIfStatusIsGood));
                OnPropertyChanged(nameof(VisibleIfStatusHasError));
            }
            public override bool IsValid => true;
        }
    }
}
