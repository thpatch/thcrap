#!/bin/bash

# Have to be in the same order as the output of `ls *.exe bin/*.exe bin/*.dll bin/*.json`
FILES_LIST="bin/act_nut_lib.dll \
    bin/bmpfont_create_gdi.dll \
    bin/bmpfont_create_gdiplus.dll \
    bin/cacert.pem \
    bin/fribidi.dll \
    bin/jansson.dll \
    bin/libcrypto-1_1.dll \
    bin/libcurl.dll \
    bin/libpng16.dll \
    bin/libssl-1_1.dll \
    bin/Microsoft.Bcl.AsyncInterfaces.dll \
    bin/Microsoft.WindowsAPICodePack.dll \
    bin/Microsoft.WindowsAPICodePack.Shell.dll \
    bin/Microsoft.WindowsAPICodePack.ShellExtensions.dll \
    bin/Microsoft.Xaml.Behaviors.dll \
    bin/scripts/install_dotnet480.sh \
    bin/scripts/setup_mirror.bat \
    bin/steam_api.dll \
    bin/System.Buffers.dll \
    bin/System.Memory.dll \
    bin/System.Numerics.Vectors.dll \
    bin/System.Runtime.CompilerServices.Unsafe.dll \
    bin/System.Text.Encodings.Web.dll \
    bin/System.Text.Json.dll \
    bin/System.Threading.Tasks.Extensions.dll \
    bin/System.ValueTuple.dll \
    bin/thcrap_configure.exe \
    bin/thcrap_configure_v3.exe \
    bin/thcrap_configure_v3.exe.config \
    bin/thcrap.dll \
    bin/thcrap_i18n.dll \
    bin/thcrap_loader.exe \
    bin/thcrap_tasofro.dll \
    bin/thcrap_test.exe \
    bin/thcrap_tsa.dll \
    bin/thcrap_update.dll \
    bin/update.json \
    bin/vc_redist.x86.exe \
    bin/win32_utf8.dll \
    bin/Xceed.Wpf.AvalonDock.dll \
    bin/Xceed.Wpf.AvalonDock.Themes.Aero.dll \
    bin/Xceed.Wpf.AvalonDock.Themes.Metro.dll \
    bin/Xceed.Wpf.AvalonDock.Themes.VS2010.dll \
    bin/Xceed.Wpf.Toolkit.dll \
    bin/zlib-ng.dll \
    thcrap.exe \
    thcrap_loader.exe"
FILES_LIST=$(echo "$FILES_LIST" | tr -s ' ')

# Arguments. Every argument without a default value is mandatory.
DATE="$(date)"
MSBUILD_PATH=
MSBUILD_USER="$USER"
GITHUB_LOGIN=
# Authentication with Github tokens: https://developer.github.com/v3/auth/#basic-authentication
GITHUB_TOKEN=
BETA=0

function parse_input
{
    while [ $# -gt 0 ]; do
        case "$1" in
            --help )
                echo "Usage: $0 arguments"
                echo '  --help:          Display this message'
                echo '  --date:          Release date, in the format YYYY-MM-DD (or any other date supported by `date -d`).'
                echo '                   Optional, defaults to today'
                echo '  --msbuild-path:  Path to msbuild.exe'
                echo '  --msbuild-user:  Name of the user building this release (displayed in the thcrap logs).'
                echo '                   Optional, defaults to $USER.'
                echo '  --github-login:  Username for Github'
                echo '  --github-token:  Token for Github'
                echo '                   To get a token, see https://help.github.com/en/github/authenticating-to-github/creating-a-personal-access-token-for-the-command-line'
                echo '  --beta:          Publish the build as a beta build'
                exit 0
                ;;
            --date )
                DATE=$2
                shift 2
                ;;
            --msbuild-path )
                MSBUILD_PATH=$2
                shift 2
                ;;
            --msbuild-user )
                MSBUILD_USER=$2
                shift 2
                ;;
            --github-login )
                GITHUB_LOGIN=$2
                shift 2
                ;;
            --github-token )
                GITHUB_TOKEN=$2
                shift 2
                ;;
            --beta )
                BETA=1
                shift 1
                ;;
            * )
                echo "Unknown argument $1"
                exit 1
        esac
    done
    
    if [ -z "$DATE" ];          then echo "--date is required"          ; exit 1; fi
    if [ -z "$MSBUILD_PATH" ];  then echo "--msbuild-path is required"  ; exit 1; fi
    if [ -z "$MSBUILD_USER" ];  then echo "--msbuild-user is required"  ; exit 1; fi
    if [ -z "$GITHUB_LOGIN" ];  then echo "--github-login is required"  ; exit 1; fi
    if [ -z "$GITHUB_TOKEN" ];  then echo "--github-token is required"  ; exit 1; fi
}

function confirm
{
    read -p "$1 [y/N] " -n 1 -r
    echo    # (optional) move to a new line
    if [[ ! $REPLY =~ ^[Yy]$ ]]
    then
    [[ "$0" = "$BASH_SOURCE" ]] && exit 1 || return 1 # handle exits from shell or function but don't exit interactive shell
    fi
}

# Fill the argument constants at the top of this file
parse_input "$@"

# Pull changes
cd git_thcrap
git pull
git submodule update --init --recursive
echo "Cleaning previous build artifacts..."
git clean -qxf -e cert.pfx
git status
confirm "Continue?"
cd ..

# build
cd git_thcrap
"$MSBUILD_PATH" -t:restore /verbosity:minimal
"$MSBUILD_PATH" /target:Rebuild /property:USERNAME=$MSBUILD_USER /property:Configuration=Release /verbosity:minimal \
    /property:ThcrapVersionY=$(date -d "$DATE" +%Y) /property:ThcrapVersionM=$(date -d "$DATE" +%m) /property:ThcrapVersionD=$(date -d "$DATE" +%d) \
    | grep -e warning -e error \
    | grep -v -e 'Number of'
cd ..

# Run unit tests
rm -rf "tmp_$DATE"
mkdir "tmp_$DATE"
cd "tmp_$DATE"
../git_thcrap/bin/bin/thcrap_test.exe
UNITTEST_STATUS=$?
cd ..
rm -rf "tmp_$DATE"
test $UNITTEST_STATUS -eq 0 || confirm 'Unit tests failed! Continue anyway?'

# Prepare the release directory
BUILD_FILES_LIST=$(cd git_thcrap/bin && echo $(ls \
    *.exe \
    bin/cacert.pem \
    bin/*.exe \
    bin/*.exe.config \
    bin/*.dll \
    bin/*.json \
    bin/scripts/* \
    | grep -vF '_d.dll'))
if [ "$BUILD_FILES_LIST" != "$FILES_LIST" ]; then
    echo "The list of files to copy doesn't match. Files list:"
    echo "$FILES_LIST" | tr ' ' '\n' > 1
    echo "$BUILD_FILES_LIST" | tr ' ' '\n' > 2
    diff --color 1 2
    rm 1 2
    confirm "Continue anyway?"
fi
rm -rf thcrap # Using -f for readonly files
mkdir thcrap
mkdir thcrap/bin
# Copy all the build files
for f in $FILES_LIST; do
    DESTDIR=$(dirname thcrap/$f)
    test -d "$DESTDIR" || mkdir -p "$DESTDIR"
    cp git_thcrap/bin/$f thcrap/$f
done
cp -r git_thcrap/scripts/ thcrap/bin/
rm -rf thcrap/bin/scripts/__pycache__/
# Add an initial repo.js, used by configure as a server list
mkdir -p thcrap/repos/thpatch
cd thcrap/repos/thpatch
wget http://srv.thpatch.net/repo.js
cd ../../..

# Copy the release to the test directory
mkdir -p thcrap_test # don't fail if it exists
rm -r thcrap_test/bin/ 2>/dev/null # Make sure to not keep leftover binaries from a previous build
cp -r thcrap/* thcrap_test/

# Build the upgrade test version
cd git_thcrap
"$MSBUILD_PATH" /target:Build /property:USERNAME=$MSBUILD_USER /property:Configuration=Release /verbosity:minimal \
    /property:ThcrapVersionY=0001 /property:ThcrapVersionM=01 /property:ThcrapVersionD=01 \
    | grep -e warning -e error \
    | grep -v -e 'Number of'
cd ..

# Copy the upgrade test version to its test directory
mkdir -p thcrap_test_upgrade # don't fail if it exists
rm -r thcrap_test_upgrade/bin/ 2>/dev/null # Make sure to not keep leftover binaries from a previous build
cp -r thcrap/* thcrap_test_upgrade/
# TODO: copy only the files needed
cp -r git_thcrap/bin/* thcrap_test_upgrade/

cd thcrap_test
explorer.exe .
cd ..
confirm "Time to test the release! Press y if everything works fine."

cd thcrap_test_upgrade
explorer.exe .
cd ..
confirm "Test the upgrade, and press y if everything works fine."


# Create the zip and its signature
rm -f thcrap.zip
cd thcrap
7z a ../thcrap.zip *
cd ..
python3 ./git_thcrap/scripts/release_sign.py -k cert.pem thcrap.zip

if [ thcrap.zip -nt thcrap.zip.sig ]; then
    echo "Error: thcrap.zip is more recent than thcrap.zip.sig"
    ls -l thcrap.zip thcrap.zip.sig
    confirm "Continue anyway? (not recommended)"
fi

rm -f thcrap_symbols.zip
cd git_thcrap/bin
7z a ../../thcrap_symbols.zip *
cd ../..

# Create the commits history
commits="$(git -C git_thcrap log --reverse --format=oneline $(git -C git_thcrap tag | tail -1)^{commit}..HEAD)"

cat > commit_github.txt <<EOF
Add a release comment if you want to.

##### \`module_name\`
$(echo "$commits" | sed -e 's/^\([0-9a-fA-F]\+\) \(.*\)/- \2 (\1)/')
EOF
unix2dos commit_github.txt
notepad.exe commit_github.txt &
confirm "Did you remove non-visible commits and include non-thcrap commits?"

echo "Uploading the release on github..."
if [ "$BETA" == 1 ]; then
    GITHUB_PRERELEASE=true
else
    GITHUB_PRERELEASE=false
fi
GIT_TAG=$(date -d "$DATE" +%Y-%m-%d)
upload_url=$(\
    jq -n \
        --arg msg "$(cat commit_github.txt)" \
        --arg date "$GIT_TAG" \
        --argjson prerelease $GITHUB_PRERELEASE \
        '{ "tag_name": $date, "name": $date, "body": $msg, "prerelease": $prerelease }' | \
    curl -s 'https://api.github.com/repos/thpatch/thcrap/releases' \
        -Lu "$GITHUB_LOGIN:$GITHUB_TOKEN" -H 'Content-Type: application/json' -d@- | \
    jq -r .upload_url | sed -e 's/{.*}//')
if [ "$upload_url" == "null" ]; then echo "Releasing on GitHub failed."; fi

function upload_files_to_github()
{
	ret=$(curl -s "$upload_url?name=thcrap.zip" -Lu "$GITHUB_LOGIN:$GITHUB_TOKEN" -H 'Content-Type: application/zip' --data-binary @thcrap.zip | jq -r '.state')
	if [ "$ret" != "uploaded" ]; then echo "thcrap.zip upload on GitHub failed."; fi
	ret=$(curl -s "$upload_url?name=thcrap.zip.sig" -Lu "$GITHUB_LOGIN:$GITHUB_TOKEN" -H 'Content-Type: application/octet-stream' --data-binary @thcrap.zip.sig | jq -r '.state')
	if [ "$ret" != "uploaded" ]; then echo "thcrap.zip.sig upload on GitHub failed."; fi
	ret=$(curl -s "$upload_url?name=thcrap_symbols.zip" -Lu "$GITHUB_LOGIN:$GITHUB_TOKEN" -H 'Content-Type: application/zip' --data-binary @thcrap_symbols.zip | jq -r '.state')
	if [ "$ret" != "uploaded" ]; then echo "thcrap_symbols.zip upload on GitHub failed."; fi
}
upload_files_to_github

function test_github_upload()
{
	echo "Testing if the release was uploaded properly..."
	wget "https://github.com/thpatch/thcrap/releases/download/$GIT_TAG/thcrap.zip" -O thcrap_github.zip || return 1
	wget "https://github.com/thpatch/thcrap/releases/download/$GIT_TAG/thcrap.zip.sig" -O thcrap_github.zip.sig || return 1
	diff -q thcrap.zip thcrap_github.zip || return 1
	diff -q thcrap.zip.sig thcrap_github.zip.sig || return 1
	rm thcrap_github.zip thcrap_github.zip.sig
	return 0
}
if ! test_github_upload; then
	# The 2nd upload_files_to_github call freezes.
	# echo "WARNING: upload failed. Trying again..."
	# upload_files_to_github
	# if ! test_github_upload; then
		explorer.exe .
		confirm "ERROR: Github upload failed twice. Please upload the release manually and then press y."
	# fi
fi

if [ "$BETA" != 1 ]; then
    # Push update
    scp root@kosuzu.thpatch.net:/var/www/thcrap_update.js .
    jq --arg version "0x$(date -d "$DATE" +%Y%m%d)" --arg zip_fn "stable/thcrap.zip" '.stable.version = $version | .stable.latest = $zip_fn' thcrap_update.js > tmp.js
    mv tmp.js thcrap_update.js
    scp thcrap_update.js root@kosuzu.thpatch.net:/var/www/thcrap_update.js
fi

echo "Releasing finished!"
