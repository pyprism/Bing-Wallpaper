#!/usr/bin/env bash

version=$1
if [[ -z "$version" ]]; then
  echo "usage: $0 <version> [platform1 platform2 ...]"
  echo "  e.g. $0 1.0.0 linux/amd64 linux/arm64"
  exit 1
fi
package_name=bing-wallpaper
binary_name=$package_name

# Default platform list (all supported). Pass extra args to override.
default_platforms=(
  "darwin/arm64"
  "linux/amd64"
  "linux/arm"
  "linux/arm64"
  "windows/amd64"
)

if [[ $# -gt 1 ]]; then
  platforms=("${@:2}")
else
  platforms=("${default_platforms[@]}")
fi

rm -rf release/
mkdir -p release

for platform in "${platforms[@]}"
do
    platform_split=(${platform//\// })
    os=${platform_split[0]}
    GOOS=${platform_split[0]}
    GOARCH=${platform_split[1]}

    if [ $os = "darwin" ]; then
        os="macOS"
    fi

    output_binary=$binary_name
    if [ $os = "windows" ]; then
        output_binary+='.exe'
    fi

    archive_name=$package_name'-'$version'-'$os'-'$GOARCH

    echo "Building release/$output_binary for $os-$GOARCH..."

    env CGO_ENABLED=1 GOOS=$GOOS GOARCH=$GOARCH go build \
        -ldflags="-s -w -X main.version=$version" \
      -o release/$output_binary .
    if [ $? -ne 0 ]; then
        echo 'An error has occurred! Aborting the script execution...'
        exit 1
    fi

    pushd release > /dev/null
    if [ $os = "windows" ]; then
        if command -v zip &>/dev/null; then
            zip $archive_name.zip $output_binary
        else
            7z a $archive_name.zip $output_binary
        fi
        rm $output_binary
    else
        chmod a+x $output_binary
        gzip -c $output_binary > $archive_name.gz
        rm $output_binary
    fi
    popd > /dev/null
done